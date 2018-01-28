/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2015,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "forwarder.hpp"
#include "core/logger.hpp"
#include "core/random.hpp"
#include "strategy.hpp"

#ifdef NDNSIM
#include "face/null-face.hpp"
#include "utils/ndn-ns3-packet-tag.hpp"
#endif // NDNSIM

#include <boost/random/uniform_int_distribution.hpp>

namespace nfd {

NFD_LOG_INIT("Forwarder");

using fw::Strategy;

const Name Forwarder::LOCALHOST_NAME("ndn:/localhost");

Forwarder::Forwarder()
  : m_faceTable(*this)
  , m_fib(m_nameTree)
  , m_pit(m_nameTree)
  , m_measurements(m_nameTree)
  , m_strategyChoice(m_nameTree, fw::makeDefaultStrategy(*this))
#ifdef NDNSIM
  , m_csFace(make_shared<NullFace>(FaceUri("contentstore://")))
#endif // NDNSIM
{
  fw::installStrategies(*this);
#ifdef NDNSIM
  getFaceTable().addReserved(m_csFace, FACEID_CONTENT_STORE);
#endif // NDNSIM
}

Forwarder::~Forwarder()
{

}

#ifndef NDNSIM
void
Forwarder::startProcessInterest(Face& face, const Interest& interest)
{
  // check fields used by forwarding are well-formed
  try {
    if (interest.hasLink()) {
      interest.getLink();
    }
  }
  catch (const tlv::Error&) {
    NFD_LOG_DEBUG("startProcessInterest face=" << face.getId() <<
                  " interest=" << interest.getName() << " malformed");
    // It's safe to call interest.getName() because Name has been fully parsed
    return;
  }

  this->onIncomingInterest(face, interest);
}

void
Forwarder::startProcessData(Face& face, const Data& data)
{
  // check fields used by forwarding are well-formed
  // (none needed)

  this->onIncomingData(face, data);
}

void
Forwarder::startProcessNack(Face& face, const lp::Nack& nack)
{
  // check fields used by forwarding are well-formed
  try {
    if (nack.getInterest().hasLink()) {
      nack.getInterest().getLink();
    }
  }
  catch (const tlv::Error&) {
    NFD_LOG_DEBUG("startProcessNack face=" << face.getId() <<
                  " nack=" << nack.getInterest().getName() <<
                  "~" << nack.getReason() << " malformed");
    return;
  }

  this->onIncomingNack(face, nack);
}
#endif // !NDNSIM

void
Forwarder::onIncomingInterest(Face& inFace, const Interest& interest)
{
  // receive Interest
  NFD_LOG_DEBUG("onIncomingInterest face=" << inFace.getId() <<
                " interest=" << interest.getName());
  const_cast<Interest&>(interest).setIncomingFaceId(inFace.getId());
  ++m_counters.getNInInterests();
  
  // /localhost scope control
  bool isViolatingLocalhost = !inFace.isLocal() &&
                              LOCALHOST_NAME.isPrefixOf(interest.getName());
  if (isViolatingLocalhost) {
    NFD_LOG_DEBUG("onIncomingInterest face=" << inFace.getId() <<
                  " interest=" << interest.getName() << " violates /localhost");
    // (drop)
    return;
  }
 

  // PIT insert
  shared_ptr<pit::Entry> pitEntry = m_pit.insert(interest).first;

  // detect duplicate Nonce
  int dnw = pitEntry->findNonce(interest.getNonce(), inFace);
  bool hasDuplicateNonce = (dnw != pit::DUPLICATE_NONCE_NONE) ||
                           m_deadNonceList.has(interest.getName(), interest.getNonce());
  if (hasDuplicateNonce) {
    // goto Interest loop pipeline
    this->onInterestLoop(inFace, interest, pitEntry);
    return;
  }
  
  // cancel unsatisfy & straggler timer
  this->cancelUnsatisfyAndStragglerTimer(pitEntry);

  // is pending?
  const pit::InRecordCollection& inRecords = pitEntry->getInRecords();
  bool isPending = inRecords.begin() != inRecords.end();
  if (!isPending) {
#ifdef NDNSIM
    if (m_csFromNdnSim == nullptr) {
#endif // NDNSIM
      m_cs.find(interest,
                bind(&Forwarder::onContentStoreHit, this, ref(inFace), pitEntry, _1, _2),
                bind(&Forwarder::onContentStoreMiss, this, ref(inFace), pitEntry, _1));
#ifdef NDNSIM
    }
    else {
      shared_ptr<Data> match = m_csFromNdnSim->Lookup(interest.shared_from_this());
      if (match != nullptr) {
        this->onContentStoreHit(inFace, pitEntry, interest, *match);
      }
      else {
        this->onContentStoreMiss(inFace, pitEntry, interest);
      }
    }
#endif // NDNSIM
  }
  else {
    this->onContentStoreMiss(inFace, pitEntry, interest);
  }
}

void
Forwarder::onContentStoreMiss(const Face& inFace,
                              shared_ptr<pit::Entry> pitEntry,
                              const Interest& interest)
{
  NFD_LOG_DEBUG("onContentStoreMiss interest=" << interest.getName());
  
  shared_ptr<Face> face = const_pointer_cast<Face>(inFace.shared_from_this());
  // insert InRecord
  pitEntry->insertOrUpdateInRecord(face, interest);

  // set PIT unsatisfy timer
  this->setUnsatisfyTimer(pitEntry);
  
#ifdef NDNSIM
  // FIB lookup
  shared_ptr<fib::Entry> fibEntry = m_fib.findLongestPrefixMatch(*pitEntry);
#else
  shared_ptr<fib::Entry> fibEntry;
  // has Link object?
  if (!interest.hasLink()) {
    // FIB lookup with Interest name
    fibEntry = m_fib.findLongestPrefixMatch(*pitEntry);
    NFD_LOG_TRACE("onContentStoreMiss noLinkObject");
  }
  else {
    const Link& link = interest.getLink();

    // in producer region?
    if (m_networkRegionTable.isInProducerRegion(link)) {
      // FIB lookup with Interest name
      fibEntry = m_fib.findLongestPrefixMatch(*pitEntry);
      NFD_LOG_TRACE("onContentStoreMiss inProducerRegion");
    }
    // has SelectedDelegation?
    else if (interest.hasSelectedDelegation()) {
      // FIB lookup with SelectedDelegation
      fibEntry = m_fib.findLongestPrefixMatch(interest.getSelectedDelegation());
      NFD_LOG_TRACE("onContentStoreMiss hasSelectedDelegation=" << interest.getSelectedDelegation());
    }
    else {
      // FIB lookup with first delegation Name
      fibEntry = m_fib.findLongestPrefixMatch(link.getDelegations().begin()->second);

      // in default-free zone?
      bool isDefaultFreeZone = !(fibEntry->getPrefix().size() == 0 && fibEntry->hasNextHops());
      if (isDefaultFreeZone) {
        // choose and set SelectedDelegation
        for (const std::pair<uint32_t, Name>& delegation : link.getDelegations()) {
          const Name& delegationName = delegation.second;
          fibEntry = m_fib.findLongestPrefixMatch(delegationName);
          if (fibEntry->hasNextHops()) {
            const_cast<Interest&>(interest).setSelectedDelegation(delegationName);
            NFD_LOG_TRACE("onContentStoreMiss enterDefaultFreeZone"
                          << " setSelectedDelegation=" << delegationName);
            break;
          }
        }
      }
      else {
        NFD_LOG_TRACE("onContentStoreMiss inConsumerRegion");
      }
    }
  }

  // dispatch to strategy
  BOOST_ASSERT(fibEntry != nullptr);
#endif // NDNSIM
  
  // dispatch to strategy
  this->dispatchToStrategy(pitEntry, bind(&Strategy::afterReceiveInterest, _1,
                                          cref(inFace), cref(interest), fibEntry, pitEntry));
}

void
Forwarder::onContentStoreHit(const Face& inFace,
                             shared_ptr<pit::Entry> pitEntry,
                             const Interest& interest,
                             const Data& data)
{
  NFD_LOG_DEBUG("onContentStoreHit interest=" << interest.getName());

#ifdef NDNSIM
  beforeSatisfyInterest(*pitEntry, *m_csFace, data);
  this->dispatchToStrategy(pitEntry, bind(&Strategy::beforeSatisfyInterest, _1,
                                          pitEntry, cref(*m_csFace), cref(data)));
#endif // NDNSIM

  const_pointer_cast<Data>(data.shared_from_this())->setIncomingFaceId(FACEID_CONTENT_STORE);
  // XXX should we lookup PIT for other Interests that also match csMatch?

  // set PIT straggler timer
  this->setStragglerTimer(pitEntry, true, data.getFreshnessPeriod());

#ifdef PATH_LABELLING
  // Reset Path ID before sending out from forwarder cache
  if (!data.isPathIdEmpty()) {
    // std::cout<<"data replied from cs, new path ID would be reset to "<<'0'<<std::endl;
    const_pointer_cast<Data>(data.shared_from_this())->setPathIdAndWire(0);
  }
#endif // PATH_LABELLING


  // goto outgoing Data pipeline
  this->onOutgoingData(data, *const_pointer_cast<Face>(inFace.shared_from_this()));
}

void
Forwarder::onInterestLoop(Face& inFace, const Interest& interest,
                          shared_ptr<pit::Entry> pitEntry)
{
#ifdef NDNSIM
  NFD_LOG_DEBUG("onInterestLoop face=" << inFace.getId() <<
                " interest=" << interest.getName());

  // (drop)
#else
  // if multi-access face, drop
  if (inFace.isMultiAccess()) {
    NFD_LOG_DEBUG("onInterestLoop face=" << inFace.getId() <<
                  " interest=" << interest.getName() <<
                  " drop");
    return;
  }

  NFD_LOG_DEBUG("onInterestLoop face=" << inFace.getId() <<
                " interest=" << interest.getName() <<
                " send-Nack-duplicate");

  // send Nack with reason=DUPLICATE
  // note: Don't enter outgoing Nack pipeline because it needs an in-record.
  lp::Nack nack(interest);
  nack.setReason(lp::NackReason::DUPLICATE);
  inFace.sendNack(nack);
#endif
}

/** \brief compare two InRecords for picking outgoing Interest
 *  \return true if b is preferred over a
 *
 *  This function should be passed to std::max_element over InRecordCollection.
 *  The outgoing Interest picked is the last incoming Interest
 *  that does not come from outFace.
 *  If all InRecords come from outFace, it's fine to pick that. This happens when
 *  there's only one InRecord that comes from outFace. The legit use is for
 *  vehicular network; otherwise, strategy shouldn't send to the sole inFace.
 */
static inline bool
compare_pickInterest(const pit::InRecord& a, const pit::InRecord& b, const Face* outFace)
{
  bool isOutFaceA = a.getFace().get() == outFace;
  bool isOutFaceB = b.getFace().get() == outFace;

  if (!isOutFaceA && isOutFaceB) {
    return false;
  }
  if (isOutFaceA && !isOutFaceB) {
    return true;
  }

  return a.getLastRenewed() > b.getLastRenewed();
}

void
Forwarder::onOutgoingInterest(shared_ptr<pit::Entry> pitEntry, Face& outFace,
                              bool wantNewNonce)
{  
  if (outFace.getId() == INVALID_FACEID) {
    NFD_LOG_WARN("onOutgoingInterest face=invalid interest=" << pitEntry->getName());
    return;
  }
  NFD_LOG_DEBUG("onOutgoingInterest face=" << outFace.getId() <<
                " interest=" << pitEntry->getName());

  // scope control
  if (pitEntry->violatesScope(outFace)) {
    NFD_LOG_DEBUG("onOutgoingInterest face=" << outFace.getId() <<
                  " interest=" << pitEntry->getName() << " violates scope");
    return;
  }
  
  // pick Interest
  const pit::InRecordCollection& inRecords = pitEntry->getInRecords();
  pit::InRecordCollection::const_iterator pickedInRecord = std::max_element(
    inRecords.begin(), inRecords.end(), bind(&compare_pickInterest, _1, _2, &outFace));
  BOOST_ASSERT(pickedInRecord != inRecords.end());
  shared_ptr<Interest> interest = const_pointer_cast<Interest>(
    pickedInRecord->getInterest().shared_from_this());

  
  
  if (wantNewNonce) {
    interest = make_shared<Interest>(*interest);
    static boost::random::uniform_int_distribution<uint32_t> dist;
    interest->setNonce(dist(getGlobalRng()));
  }

  // insert OutRecord
  pitEntry->insertOrUpdateOutRecord(outFace.shared_from_this(), *interest);

  // send Interest
  outFace.sendInterest(*interest);
  ++m_counters.getNOutInterests();
}

void
Forwarder::onInterestReject(shared_ptr<pit::Entry> pitEntry)
{
  if (pitEntry->hasUnexpiredOutRecords()) {
    NFD_LOG_ERROR("onInterestReject interest=" << pitEntry->getName() <<
                  " cannot reject forwarded Interest");
    return;
  }
  NFD_LOG_DEBUG("onInterestReject interest=" << pitEntry->getName());

  // cancel unsatisfy & straggler timer
  this->cancelUnsatisfyAndStragglerTimer(pitEntry);

  // set PIT straggler timer
  this->setStragglerTimer(pitEntry, false);
}

void
Forwarder::onInterestUnsatisfied(shared_ptr<pit::Entry> pitEntry)
{
  NFD_LOG_DEBUG("onInterestUnsatisfied interest=" << pitEntry->getName());

  // invoke PIT unsatisfied callback
#ifdef NDNSIM
  beforeExpirePendingInterest(*pitEntry);
#endif // NDNSIM
  this->dispatchToStrategy(pitEntry, bind(&Strategy::beforeExpirePendingInterest, _1,
                                          pitEntry));

  // goto Interest Finalize pipeline
  this->onInterestFinalize(pitEntry, false);
}

void
Forwarder::onInterestFinalize(shared_ptr<pit::Entry> pitEntry, bool isSatisfied,
                              const time::milliseconds& dataFreshnessPeriod)
{
  NFD_LOG_DEBUG("onInterestFinalize interest=" << pitEntry->getName() <<
                (isSatisfied ? " satisfied" : " unsatisfied"));

  // Dead Nonce List insert if necessary
  this->insertDeadNonceList(*pitEntry, isSatisfied, dataFreshnessPeriod, 0);

  // PIT delete
  this->cancelUnsatisfyAndStragglerTimer(pitEntry);
  m_pit.erase(pitEntry);
}

void
Forwarder::onIncomingData(Face& inFace, const Data& data)
{
  // receive Data
  NFD_LOG_DEBUG("onIncomingData face=" << inFace.getId() << " data=" << data.getName());
  const_cast<Data&>(data).setIncomingFaceId(inFace.getId());
  ++m_counters.getNInDatas();

  // /localhost scope control
  bool isViolatingLocalhost = !inFace.isLocal() &&
                              LOCALHOST_NAME.isPrefixOf(data.getName());
  if (isViolatingLocalhost) {
    NFD_LOG_DEBUG("onIncomingData face=" << inFace.getId() <<
                  " data=" << data.getName() << " violates /localhost");
    // (drop)
    return;
  }

  // PIT match
  pit::DataMatchResult pitMatches = m_pit.findAllDataMatches(data);
  if (pitMatches.begin() == pitMatches.end()) {
    // goto Data unsolicited pipeline
    this->onDataUnsolicited(inFace, data);
    return;
  }
  

#ifdef NDNSIM
  // Remove Ptr<Packet> from the Data before inserting into cache, serving two purposes
  // - reduce amount of memory used by cached entries
  // - remove all tags that (e.g., hop count tag) that could have been associated with Ptr<Packet>
  //
  // Copying of Data is relatively cheap operation, as it copies (mostly) a collection of Blocks
  // pointing to the same underlying memory buffer.
  shared_ptr<Data> dataCopyWithoutPacket = make_shared<Data>(data);
  dataCopyWithoutPacket->removeTag<ns3::ndn::Ns3PacketTag>();

  // CS insert
  if (m_csFromNdnSim == nullptr)
    m_cs.insert(*dataCopyWithoutPacket);
  else
    m_csFromNdnSim->Add(dataCopyWithoutPacket);
#else
  // CS insert
  m_cs.insert(data);
#endif // NDNSIM

#ifdef NDNSIM
  std::set<shared_ptr<Face> > pendingDownstreams;
#else
  std::set<Face*> pendingDownstreams;
#endif // NDNSIM

  // foreach PitEntry
  for (const shared_ptr<pit::Entry>& pitEntry : pitMatches) {
    NFD_LOG_DEBUG("onIncomingData matching=" << pitEntry->getName());

    // cancel unsatisfy & straggler timer
    this->cancelUnsatisfyAndStragglerTimer(pitEntry);

    // remember pending downstreams
#ifdef NDNSIM
    const pit::InRecordCollection& inRecords = pitEntry->getInRecords();
    for (pit::InRecordCollection::const_iterator it = inRecords.begin();
                                                 it != inRecords.end(); ++it) {
      if (it->getExpiry() > time::steady_clock::now()) {
        pendingDownstreams.insert(it->getFace());
      }
    }
#else
    const pit::InRecordCollection& inRecords = pitEntry->getInRecords();
    for (const pit::InRecord& inRecord : inRecords) {
      if (inRecord.getExpiry() > time::steady_clock::now()) {
        pendingDownstreams.insert(inRecord.getFace().get());
      }
    }

#endif // NDNSIM

    // invoke PIT satisfy callback
#ifdef NDNSIM
    beforeSatisfyInterest(*pitEntry, inFace, data);
#endif // NDNSIM
    this->dispatchToStrategy(pitEntry, bind(&Strategy::beforeSatisfyInterest, _1,
                                            pitEntry, cref(inFace), cref(data)));

    // Dead Nonce List insert if necessary (for OutRecord of inFace)
    this->insertDeadNonceList(*pitEntry, true, data.getFreshnessPeriod(), &inFace);

    // mark PIT satisfied
    pitEntry->deleteInRecords();
    pitEntry->deleteOutRecord(inFace);

    // set PIT straggler timer
    this->setStragglerTimer(pitEntry, true, data.getFreshnessPeriod());
  }

#ifdef PATH_LABELLING
  // Before sending data out, put the incoming face ID information in the path ID field
  if (!data.isPathIdEmpty()) {
    //std::cout << "the old path ID is:";
    //std::cout.write(reinterpret_cast<const char*>(data.getPathId().value()), data.getPathId().value_size());
    //std::cout << std::endl;
    uint8_t pathIdOld = *(data.getPathId().value());
    uint8_t tmp = (pathIdOld<<1)|(pathIdOld>>7);
    uint8_t inFaceID = static_cast<uint8_t>(inFace.getId());
    uint8_t pathIdNew = inFaceID^tmp;
    //std::cout << "inFace=" << inFace.getId() << " inFaceID=" << (int)pathIdNew << " tmp=" << (int)tmp << "\n";
    //std::cout << "FORWARDER: the updated path ID is :" << (int)(pathIdNew) << "\n";
    (const_cast<Data &>(data)).setPathIdAndWire(pathIdNew);
  }
#endif // PATH_LABELLING
  
  // foreach pending downstream
#ifdef NDNSIM
  for (std::set<shared_ptr<Face> >::iterator it = pendingDownstreams.begin();
      it != pendingDownstreams.end(); ++it) {
    shared_ptr<Face> pendingDownstream = *it;
    if (pendingDownstream.get() == &inFace) {
#else
  for (Face* pendingDownstream : pendingDownstreams) {
    if (pendingDownstream == &inFace) {
#endif
      continue;
    }
    // goto outgoing Data pipeline
    this->onOutgoingData(data, *pendingDownstream);
  }
}

void
Forwarder::onDataUnsolicited(Face& inFace, const Data& data)
{
  // accept to cache?
  bool acceptToCache = inFace.isLocal();
  if (acceptToCache) {
    // CS insert
#ifdef NDNSIM
    if (m_csFromNdnSim == nullptr)
#endif // NDNSIM
      m_cs.insert(data, true);
#ifdef NDNSIM
    else
      m_csFromNdnSim->Add(data.shared_from_this());
#endif // NDNSIM
  }

  NFD_LOG_DEBUG("onDataUnsolicited face=" << inFace.getId() <<
                " data=" << data.getName() <<
                (acceptToCache ? " cached" : " not cached"));
}

void
Forwarder::onOutgoingData(const Data& data, Face& outFace)
{
  if (outFace.getId() == INVALID_FACEID) {
    NFD_LOG_WARN("onOutgoingData face=invalid data=" << data.getName());
    return;
  }
  NFD_LOG_DEBUG("onOutgoingData face=" << outFace.getId() << " data=" << data.getName());

  // /localhost scope control
  bool isViolatingLocalhost = !outFace.isLocal() &&
                              LOCALHOST_NAME.isPrefixOf(data.getName());
  if (isViolatingLocalhost) {
    NFD_LOG_DEBUG("onOutgoingData face=" << outFace.getId() <<
                  " data=" << data.getName() << " violates /localhost");
    // (drop)
    return;
  }

  // TODO traffic manager

#ifndef NDNSIM
#ifdef HOP_COUNT
  /*
   * Code added by Mauro (msardara@cisco.com)
   */
  if (!outFace.isLocal())
    const_cast<Data &>(data).incrementHopCount();
#endif
#endif // !NDNSIM

  // send Data
  outFace.sendData(data);
  ++m_counters.getNOutDatas();
}

#ifndef NDNSIM
void
Forwarder::onIncomingNack(Face& inFace, const lp::Nack& nack)
{
  // if multi-access face, drop
  if (inFace.isMultiAccess()) {
    NFD_LOG_DEBUG("onIncomingNack face=" << inFace.getId() <<
                  " nack=" << nack.getInterest().getName() <<
                  "~" << nack.getReason() << " face-is-multi-access");
    return;
  }

  // PIT match
  shared_ptr<pit::Entry> pitEntry = m_pit.find(nack.getInterest());
  // if no PIT entry found, drop
  if (pitEntry == nullptr) {
    NFD_LOG_DEBUG("onIncomingNack face=" << inFace.getId() <<
                  " nack=" << nack.getInterest().getName() <<
                  "~" << nack.getReason() << " no-PIT-entry");
    return;
  }

  // has out-record?
  pit::OutRecordCollection::iterator outRecord = pitEntry->getOutRecord(inFace);
  // if no out-record found, drop
  if (outRecord == pitEntry->getOutRecords().end()) {
    NFD_LOG_DEBUG("onIncomingNack face=" << inFace.getId() <<
                  " nack=" << nack.getInterest().getName() <<
                  "~" << nack.getReason() << " no-out-record");
    return;
  }

  // if out-record has different Nonce, drop
  if (nack.getInterest().getNonce() != outRecord->getLastNonce()) {
    NFD_LOG_DEBUG("onIncomingNack face=" << inFace.getId() <<
                  " nack=" << nack.getInterest().getName() <<
                  "~" << nack.getReason() << " wrong-Nonce " <<
                  nack.getInterest().getNonce() << "!=" << outRecord->getLastNonce());
    return;
  }

  NFD_LOG_DEBUG("onIncomingNack face=" << inFace.getId() <<
                " nack=" << nack.getInterest().getName() <<
                "~" << nack.getReason() << " OK");

  // record Nack on out-record
  outRecord->setIncomingNack(nack);

  // trigger strategy: after receive NACK
  shared_ptr<fib::Entry> fibEntry = m_fib.findLongestPrefixMatch(*pitEntry);
  this->dispatchToStrategy(pitEntry, bind(&Strategy::afterReceiveNack, _1,
                                          cref(inFace), cref(nack), fibEntry, pitEntry));
}

void
Forwarder::onOutgoingNack(shared_ptr<pit::Entry> pitEntry, const Face& outFace,
                          const lp::NackHeader& nack)
{
  if (outFace.getId() == INVALID_FACEID) {
    NFD_LOG_WARN("onOutgoingNack face=invalid" <<
                  " nack=" << pitEntry->getInterest().getName() <<
                  "~" << nack.getReason() << " no-in-record");
    return;
  }

  // has in-record?
  pit::InRecordCollection::const_iterator inRecord = pitEntry->getInRecord(outFace);

  // if no in-record found, drop
  if (inRecord == pitEntry->getInRecords().end()) {
    NFD_LOG_DEBUG("onOutgoingNack face=" << outFace.getId() <<
                  " nack=" << pitEntry->getInterest().getName() <<
                  "~" << nack.getReason() << " no-in-record");
    return;
  }

  // if multi-access face, drop
  if (outFace.isMultiAccess()) {
    NFD_LOG_DEBUG("onOutgoingNack face=" << outFace.getId() <<
                  " nack=" << pitEntry->getInterest().getName() <<
                  "~" << nack.getReason() << " face-is-multi-access");
    return;
  }

  NFD_LOG_DEBUG("onOutgoingNack face=" << outFace.getId() <<
                " nack=" << pitEntry->getInterest().getName() <<
                "~" << nack.getReason() << " OK");

  // create Nack packet with the Interest from in-record
  lp::Nack nackPkt(inRecord->getInterest());
  nackPkt.setHeader(nack);

  // erase in-record
  pitEntry->deleteInRecord(outFace);

  // send Nack on face
  const_cast<Face&>(outFace).sendNack(nackPkt);
}
#endif // !NDNSIM

static inline bool
compare_InRecord_expiry(const pit::InRecord& a, const pit::InRecord& b)
{
  return a.getExpiry() < b.getExpiry();
}

void
Forwarder::setUnsatisfyTimer(shared_ptr<pit::Entry> pitEntry)
{
  const pit::InRecordCollection& inRecords = pitEntry->getInRecords();
  pit::InRecordCollection::const_iterator lastExpiring =
    std::max_element(inRecords.begin(), inRecords.end(),
    &compare_InRecord_expiry);

  time::steady_clock::TimePoint lastExpiry = lastExpiring->getExpiry();
  time::nanoseconds lastExpiryFromNow = lastExpiry  - time::steady_clock::now();
  if (lastExpiryFromNow <= time::seconds(0)) {
    // TODO all InRecords are already expired; will this happen?
  }

  scheduler::cancel(pitEntry->m_unsatisfyTimer);
  pitEntry->m_unsatisfyTimer = scheduler::schedule(lastExpiryFromNow,
    bind(&Forwarder::onInterestUnsatisfied, this, pitEntry));
}


void
Forwarder::setStragglerTimer(shared_ptr<pit::Entry> pitEntry, bool isSatisfied,
                             const time::milliseconds& dataFreshnessPeriod)
{
  time::nanoseconds stragglerTime = time::milliseconds(100);

  scheduler::cancel(pitEntry->m_stragglerTimer);
  pitEntry->m_stragglerTimer = scheduler::schedule(stragglerTime,
    bind(&Forwarder::onInterestFinalize, this, pitEntry, isSatisfied, dataFreshnessPeriod));
}

void
Forwarder::cancelUnsatisfyAndStragglerTimer(shared_ptr<pit::Entry> pitEntry)
{
  scheduler::cancel(pitEntry->m_unsatisfyTimer);
  scheduler::cancel(pitEntry->m_stragglerTimer);
}

static inline void
insertNonceToDnl(DeadNonceList& dnl, const pit::Entry& pitEntry,
                 const pit::OutRecord& outRecord)
{
  dnl.add(pitEntry.getName(), outRecord.getLastNonce());
}

void
Forwarder::insertDeadNonceList(pit::Entry& pitEntry, bool isSatisfied,
                               const time::milliseconds& dataFreshnessPeriod,
                               Face* upstream)
{
  // need Dead Nonce List insert?
  bool needDnl = false;
  if (isSatisfied) {
    bool hasFreshnessPeriod = dataFreshnessPeriod >= time::milliseconds::zero();
    // Data never becomes stale if it doesn't have FreshnessPeriod field
    needDnl = static_cast<bool>(pitEntry.getInterest().getMustBeFresh()) &&
              (hasFreshnessPeriod && dataFreshnessPeriod < m_deadNonceList.getLifetime());
  }
  else {
    needDnl = true;
  }

  if (!needDnl) {
    return;
  }

  // Dead Nonce List insert
  if (upstream == 0) {
    // insert all outgoing Nonces
    const pit::OutRecordCollection& outRecords = pitEntry.getOutRecords();
    std::for_each(outRecords.begin(), outRecords.end(),
                  bind(&insertNonceToDnl, ref(m_deadNonceList), cref(pitEntry), _1));
  }
  else {
    // insert outgoing Nonce of a specific face
    pit::OutRecordCollection::const_iterator outRecord = pitEntry.getOutRecord(*upstream);
    if (outRecord != pitEntry.getOutRecords().end()) {
      m_deadNonceList.add(pitEntry.getName(), outRecord->getLastNonce());
    }
  }
}

} // namespace nfd
