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
 *
 *  Author: Natalya Rozhnova <natalya.rozhnova@cisco.com>
 */

#ifndef NFD_DAEMON_FW_BSR_SCHEME_HPP
#define NFD_DAEMON_FW_BSR_SCHEME_HPP

#include "strategy.hpp"
#include "ns3/timer.h"
#include "bsr-table.hpp"

#include <map>
#include <list>

namespace nfd {
namespace fw {

class BSRScheme : public Strategy
{
public:
  BSRScheme(Forwarder& forwarder, const Name& name = STRATEGY_NAME);

  virtual
  ~BSRScheme();

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
  SendInterest(const Interest& interest,
		  	   shared_ptr<fib::Entry> fibEntry,
		  	   shared_ptr<pit::Entry> pitEntry);

  void
  RetransmitAll(std::map<shared_ptr<nfd::Face>,
		  	    std::list<ns3::ndn::BSREntry> > & bsr_rtx_map,
		  	    ndn::Name prefix);

  void
  LastPacketTimerExpire();

  void
  ConfigureTimer();

  void
  InsertToTimestampMap(ndn::Name prefix,
		  	  	  	   shared_ptr<nfd::Face> face);

  void
  PrintTimestampMap();

public:
  static const Name STRATEGY_NAME;

private:
  typedef std::map<shared_ptr<nfd::Face>, int64_t> TimestampSubMap;
  typedef std::map<ndn::Name, TimestampSubMap > TimestampMap;
  ns3::ndn::BSRTable m_bsr;
  TimestampMap m_LastTimestampMap; /* Protection against the last packet loss (normally should be per content object...)
   	   	   	   	   	   	   	   	   	   	   	   	   	   	   	   	   	   	   	* Contains the last timestamps per content object and face. Checked every m_timer_delay for expired.
   	   	   	   	   	   	   	   	   	   	   	   	   	   	   	   	   	   	   	* When expired, everything should be deleted.
   	   	   	   	   	   	   	   	   	   	   	   	   	   	   	   	   	   	   	*/
  uint64_t m_loss_count;
  uint32_t m_count_sent;
  ns3::Timer m_timer;
  int64_t m_timer_delay;
};

} // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_ORDERING_SCHEME_HPP
