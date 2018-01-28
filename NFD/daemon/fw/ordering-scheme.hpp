/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014,  Regents of the University of California,
 *                      Arizona Board of Regents,
 *                      Colorado State University,
 *                      University Pierre & Marie Curie, Sorbonne University,
 *                      Washington University in St. Louis,
 *                      Beijing Institute of Technology,
 *                      The University of Memphis
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

#ifndef NFD_DAEMON_FW_ORDERING_SCHEME_HPP
#define NFD_DAEMON_FW_ORDERING_SCHEME_HPP

#include "strategy.hpp"
#include "ns3/timer.h"

#include <map>
#include <list>

namespace nfd {
namespace fw {

/** \class OrderingScheme
 *  \brief a forwarding strategy that forwards Interest
 *         to all nexthops
 */
class OrderingScheme : public Strategy
{
public:
  OrderingScheme(Forwarder& forwarder, const Name& name = STRATEGY_NAME);

  virtual
  ~OrderingScheme();

  virtual void
  afterReceiveInterest(const Face& inFace,
                       const Interest& interest,
                       shared_ptr<fib::Entry> fibEntry,
                       shared_ptr<pit::Entry> pitEntry) DECL_OVERRIDE;

  virtual void
  beforeSatisfyInterest(shared_ptr<pit::Entry> pitEntry,
		  	  	  	    const Face& inFace,
		  	  	  	    const Data& data);

  void
  InsertInterestOrder(const Interest& interest, shared_ptr<pit::Entry> pitEntry);

  void
  SetTimestampForRetransmitted(shared_ptr<pit::Entry> pitEntry);

  uint64_t
  GetTimestamp(shared_ptr<pit::Entry>);

  bool
  DeleteTimestamp(shared_ptr<pit::Entry> entry);

  bool
  NormalOrdering(ndn::Name name, shared_ptr<pit::Entry> pitEntry);

  void
  SetNewExpected(ndn::Name name, shared_ptr<pit::Entry> pitEntry);

  bool
  IsRetransmission(ndn::Name name, shared_ptr<pit::Entry> pitEntry);

  shared_ptr<pit::Entry>
  GetExpected(ndn::Name name);

  void
  RetransmitAllTillReceived(ndn::Name name, shared_ptr<pit::Entry> pitEntry, int32_t n);

  void
  RetransmitInterest(shared_ptr<pit::Entry> pitEntry);

  void
  InterestToRetransmit(bool val);

  bool
  GetInterestToRetransmit();

  void
  SendInterest(const Interest& interest,
  		shared_ptr<fib::Entry> fibEntry,
  		shared_ptr<pit::Entry> pitEntry);

  void
  PrintOrderingMap();

  void
  PrintExpectedMap();

  void
  PrintTimestampMap();

  virtual void
  ClearAllWithPrefix(const ndn::Name& name);

  void
  SetNumberOfRetransmissions(uint32_t num);

  void
  LastPacketTimerExpire(ndn::Name prefix);

  void
  ConfigureTimer(ndn::Name prefix);

public:
  static const Name STRATEGY_NAME;

private:
//  typedef std::list<double> TimestampList;
//  typedef std::map<ndn::Name, TimestampList> TimestampMap;
  typedef std::map<shared_ptr<pit::Entry>, uint64_t> TimestampMap;
//  typedef std::list<shared_ptr<pit::Entry> > SendList;
//  typedef std::map<ndn::Name, SendList> SendingMap;
  typedef std::pair<shared_ptr<pit::Entry>, uint32_t> PitRtxPair;
  typedef std::list<PitRtxPair> SendMapWithRTXNo;
  typedef std::map<ndn::Name, SendMapWithRTXNo> SendingMap;


//  typedef std::map<ndn::Name, ndn::Name> NameMap;
  typedef std::map<ndn::Name, shared_ptr<pit::Entry> > NameMap;


  SendingMap m_SendingOrder;				// Order of sent interests [Prefix][pitEntry][RTX #]
  NameMap m_ExpectedChunk;					// Expected Chunk that should be received [Prefix][CompleteName]
  TimestampMap m_SendingTimestamps;
  bool m_retransmit;
  uint64_t m_loss_count;
  uint32_t m_WirelessQueueSize;
  uint32_t m_PrevQueueSize;
  int32_t m_QueueEvolution;
  uint32_t m_count_sent;
  uint32_t m_count_queue;
  uint32_t m_QueueSizeSum;
  uint32_t m_MaxRtxAllowed;
  ns3::Timer m_timer; 							// protection against the last packet loss (normally should be per content object...)
  double m_timer_delay;
};

} // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_ORDERING_SCHEME_HPP
