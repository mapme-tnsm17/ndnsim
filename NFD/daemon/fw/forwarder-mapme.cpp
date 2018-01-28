#ifdef MAPME

#include "forwarder-mapme.hpp"

#include "core/logger.hpp"
#include "core/random.hpp"
#include "face/null-face.hpp"
#include "forwarder.hpp"
#include "strategy.hpp"

#define MS2NS(x) time::nanoseconds(x * 1000000)
#define S2MS(x) (x * 1000)

namespace nfd {

NFD_LOG_INIT("ForwarderMapMe");

using fw::Strategy;

#define UNIT_COST 1
#define TTL_NA 255
#define RETX_NA 255

// Currently, discovery goes in hand with notifications. We might decouple this.
#define DO_DISCOVERY (m_Tu > 0)

/*------------------------------------------------------------------------------
 * ForwarderMapMe : forwarding pipeline - Constructor & accessors
 *
 * TODO
 *  - ideally, should take the TFibEntry class as a parameter so that we can
 *  subclass it
 *
 *----------------------------------------------------------------------------*/

ForwarderMapMe::ForwarderMapMe(uint64_t Tu = MAPME_DEFAULT_TU)
  : Forwarder()
  , m_Tu(Tu)
  , m_retx(MAPME_DEFAULT_RETX)
#ifdef NDNSIM
  , m_onSpecialInterest(ns3::MakeNullCallback<void, std::string, uint64_t, uint64_t, uint64_t>())
  , m_onProcessing(ns3::MakeNullCallback<void, std::string, uint64_t, uint64_t, uint64_t>())
#endif // NDNSIM
{
  /* XXX This cannot be an initializer because... */
  m_removeFibEntries = false;

  /* MapMe reacts to face add events to send special interests */
  getFaceTable().onAdd.connect(bind(&ForwarderMapMe::onFaceAdded, this, _1));
}

/*------------------------------------------------------------------------------
 * Helper functions
 *----------------------------------------------------------------------------*/

bool
ForwarderMapMe::isFaceValidAndUp(shared_ptr<Face> face)
{
  //return (static_cast<bool>(face) && face->getId() != INVALID_FACEID && face->isUp());
  bool valid = (static_cast<bool>(face) && face->getId() != INVALID_FACEID && face->isUp());
// DISABLED|  if (!valid) {
// DISABLED|    std::cout << "Œ " << static_cast<bool>(face) << " id=" << face->getId() << " != " << INVALID_FACEID << "up=" << face->isUp() << std::endl;
// DISABLED|  }
  return valid;
}

bool
ForwarderMapMe::anyFaceValid(shared_ptr<fib::Entry> fibEntry)
{
// DISABLED|  std::cout << " x entry " << fibEntry->getPrefix() << std::endl;
  for (auto it : fibEntry->getNextHops()) {
    if(isFaceValidAndUp(it.getFace())) {
//      std::cout << "   . valid face " << it.getFace()->getId() << " - desc=" << it.getFace()->getDescription() << std::endl;
      return true;
    }
  }
  return false;
}

void 
ForwarderMapMe::BroadcastInterest(const Interest& interest, const Face & inFace, shared_ptr<pit::Entry> pitEntry)
{
  FaceTable::const_iterator it;
  for (it = m_faceTable.begin(); it != m_faceTable.end(); ++it) {
    // avoid local, X2, ingress and down faces
    if (!(*it)->isLocal() && ((*it)->isX2()) && it->get() != &inFace && (*it)->isUp() ) {
      //std::cout << "- found face" << (*it)->getDescription() << std::endl;
      if (pitEntry) {
        NFD_LOG_INFO("    . inserting/updating pit entry");
        pitEntry->insertOrUpdateOutRecord(*it, interest);
      }
      (*it)->sendInterest(interest);
      ++m_counters.getNOutInterests();   
//    }	else {
//      if ((*it)->isLocal())
//        std::cout << "face " << (*it)->getDescription() << " is local" << std::endl;
//      if ((*it)->isX2())
//        std::cout << "face " << (*it)->getDescription() << " is X2" << std::endl;
//      if (it->get() == &inFace)
//        std::cout << "face " << (*it)->getDescription() << " is ingress" << std::endl;
//      if (!(*it)->isUp())
//        std::cout << "face " << (*it)->getDescription() << " is down" << std::endl;
    }
  }
}

/*------------------------------------------------------------------------------
 * Special interest management (creation & retransmission)
 *----------------------------------------------------------------------------*/

// Per-prefix heuristics
//  - IU/IN not acked, send IU immediately
//  - send IU at least every X seconds (or N updates)
//  - send IU after attached more than Y seconds ?
//  - always send IU after long disconnectivity ?

// send == force sending the packet
void
ForwarderMapMe::sendSpecialInterest(Name prefix, weak_ptr<fib::Entry> wpFibEntry, FaceId faceId, bool is_first, uint64_t num_retx)
{
  shared_ptr<Face> face = getFaceTable().get(faceId);
  if (!isFaceValidAndUp(face)) {
    // This could happen if the producer moves fast, evt. during a retransmission
    NFD_LOG_INFO("Invalid face, cannot send special interest");
    return;
  }

  /* In case fibEntry has been deleted... */
  shared_ptr<nfd::fib::Entry> fibEntry = wpFibEntry.lock();
  if (!fibEntry) { 
    NS_LOG_INFO("FIB entry has been deleted, no scheduling any IU or IN");   	
    return;  
  }

  /* We get the sequence number from the fibEntry every time to be sure to send
   * the latest one (in case of retransmissions)
   */
  uint64_t seq = fibEntry->getSeq();

  /* Heuristic: interest type */
  shared_ptr<TFibEntry> tfibEntry = dynamic_pointer_cast<TFibEntry>(fibEntry->getExtension());
  assert(tfibEntry);
  auto it = tfibEntry->m_lastAckedUpdate.find(prefix);
  time::steady_clock::time_point now = time::steady_clock::now();
  double interval_s = time::duration_cast<time::duration<double>>(now - it->second).count();

  tlv::SpecialInterestTypeValue type = (it != tfibEntry->m_lastAckedUpdate.end() 
      && (S2MS(interval_s) < m_Tu) && is_first)
    ? tlv::TYPE_IN 
    : tlv::TYPE_IU;

  shared_ptr<Interest> specialInterest = make_shared<Interest>(prefix);
  specialInterest->setAsSpecialInterest(type, seq);

  NS_LOG_INFO(" - sendSpecialInterest prefix=" << prefix << " face=" <<
      face->getDescription() << " seq=" << seq << " type=" << type);

#ifdef NDNSIM
  m_onSpecialInterest(prefix.toUri(), seq, TTL_NA, num_retx);
#endif // NDNSIM
  face->sendInterest(*specialInterest);

  // XXX manage retransmission here ?
  // - expected by onFaceAdded
}

/*------------------------------------------------------------------------------
 * Event handling
 *----------------------------------------------------------------------------*/

void
ForwarderMapMe::onFaceAdded(shared_ptr<Face> face)
{
  // Ignore local faces
  if (face->isLocal())
    return;

  // TODO improve efficiency by avoiding FIB scans everytime

  for (auto i = m_fib.begin(); i != m_fib.end(); i++) {
    shared_ptr<fib::Entry> entry = i.operator->();
    shared_ptr<TFibEntry> tfibEntry = dynamic_pointer_cast<TFibEntry>(entry->getExtension());
    if (!tfibEntry) {
      tfibEntry = make_shared<TFibEntry>();
      entry->setExtension(tfibEntry);
    }

    for (auto & j : entry->getNextHops()) {
      if (j.getFace()->getLocalUri() == FaceUri("appFace://")) {
        entry->setSeq(entry->getSeq() + 1);
        // We need to schedule it because the face might have been called while
        // the underlying physical layer is not ready yet (eg. ndnSIM)
        // Default argument does not seem to be taken into account by bind
        NS_LOG_INFO("***** MOBILITY EVENT prefix=" << entry->getPrefix() << ". Sending special interest");
        // We set false because we don't want to force an IU
        scheduler::schedule(MS2NS(0), bind(&ForwarderMapMe::setFacePending, this,
              entry->getPrefix(), weak_ptr<fib::Entry>(entry), tfibEntry, face->getId(), true, true, 0));
				break;
			}
		}
  }
}


/*------------------------------------------------------------------------------
 * Special interest handling
 *----------------------------------------------------------------------------*/
 
// XXX NOTE: we can have prevHops without timer, are they fully considered here
// ?
void
ForwarderMapMe::onSpecialInterest(Face & inFace, const Interest & interest)
{
  uint64_t fibSeq, seq = interest.getSequenceNumber();
  bool send = (interest.getSpecialInterestType() == tlv::TYPE_IU);

  NS_LOG_INFO("(Special) Incoming special interest " << interest.getName());
  m_onProcessing(interest.getName().toUri(), seq, TTL_NA, RETX_NA);
  // XXX This could be more general ! onIncomingInterest !
  NFD_LOG_INFO("onIncomingIU(face=" << inFace.getDescription() << 
      ", interest=" << interest.getName() << ", seq="<< seq << ")");

  /* Immediately send an acknowledgement back */
  // XXX Wrong, only send back ack if valid !
  NFD_LOG_INFO("send IU ack for prefix=" << interest.getName() << ", face=" << inFace.getDescription());
  shared_ptr<Interest> ack = make_shared<Interest>(interest);
  ack->setAsSpecialInterest(send ? tlv::TYPE_IU_ACK: tlv::TYPE_IN_ACK, seq);
  inFace.sendInterest(*ack);

  shared_ptr<fib::Entry> fibEntry = m_fib.findExactMatch(interest.getName());
  if (!static_cast<bool>(fibEntry)) {
    /* This will typically occur for a producer that has moved, the face is
     * destroyed, and thus all corresponding FIB entries
     * Upon receiving an IU, we need to create a new FIB entry
     */
    fibEntry = m_fib.insert(interest.getName()).first;
    // XXX shall this be done atomically ?
    fibEntry->addNextHop(inFace.shared_from_this(), UNIT_COST);
    fibEntry->setSeq(0);
    fibEntry->setExtension(make_shared<TFibEntry>());

    NFD_LOG_INFO("Created FIB Entry for prefix" << interest.getName()
        << " (seqno=" << interest.getSequenceNumber()
        << ", face=" << inFace.getDescription() << ")"); 
    // TODO  initial prefix dissemination might be handled here
    
// XXX Why is it needed ?
// DEPRECATED|#ifdef NDNSIM
// DEPRECATED|    Discard(interest);
// DEPRECATED|#endif // NDNSIM
  }

  shared_ptr<TFibEntry> tfibEntry = dynamic_pointer_cast<TFibEntry>(fibEntry->getExtension());
  if (!tfibEntry) {
    tfibEntry = make_shared<TFibEntry>();
    fibEntry->setExtension(tfibEntry);
  }


  fibSeq = fibEntry->getSeq(); // XXX
  if (seq > fibSeq) {

    NS_LOG_INFO("OnSpecialInterest: Received higher seqno, updating FIB seqno");
    /* This has to be done first to allow processing SpecialInterestAck's */
    fibEntry->setSeq(seq);

    /* Reliably forward the IU on all prevHops.
     *
     * NOTE: we obviously don't forward an IN. 
     */
    NS_LOG_INFO("OnSpecialInterest: forwarding IU to pending faces");
    if (interest.getSpecialInterestType() == tlv::TYPE_IU) {
      for (auto it = tfibEntry->pendingUpdates.begin(); it != tfibEntry->pendingUpdates.end(); it++) {
        setFacePending(interest.getName(), fibEntry, tfibEntry, it->first, send, false);
      }
    }

    /* nextHops -> prevHops
     *
     * We add to the list of pendingUpdates the current next hops, and
     * eventually forward them an IU too.
     *
     * Exception: nextHops -> nextHops
     *   Because of retransmission issues, it is possible that a second interest
     *   (with same of higher sequence number) is receive from a next-hop
     *   interface. In that case, the face remains a next hop.
     * 
     * NOTE: even if only the seq changes, and not the shape of the
     *   arborescence, we still forward an IU
     */
    const fib::NextHopList& nexthops = fibEntry->getNextHops();

    NS_LOG_INFO("OnSpecialInterest: setting previous next hops as pending and forwarding IU");
    bool isAlreadyNextHop = false;
    bool complete = true;
    for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it) {
      shared_ptr<Face> face = it->getFace();
      NFD_LOG_INFO(" - Next hop face=" << face->getDescription());
      if (face == inFace.shared_from_this()) {
        NFD_LOG_INFO("   . Ignored this next hop since equal to inFace=" << inFace.getDescription());
        isAlreadyNextHop = true;
        continue;
      }

      // This will forward an IU + add an entry for an IU or IN
      setFacePending(interest.getName(), fibEntry, tfibEntry, face->getId(), send, false);
      complete = false;
    }

    if (complete)
      NS_LOG_INFO("Update completed ----------------------");

    /* We are considering : * -> nextHops
     *
     * If inFace was a previous hop, we need to cancel the timer and remove
     * the entry. Also, the face should be added to next hops.
     * 
     * Optimization : nextHops -> nextHops 
     *  - no next hop to add
     *  - we know that inFace was not a previous hop since it was a next hop and
     *  this forms a partition. No need for a search
     */
    NS_LOG_INFO("OnSpecialInterest: Updating FIB next hops if needed");
    if (!isAlreadyNextHop) {
      auto it = tfibEntry->pendingUpdates.find(inFace.getId());
      if (it != tfibEntry->pendingUpdates.end()) {
        scheduler::cancel(it->second);
        tfibEntry->pendingUpdates.erase(it);
      }

      fibEntry->removeNextHops();
      fibEntry->addNextHop(inFace.shared_from_this(), UNIT_COST);
    }

    /* In all cases, the FIB entry has been updated, so we signal it and
     * forward the special interest if it was an IU. 
     *
     * NOTE: we always forward it even if only the sequence number has been
     * updated (immediate reconnect for instance).
     */
    m_fib._onUpdate(fibEntry);

  } else if (seq == fibSeq) {
    /* Multipath, multihoming, multiple producers or duplicate interest
     *
     * XXX Let's not add a next hop twice (is it checked by addNextHop ?)
     *
     * In all cases, we assume the propagation was already done when the first
     * interest with the same sequence number was received, so we stop here
     */
    // in case of duplicate, don't send signal !!

    NFD_LOG_INFO("Update the FIB by appending the ingress face: multipath case, no need to forward");
    // addNextHop takes care of avoiding duplicates, but it will re-sort next
    // hops all the time.
    fibEntry->addNextHop(inFace.shared_from_this(), UNIT_COST);
    m_fib._onUpdate(fibEntry);

  } else { // seq < fibSeq
    /* face is propagating outdated information, we can just consider it as a
     * prevHops */
    NFD_LOG_INFO("Update the interest (" << seq << ") with sequence number in FIB (" << fibSeq << "), and send it backward");

    /* Send the special interest backwards with the new sequence number to
     * reconciliate this outdated part of the arborescence.
     *
     * NOTE: we don't do this for IN since there is no use in sending the
     * sequence number to the producer back again ?
     */
    setFacePending(interest.getName(), fibEntry, tfibEntry, inFace.getId(), send, false);
  }

}

/* TFIB entry can be shared_ptr since it is used only once */
// Reliable send interest
// XXX might need a weak_ptr for tfibEntry ????
void
ForwarderMapMe::setFacePending(Name prefix, weak_ptr<fib::Entry> wpFibEntry, shared_ptr<TFibEntry> tfibEntry, FaceId faceId, bool send, bool is_first, uint32_t num_retx)
{
  NS_LOG_INFO(" - setFacePending " << getFaceTable().get(faceId)->getDescription()
      << " prefix=" << prefix << " num_retx=" << num_retx);
  scheduler::EventId timer;

  // XXX need to cancel an existing timer 
  //
  /* NOTE: if the face is pending an we receive an IN, maybe we should not
   * cancel the timer
   */

  // NOTE
  // - at producer, send always true, we always send something reliably so we
  // set the timer.
  // - in the network, we always forward an IU, and never an IN
  if (is_first || send) {
    NS_LOG_INFO("** reliably sending special interest");
    sendSpecialInterest(prefix, wpFibEntry, faceId, is_first, num_retx);
    timer = scheduler::schedule(MS2NS(m_retx), bind(&ForwarderMapMe::setFacePending, this, prefix, wpFibEntry, tfibEntry, faceId, send, is_first, num_retx + 1));
  } else {
    NS_LOG_INFO("** not forwarding because send is false");
    timer = scheduler::EventId();
  }

  /* This is NULL in case of a received IN */
  // XXX was pending and we receive an IN ????

  // If the timer exists, cancel it and recreate it (= reset)
  auto it = tfibEntry->pendingUpdates.find(faceId);
  if ((it != tfibEntry->pendingUpdates.end()) && (it->second))
    scheduler::cancel(it->second);

  tfibEntry->pendingUpdates[faceId] = timer;
}

void
ForwarderMapMe::onSpecialInterestAck(Face& inFace, const Interest& interest)
{
  NFD_LOG_INFO("onIncoming interest, type=IU_ACK, face=" << inFace.getDescription() <<
      " interest=" << interest.getName()<<", seq="<<interest.getSequenceNumber());

  shared_ptr<fib::Entry> fibEntry = m_fib.findExactMatch(interest.getName());
  assert(!!fibEntry);

  /* Test if the latest pending update has been ack'ed, otherwise just ignore */
  // XXX notification ack use timer from pendingUpdates ?????
  // -> in fact pendingUpdate assumes (for naming) that everything is an update,
  // forwarded or not. We had better name it pending or pendingSignalization
  // or...
  // XXX need better type management here 
  if (interest.getSequenceNumber() != -1) {
    uint64_t seq = (uint64_t)interest.getSequenceNumber();
    uint64_t seq_fib = fibEntry->getSeq();
    if (seq < seq_fib) {
      NS_LOG_INFO("Ignored special interest Ack with seq=" << seq << ", expected " << seq_fib);
      return;
    }
  }

  shared_ptr<TFibEntry> tfibEntry = dynamic_pointer_cast<TFibEntry>(fibEntry->getExtension());
  if (!tfibEntry) {
    tfibEntry = make_shared<TFibEntry>();
    fibEntry->setExtension(tfibEntry);
  }
  
  auto it = tfibEntry->pendingUpdates.find(inFace.getId());
  if (it == tfibEntry->pendingUpdates.end()) {
    NS_LOG_INFO("Ignored Possible duplicate IU ack");
    return;
  }

  if (it->second)
    scheduler::cancel(it->second);
  tfibEntry->pendingUpdates.erase(it);
  NFD_LOG_INFO(" . removed TFIB entry for ack coming on face=" << inFace.getDescription());

  // XXX This is for the producer only
  // We need to update the timestamp only for IU Acks, not for IN Acks
  if (interest.getSpecialInterestType() == tlv::TYPE_IU_ACK) {
    tfibEntry->m_lastAckedUpdate[interest.getName()] = time::steady_clock::now();
  }
}

/*-----------------------------------------------------------------------------
 * Overloaded functions
 *----------------------------------------------------------------------------*/

// XXX we might not need to touch forwarding at all but for discovery !!!!
//
void
ForwarderMapMe::onContentStoreMiss(const Face& inFace,
                                   shared_ptr<pit::Entry> pitEntry,
                                   const Interest& interest)
{
  NFD_LOG_INFO("(I) onContentStoreMiss interest=" << interest.getName() << " - face=" << inFace.getDescription());

  shared_ptr<Face> face = const_pointer_cast<Face>(inFace.shared_from_this());
  // insert InRecord
  pitEntry->insertOrUpdateInRecord(face, interest);

  // set PIT unsatisfy timer
  this->setUnsatisfyTimer(pitEntry);

  // FIB lookup
  shared_ptr<fib::Entry> fibEntry = m_fib.findLongestPrefixMatch(interest.getName());
  if (!fibEntry) {
    NFD_LOG_INFO("No corresponding FIB entry. Dropped interest " << interest.getName());
    return;
  }

  bool isDiscovery = false;
  // XXX Handle the case of incoming discovery interests
  if (DO_DISCOVERY && (interest.getSequenceNumber() != -1)) {
    isDiscovery = true;
    /* Discovery mode */
    uint64_t seq = (uint64_t)interest.getSequenceNumber();
    if (seq >= fibEntry->getSeq()) {
      NFD_LOG_INFO("Discard discovery interest with higher seq" << interest.getName());
      /* Drop the interest */
      return;
    }
    NFD_LOG_INFO("Discovery interest made progress seq=" << seq << "< fibseq=" << fibEntry->getSeq());
  }

  // XXX Remove discovery flag
  const_cast<Interest&>(interest).setAsSpecialInterest(tlv::TYPE_UNDEFINED, -1);

  if (anyFaceValid(fibEntry)) {
    if (isDiscovery) {
      NFD_LOG_INFO("Discovery interest found valid face " << interest.getName());
    }
    this->dispatchToStrategy(pitEntry, bind(&Strategy::afterReceiveInterest, _1,
                                          cref(inFace), cref(interest), fibEntry, pitEntry)); 
    
  } else if (DO_DISCOVERY) {
    NFD_LOG_INFO("No valid IU face found, broadcast the interest " << interest.getName() << " with seq=" << fibEntry->getSeq());
    const_cast<Interest&>(interest).setAsSpecialInterest(tlv::TYPE_UNDEFINED, fibEntry->getSeq());
    BroadcastInterest(interest, inFace, pitEntry);
  } else {
    NFD_LOG_INFO("No discovery. Dropped interest" << interest.getName());
  }
}

/* Hooks for special interest processing */
void
ForwarderMapMe::onIncomingInterest(Face& inFace, const Interest& interest)
{
  switch (interest.getSpecialInterestType())
  {
    case tlv::TYPE_IU:
    case tlv::TYPE_IN:
      // XXX NEW SPECIAL INTEREST
      onSpecialInterest(inFace, interest);
      return;

    case tlv::TYPE_IU_ACK:
    case tlv::TYPE_IN_ACK:
      // XXX NEW SPECIAL INTEREST ACK
      onSpecialInterestAck(inFace, interest);
      return;

    default:
      // Normal behaviour
      Forwarder::onIncomingInterest(inFace, interest);
      break;
  }
}

} // namespace nfd

#endif // MAPME
