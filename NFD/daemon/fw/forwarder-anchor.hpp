#ifndef NFD_DAEMON_FW_FORWARDER_ANCHOR_HPP
#define NFD_DAEMON_FW_FORWARDER_ANCHOR_HPP

#ifdef ANCHOR

#include "forwarder.hpp"
#include "table/strategy-choice.hpp"
#include "table/fib-entry.hpp"
#include "daemon/face/face.hpp"

#define ANCHOR_ORIGIN_STR "/Origin"
#define ANCHOR_LOCATOR_STR "/BaseStation"
#define ANCHOR_ANNOUNCE_STR "/Announcement"
#define ANCHOR_UPDATE_STR "/LocationUpdate"
#define ANCHOR_DEFAULT_RETX 20 /* ms */

namespace nfd {

class ForwarderAnchor : public Forwarder
{
public:
  ForwarderAnchor();

private:
  void
  onFaceAdded(shared_ptr<Face> face);

  bool
  isFaceValidAndUp(shared_ptr<Face> face);

  void
  sendSpecialInterest(Name prefix, weak_ptr<fib::Entry> wpFibEntry, FaceId faceId, uint64_t num_retx = 0);

#ifdef NDNSIM
  virtual void
  onOutgoingInterest(shared_ptr<pit::Entry> pitEntry, Face& outFace, bool wantNewNonce);
#endif // NDNSIM

  // allow Strategy (base class) to enter pipelines
  friend class fw::Strategy;

protected:
  uint64_t m_retx;

private:
  std::map<Name, scheduler::EventId> m_pendingUpdates;

PUBLIC_WITH_TESTS_ELSE_PROTECTED:
  VIRTUAL_WITH_TESTS void
  onIncomingData(Face& inFace, const Data& data);

#ifdef NDNSIM
  // XXX For all similar methods in all forwarder: should it be PUBLIC_WITH_TESTS_ELSE_PROTECTED ?
public:

  /* Callback triggered upon IU reception */
  ns3::Callback<void, std::string /* prefix */, uint64_t /* seq */, uint64_t /* ttl */, uint64_t /* num_retx */> m_onSpecialInterest;
  ns3::Callback<void, std::string /* prefix */, uint64_t /* seq */, uint64_t /* ttl */, uint64_t /* num_retx */> m_onProcessing;
#endif // NDNSIM

};

} // namespace nfd

#endif // ANCHOR

#endif // NFD_DAEMON_FW_FORWARDER_ANCHOR_HPP
