/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#ifndef NDN_PRODUCER_H
#define NDN_PRODUCER_H

#include "ns3/ndnSIM/model/ndn-common.hpp"

#include "ndn-app.hpp"
#include "ns3/ndnSIM/model/ndn-common.hpp"

#include "ns3/nstime.h"
#include "ns3/ptr.h"

#include "ns3/random-variable-stream.h"



/* 
 * the ndn-producer code is modified by xuan zeng (xuan.zeng@irt-systemx.fr)
 * 
 * 
 * The following two Macros definations are used to enable/disable some metrics
 * computation for producer management protocols.
 * 
 * In particular, the 2 macros below are used to enable/disable
 * 1) handover latency computation 2) path stretch computation, respectively.
 * You can comment/uncomment them to enable/disable the desired functionality.
 */
#define WITH_HANDOVER_TRACING 1
#define STRETCH_P 1

namespace ns3 {
namespace ndn {
  
#ifdef WITH_HANDOVER_TRACING
class producerHandoverStats
{
public:
    static std::string getHeader();
    std::string toString();
public:
  double l3_handoff_time;
  unsigned first_interest_after_association_seq;
  unsigned last_interest_before_disassociation_seq;
  double producer_handoff_delay;
  
};
#endif

/**
 * @ingroup ndn-apps
 * @brief A simple Interest-sink applia simple Interest-sink application
 *
 * A simple Interest-sink applia simple Interest-sink application,
 * which replying every incoming Interest with Data packet with a specified
 * size and name same as in Interest.cation, which replying every incoming Interest
 * with Data packet with a specified size and name same as in Interest.
 */
class Producer : public App {
public:
  static TypeId
  GetTypeId(void);

  Producer();

  // inherited from NdnApp
  virtual void
  OnInterest(shared_ptr<const Interest> interest);

  void
  onAttachtoNewBS();
  
#ifdef WITH_HANDOVER_TRACING
void setlogging();
#endif

protected:
  // inherited from Application base class.
  virtual void
  StartApplication(); // Called at time specified by Start

  virtual void
  StopApplication(); // Called at time specified by Stop

private:
  Name m_prefix;
  Name m_postfix;
  uint32_t m_virtualPayloadSize;
  Time m_freshness;

  uint32_t m_signature;
  Name m_keyLocator;
  
   EventId m_retxEvent; ///< @brief EventId of pending "send packet" event
  static const Time m_retxTimer;    ///< @brief Currently estimated retransmission timer
  
  static const Name PREFIX_ANNOUNCEMENT_PREFIX;

  
    void 
    sendAnnounceInterest(shared_ptr<Interest> interest);
    
    int m_seq;

#ifdef KITE
public:
  void
  setUpTraceToAnchor();
private:
  EventId m_updateTraceEvent;
  static const Time TRACE_UPDATE_INTERVAL;
  bool m_kiteEnabled;
  Ptr<UniformRandomVariable> m_rand;
#endif // KITE
  
  
#ifdef WITH_HANDOVER_TRACING
  void onAPassociation(std::string context, double time, char type, Ptr<Node> sta, Ptr<Node> ap);
  double m_last_association_time;
  double m_last_received_interest_time;
  unsigned m_last_received_interest_seq;
  TracedCallback<producerHandoverStats &> m_HandoverStatsTrace;
#endif

#ifdef BUGFIXES // XXX XXX XXX
public:
  Name &
  GetPrefix() { return m_prefix; }
#endif // BUGFIXES

};



} // namespace ndn
} // namespace ns3

#endif // NDN_PRODUCER_H
