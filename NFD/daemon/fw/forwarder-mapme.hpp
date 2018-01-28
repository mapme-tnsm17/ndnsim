#ifndef NFD_DAEMON_FW_FORWARDER_MAPME_HPP
#define NFD_DAEMON_FW_FORWARDER_MAPME_HPP

#ifdef MAPME

#ifndef FIB_EXTENSIONS
#error "MAPME requires FIB_EXTENSIONS"
#endif // ! FIB_EXTENSIONS

#include "forwarder.hpp"
#include "table/strategy-choice.hpp"
#include "table/fib-entry.hpp"
#include "daemon/face/face.hpp"

#ifdef NDNSIM
//for logging IU overhead
#include "ns3/callback.h"
#include <boost/unordered_map.hpp>
#endif // NDNSIM
//

#define MAPME_DEFAULT_TU 5000 /* ms */
#define MAPME_DEFAULT_RETX 20 /* ms */

namespace nfd {

// XXX merge mobility manager inside this class ?

namespace fw {
  class Strategy;
} // namespace fw

/*-----------------------------------------------------------------------------
 * Temporary FIB (TFIB) entry data structure : FIB extension
 *
 * The TFIB is about managing so called 'previous hops', which correspond to
 * previous next hops that should be (reliably) notified about the new prefix
 * location thanks to an Update interest. Upon completion of all updates for a
 * prefix, the corresponding entry is empty and ought to be deleted.
 *
 * TODO:
 *  - update forwarding of regular interests (only add discovery)
 *  - add support for producer/interface identifier
 *  - document face lifetime in TFibEntry
 *  - check the correctness of the algorithm when an IN is received on the same
 *    face an IU was previously received (acked or not)
 *  - shall an IN cancel previous IU wrt retransmissions ? first idea: no.
 *    RULE? an IN never touches the existing pendingUpdates part, it can just
 *    add entries with NULL timer ! XXX to check/enforce in the code!
 *  - avoid sending IU back for a producer (see first question).
 *  - if I ever need to send an IU, I can find those in the pending list with
 *    !timer. When are those entries cleared ?
 *
 * FUTURE EXTENSION: Multiple producers
 *  - what about two producers, one doing update, the other notifications ?
 *
 * FUTURE EXTENSION: seq sync
 *  - how to determine seq ? how to know it is high enough for global update ?
 *  we will receive IU back with a higher seq, but not necessarily the highest.
 *  we might send a probe packet to the last hop before the destination ? IU
 *  with seq = FFFF..FF, that does not update FIB at all, but reaches the last
 *  hop which then replies with the seq instead of stopping the process. BUT...
 *  this reply should not affect the FIB neither.
 *
 * QUESTIONS:
 *  - is there an interest to inform the producer that he has the wrong sequence
 *     number ? or that the packet was dropped ? ICMP like ?
 *
 * NOTES:
 *  - a pendingUpdate timer can be used in two ways:
 *
 *    . with IU, to implement reliability
 *
 *    . with IN, to implement a heuristic like (send IU in XXX seconds). Note
 *    that it is important (XXX why?) that the IU sent after an IN has an
 *    incremented seq. In fact, we mix reliability and heuristics. Heuristic
 *    occurs after the ack, but we need to take into account the association
 *    time, not the ack time. 
 *
 *    -> This allow us to really ignore packets with a lower seq, without
 *    checking if they being more info (eg IU after IN).
 *
 *  - There is no such thing as a pendingUpdates that has completed / been acked. An
 *  ack removed the face from the pendingUpdate map.
 *
 *---------------------------------------------------------------------------*/

class FibExtensionData;

class TFibEntry : public FibExtensionData 
{
private:

  // XXX shall we consider std::unordered_map or even boost:unordered_map ?
  // XXX because the face might not be valid anymore, it might be better to use
  // weak_ptr than shared_ptr
  /* NOTES:
   *  - let's avoid using pointers since they cannot be easily used in a map
   */
  std::map<FaceId, scheduler::EventId> pendingUpdates;
  
  // Per-prefix heuristic timer
  // XXX check timer type !
  // XXX This should be for the producer only
  std::map<Name, time::steady_clock::TimePoint> m_lastAckedUpdate;

  friend class ForwarderMapMe;

};


/*------------------------------------------------------------------------------
 * ForwarderMapMe : forwarding pipeline
 *----------------------------------------------------------------------------*/

class ForwarderMapMe : public Forwarder
{
public:
  ForwarderMapMe(uint64_t Tu);

  void
  onSpecialInterest(Face & inFace, const Interest & interest);

  void
  onSpecialInterestAck(Face& inFace, const Interest& interest);

  virtual void
  onIncomingInterest(Face& inFace, const Interest& interest);

  virtual void
  onContentStoreMiss(const Face& inFace, shared_ptr<pit::Entry> pitEntry, const Interest& interest);

protected:

  void 
  BroadcastInterest(const Interest& interest, const Face& inFace, shared_ptr<pit::Entry> pitEntry = nullptr);

#ifdef NDNSIM
  void
  Discard(const Interest& interest);
#endif // NDNSIM 

  bool
  isFaceValidAndUp(shared_ptr<Face> face);

  bool
  anyFaceValid(shared_ptr<fib::Entry> fibEntry);

private:
  void
  onFaceAdded(shared_ptr<Face> face);

  void
  sendSpecialInterest(Name prefix, weak_ptr<fib::Entry> fibEntry, FaceId faceId, bool is_first, uint64_t num_retx);

  void
  setFacePending(Name prefix, weak_ptr<fib::Entry> wpFibEntry, shared_ptr<TFibEntry> tfibEntry, FaceId faceId, bool send, bool is_first, uint32_t num_retx = 0);

protected:
  static const time::nanoseconds IU_retransmissionTime;

  // allow Strategy (base class) to enter pipelines
  friend class fw::Strategy;

protected:

  uint64_t m_Tu;
  uint64_t m_retx;

#ifdef NDNSIM
public:

  /* Callback triggered upon IU reception */
  ns3::Callback<void, std::string /* prefix */, uint64_t /* seq */, uint64_t /* ttl */, uint64_t /* num_retx */> m_onSpecialInterest;
  ns3::Callback<void, std::string /* prefix */, uint64_t /* seq */, uint64_t /* ttl */, uint64_t /* num_retx */> m_onProcessing;
#endif // NDNSIM

};

} // namespace nfd

#endif // MAPME

#endif // NFD_DAEMON_FW_FORWARDER_MAPME_HPP
