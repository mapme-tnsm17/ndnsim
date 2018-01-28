/**
 * 
 * author: xuan zeng (xuan.zeng@irt-systemx.fr)
 * 
 * this class contains the implementation of KITE producer mobility management protocol
 * with a few of of our fixes identified through simulations.
 */



#ifndef NFD_DAEMON_FW_FORWARDER_KITE_HPP
#define NFD_DAEMON_FW_FORWARDER_KITE_HPP

#ifdef KITE

#include "forwarder.hpp"

// DEPRECATED| static const Time TRACE_UPDATE_INTERVAL;
// DEPRECATED| const Time Producer::TRACE_UPDATE_INTERVAL=Time("1.0s");
#define KITE_DEFAULT_RETX 2000 /* ms */
#define KITE_DEFAULT_RETX_FACTOR 1
#define KITE_RELIABLE_RETX 60  /*ms*/

#define KITE_BASE_TRACENAME "/anchor"
#define KITE_TRACE_ACK_PREFIX "/KITEACK"

// XXX use TRACED or TRACING, it is already confusing !
#define KITE_FLAG_TRACEABLE 1 
#define KITE_FLAG_FOLLOW_TRACE_AND_FIB 0 // the one sent by consumer ! XXX maybe ill named
#define KITE_FLAG_FOLLOW_TRACE_ONLY 2    // the one sent by consumer ! XXX maybe ill named

#define WITH_RELIABLE_RETRANSMISSION 1

namespace nfd {

class ForwarderKite : public Forwarder
{
public:

  ForwarderKite();
  /** @brief set the pull bit of forwarder kite. If pull bit is set, pending interests can be pulled/retransmitted
   *  when traced interest arrives at a router
   *  by default, the pull bit is set to false, to make a fair comparison with kite.
   */  
  void setPull(bool isPull)
  {
    m_isPull=isPull;
  }
  
  virtual void
  onContentStoreMiss(const Face& inFace, shared_ptr<pit::Entry> pitEntry, const Interest& interest);

//UNTIL_CODE_IS_CLEANED|#ifdef NDNSIM
//UNTIL_CODE_IS_CLEANED|  virtual void
//UNTIL_CODE_IS_CLEANED|  onOutgoingInterest(shared_ptr<pit::Entry> pitEntry, Face& outFace, bool wantNewNonce);
//UNTIL_CODE_IS_CLEANED|#endif // NDNSIM

  virtual void
  onIncomingInterest(Face& inFace, const Interest& interest);
  
  // allow Strategy (base class) to enter pipelines
  friend class fw::Strategy;

private:

  void
  updateTraceToAnchor(weak_ptr<Face> wpFace);

  void
  refreshTraceToAnchor(std::string prefix);

  void
  onFaceAdded(shared_ptr<Face> face);

  void
  onFaceRemoved(shared_ptr<Face> face);

  void
  onFibEntryChanged(shared_ptr<fib::Entry> fibEntry);

#ifdef WITH_RELIABLE_RETRANSMISSION
  void 
  sendReliableTracedInterest(weak_ptr<Face> wpFace, std::string prefix, shared_ptr<Interest> interest);
  std::map<std::string, scheduler::EventId> m_pendingRetx;
  uint32_t m_ack_seq;///< sequence number used for sending ACK interest from anchor router
#endif
   
private:  
  std::map<std::string, scheduler::EventId> m_pendingUpdates;
  std::map<std::string, uint64_t> m_seq;
  std::map<std::string, uint64_t> m_retx;
  bool m_isPull;


  
#ifdef NDNSIM
public:

  /* Callback triggered upon IU reception */
  ns3::Callback<void, std::string /* prefix */, uint64_t /* seq */, uint64_t /* ttl */, uint64_t /* num_retx */> m_onSpecialInterest;
  ns3::Callback<void, std::string /* prefix */, uint64_t /* seq */, uint64_t /* ttl */, uint64_t /* num_retx */> m_onProcessing;
#endif // NDNSIM

};

} // namespace nfd

#endif // KITE

#endif // NFD_DAEMON_FW_FORWARDER_KITE_HPP
