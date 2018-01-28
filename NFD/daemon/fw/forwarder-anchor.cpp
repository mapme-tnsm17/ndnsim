#ifdef ANCHOR

#include "forwarder-anchor.hpp"

#include "core/logger.hpp"
#include "core/random.hpp"
#include "face/null-face.hpp"
#include "forwarder.hpp"
#include "strategy.hpp"

#include <boost/algorithm/string/predicate.hpp>

#define MS2NS(x) time::nanoseconds(x * 1000000)

namespace nfd {

NFD_LOG_INIT("ForwarderAnchor");

using fw::Strategy;

#define UNIT_COST 1
#define TTL_NA 0
#define NUM_RETX_NA 0

/*------------------------------------------------------------------------------
 * ForwarderAnchor
 *----------------------------------------------------------------------------*/

ForwarderAnchor::ForwarderAnchor()
  : Forwarder()
  , m_retx(ANCHOR_DEFAULT_RETX)
#ifdef NDNSIM
  , m_onSpecialInterest(ns3::MakeNullCallback<void, std::string, uint64_t, uint64_t, uint64_t>())
  , m_onProcessing(ns3::MakeNullCallback<void, std::string, uint64_t, uint64_t, uint64_t>())
#endif // NDNSIM
{
  /* Anchor reacts to face add events to send special interests */
  getFaceTable().onAdd.connect(bind(&ForwarderAnchor::onFaceAdded, this, _1));
}

/*------------------------------------------------------------------------------
 * Event handling
 *----------------------------------------------------------------------------*/

bool
ForwarderAnchor::isFaceValidAndUp(shared_ptr<Face> face)
{
  return (static_cast<bool>(face) && face->getId() != INVALID_FACEID && face->isUp());
}

void
ForwarderAnchor::sendSpecialInterest(Name prefix, weak_ptr<fib::Entry> wpFibEntry, FaceId faceId, uint64_t num_retx)
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

  NS_LOG_INFO("! sendSpecialinterest prefix=" << prefix << ", fib prefix=" << fibEntry->getPrefix());

  /* We get the sequence number from the fibEntry every time to be sure to send
   * the latest one (in case of retransmissions)
   */
  uint64_t seq = fibEntry->getSeq();

  Name announceName(ANCHOR_ANNOUNCE_STR + std::string("/") + boost::lexical_cast<std::string>(seq) + prefix.toUri());
  shared_ptr<Interest> specialInterest = make_shared<Interest>(announceName, ndn::time::milliseconds(m_retx));

  NFD_LOG_INFO("FW/anchor : sending prefix announcement interest to BS/anchor: " << announceName << " face=" << face->getDescription() << "/" << face->getId() << ", num_retx=" << num_retx);

  face->sendInterest(*specialInterest);
  
  // Schedule retransmission
  m_pendingUpdates[prefix] = scheduler::schedule(MS2NS(ANCHOR_DEFAULT_RETX),
      bind(&ForwarderAnchor::sendSpecialInterest, this, 
        prefix, wpFibEntry, faceId, ++num_retx));
}

void
ForwarderAnchor::onFaceAdded(shared_ptr<Face> face)
{
  // Ignore application faces...
  if (face->getLocalUri() == FaceUri("appFace://"))
    return;

  // TODO improve efficiency by avoiding FIB scans everytime
  NFD_LOG_INFO("ForwarderAnchor::onFaceAdded face=" << face->getDescription());

  //NFD_LOG_INFO("Scanning FIB...");
  for (auto i = m_fib.begin(); i != m_fib.end(); i++) {
    shared_ptr<fib::Entry> entry = i.operator->();
    //NFD_LOG_INFO("ENTRY =" << entry->getPrefix());
    for (auto & j : entry->getNextHops()) {
      //NFD_LOG_INFO("next hop - face uri=" << j.getFace()->getLocalUri());
      if(j.getFace()->getLocalUri() == FaceUri("appFace://")) {
        std::string pfx = entry->getPrefix().toUri();
        //NFD_LOG_INFO("Current pfx=" << pfx << ", ANCHOR_ANNOUNCE_STR=" << ANCHOR_ANNOUNCE_STR << ", ANCHOR_LOCATOR_STR=" << ANCHOR_LOCATOR_STR);
        if (boost::starts_with(pfx, ANCHOR_ANNOUNCE_STR) || boost::starts_with(pfx, ANCHOR_LOCATOR_STR) || boost::starts_with(pfx, ANCHOR_UPDATE_STR)) {
          continue;
        }
        std::cout << pfx << " -- " << ANCHOR_ORIGIN_STR << std::endl;

        // For Anchor-based, all prefixes on producers are prefixes with
        // ANCHOR_ORIGIN_STR. It might happen that a face is created on the node
        // hosting the Anchor and we have to ignore such prefixes...
        if (!boost::starts_with(pfx, ANCHOR_ORIGIN_STR)) {
          continue;
        }
        Name basePrefix(entry->getPrefix().getSubName(Name(ANCHOR_ORIGIN_STR).size()));

        uint64_t seq = entry->getSeq();
        entry->setSeq(seq+1);

        //NFD_LOG_INFO("we are a producer for prefix=" << entry->getPrefix() << ", sending special interest seq=0");
        scheduler::schedule(MS2NS(0), bind(&ForwarderAnchor::sendSpecialInterest, this,
              basePrefix, weak_ptr<fib::Entry>(entry), face->getId(), 0));

				break;
			}
		}
  }
}

/*------------------------------------------------------------------------------
 * Interest / data handling
 *----------------------------------------------------------------------------*/

void
ForwarderAnchor::onIncomingData(Face& inFace, const Data& data)
{
  // Handle special interests and cancel timeouts
  // pair<prefix, seq> -> event
  const Name& originalName = data.getName();
  const Name& announcePrefix = Name(ANCHOR_ANNOUNCE_STR);
  if (announcePrefix.isPrefixOf(originalName)) {
    const Name& prefix = originalName.getSubName(announcePrefix.size() + 1);

    // XXX currently, no check on sequence number

    auto it = m_pendingUpdates.find(prefix);
    if (it != m_pendingUpdates.end()) {
      // This forwarder is the originator of the message
      scheduler::cancel(it->second);
      m_pendingUpdates.erase(it);
      NS_LOG_INFO(" . Canceled retransmission for ack coming on face=" << inFace.getDescription());
      return;
    }
  } 

  // Continue sending the data packet back using the PIT, etc.
  Forwarder::onIncomingData(inFace, data);
}

#ifdef NDNSIM
void
ForwarderAnchor::onOutgoingInterest(shared_ptr<pit::Entry> pitEntry, Face& outFace,
                        bool wantNewNonce)    
{
  Forwarder::onOutgoingInterest(pitEntry, outFace, wantNewNonce);

  const Name interestName(pitEntry->getName());
  std::string interestNameStr(interestName.toUri());

  /* Don't account for messages forwarded to the Anchor app */
  if (outFace.getLocalUri() == FaceUri("appFace://")) {

    // We only forward updates */
    if (boost::starts_with(interestNameStr, ANCHOR_UPDATE_STR)) {
      std::string seq_s(interestName.getSubName(1, 1).toUri());
      seq_s.erase(0, 1);
      uint64_t seq = std::stoull(seq_s);
      m_onProcessing(interestName.getSubName(3).toUri(), seq, TTL_NA, NUM_RETX_NA);
    }

    return;
  }

  /* XXX assume locator size is 1
   *
   * /ANNOUNCE/<seq>/name/of/prefix
   * /UPDATE/<seq>/<locator>/name/of/prefix
   */

  if (boost::starts_with(interestNameStr, ANCHOR_ANNOUNCE_STR)) {
    std::string seq_s(interestName.getSubName(1, 1).toUri());
    seq_s.erase(0, 1);
    uint64_t seq = std::stoull(seq_s);
    m_onSpecialInterest(interestName.getSubName(2).toUri(), seq, TTL_NA, NUM_RETX_NA);
  } else if (boost::starts_with(interestNameStr, ANCHOR_UPDATE_STR)) {
    std::string seq_s(interestName.getSubName(1, 1).toUri());
    seq_s.erase(0, 1);
    uint64_t seq = std::stoull(seq_s);
    m_onSpecialInterest(interestName.getSubName(3).toUri(), seq, TTL_NA, NUM_RETX_NA);
  }
}
#endif // NDNSIM

} // namespace nfd

#endif // ANCHOR
