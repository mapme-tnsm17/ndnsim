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
 * Author: Natalya Rozhnova <natalya.rozhnova@cisco.com>
 *
 */
#include "bsr-scheme.hpp"

//#include <sys/time.h>


#include "ns3/ndnSIM/model/ndn-net-device-face.hpp"
#include "ns3/ptr.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-mac-queue.h"
#include "ns3/adhoc-wifi-mac.h"
#include "ns3/pointer.h"
#include "table/pit-in-record.hpp"
#include "table/pit-entry.hpp"
#include "ns3/data-rate.h"
#include "ns3/simulator.h"
#include "utils/ndn-ns3-packet-tag.hpp"
#include "utils/rtt-tag.hpp"

//#include "ns3/point-to-point-net-device.h"
//#include "ns3/queue.h"


namespace nfd {
namespace fw {

NFD_LOG_INIT("BSRScheme");
const Name BSRScheme::STRATEGY_NAME("ndn:/localhost/nfd/strategy/bsrscheme");
NFD_REGISTER_STRATEGY(BSRScheme);

BSRScheme::BSRScheme(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder, name)
  , m_loss_count(0)
  , m_count_sent(0)
  , m_timer_delay(1000000) // in microseconds
{
//	ConfigureTimer();
}

BSRScheme::~BSRScheme()
{
	m_bsr.ClearAll();
	if(m_timer.IsRunning())
		m_timer.Remove();
}

static inline bool
predicate_PitEntry_canForwardTo_NextHop(shared_ptr<pit::Entry> pitEntry,
                                        const fib::NextHop& nexthop)
{
  return pitEntry->canForwardTo(*nexthop.getFace());
}


void
BSRScheme::afterReceiveInterest(const Face& inFace,
                   const Interest& interest,
                   shared_ptr<fib::Entry> fibEntry,
                   shared_ptr<pit::Entry> pitEntry)
{
//	std::cout<<"Got Interest"<<this->m_MaxRtxAllowed<<std::endl;

	if (pitEntry->hasUnexpiredOutRecords())
	{
		// not a new Interest, don't forward
		return;
	}

	this->SendInterest(interest, fibEntry, pitEntry);

}

void
BSRScheme::SendInterest(const Interest& interest,
		shared_ptr<fib::Entry> fibEntry,
		shared_ptr<pit::Entry> pitEntry)
{
	const fib::NextHopList& nexthops = fibEntry->getNextHops();

	fib::NextHopList::const_iterator it = std::find_if(nexthops.begin(), nexthops.end(),
			bind(&predicate_PitEntry_canForwardTo_NextHop, pitEntry, _1));

	if (it == nexthops.end()) {
		this->rejectPendingInterest(pitEntry);
		return;
	}

	shared_ptr<Face> outFace = it->getFace();
	this->sendInterest(pitEntry, outFace);

	if(outFace->GetWirelessFace())
	{
		m_bsr.InsertElement(interest.getName().getPrefix(-1), outFace, pitEntry, 0);
	}
	InsertToTimestampMap(interest.getName().getPrefix(-1), outFace);
	ConfigureTimer();
}

void
BSRScheme::beforeSatisfyInterest(shared_ptr<pit::Entry> pitEntry,
	  	    const Face& inFace,
	  	    const Data& data)
{
	//Check whether the gotten data is the expected one
	if(inFace.GetWirelessFace())
	{
		ns3::ndn::BSREntry * bsr_entry = m_bsr.GetBsrEntry(data.getName().getPrefix(-1), const_pointer_cast<Face>(inFace.shared_from_this()), pitEntry);

		if(bsr_entry->GetIfExpected())
		{
			if(bsr_entry->GetNRetransmissions() > 0)
			{
				// Setting a RTT tag to reduce the RTT on consumer side
				int64_t timestamp = bsr_entry->GetTimestamp();
				uint64_t wireless_rtt = ns3::Simulator::Now().GetMicroSeconds() - timestamp;

				// Now set tag with wireless rtt value
				auto ns3PacketTag = data.getTag<ns3::ndn::Ns3PacketTag>();
				if (ns3PacketTag != nullptr)
				{
					ns3::ndn::RTTTag rtt_tag;
					rtt_tag.SetLabel(wireless_rtt);
					ns3PacketTag->getPacket()->AddPacketTag(rtt_tag);
				}
				else
					std::cout<<"NO TAG"<<std::endl;
			}

			m_bsr.DeleteBsrEntry(data.getName().getPrefix(-1), const_pointer_cast<Face>(inFace.shared_from_this()), pitEntry);
		}
		else
		{
			std::list<ns3::ndn::BSREntry> * bsr_list = m_bsr.GetBsrList(data.getName().getPrefix(-1), const_pointer_cast<Face>(inFace.shared_from_this()));

			std::map<shared_ptr<nfd::Face>, std::list<ns3::ndn::BSREntry> > * bsr_rtx_map = m_bsr.GetInterestsToRetransmit(*bsr_list, pitEntry,
																											const_pointer_cast<Face>(inFace.shared_from_this()),
																											this->m_MaxRtxAllowed);

			if(!bsr_rtx_map->empty())
				RetransmitAll(*bsr_rtx_map, data.getName().getPrefix(-1));
		}
	}

	ConfigureTimer();

//	m_count_sent++;
//	std::cout<<"sent "<<m_count_sent<<" "<<pitEntry<<std::endl;
}

void
BSRScheme::RetransmitAll(std::map<shared_ptr<nfd::Face>, std::list<ns3::ndn::BSREntry> > & bsr_rtx_map, ndn::Name prefix)
{
	std::map<shared_ptr<nfd::Face>, std::list<ns3::ndn::BSREntry> >::iterator mit(bsr_rtx_map.begin());
	std::list<ns3::ndn::BSREntry> & bsr_rtx_list = mit->second;
	shared_ptr<nfd::Face> outFace = mit->first;

	std::list<ns3::ndn::BSREntry>::iterator lit(bsr_rtx_list.begin());
		for(;lit != bsr_rtx_list.end(); ++lit)
		{
			std::list<ns3::ndn::BSREntry>::iterator lit2(lit);
			shared_ptr<pit::Entry> pitEntry = lit->GetPitEntry();
			this->sendInterest(pitEntry, outFace);
			m_bsr.InsertRetransmission(prefix, outFace, *lit);
			bsr_rtx_list.erase(lit2);
			lit--;
			InsertToTimestampMap(prefix, outFace);
		}

		ConfigureTimer();

}

void
BSRScheme::ConfigureTimer()
{
	if(!m_timer.IsRunning() && !m_LastTimestampMap.empty())
	{
		m_timer.SetFunction(&BSRScheme::LastPacketTimerExpire, this);
		m_timer.SetDelay(ns3::MicroSeconds(m_timer_delay));
		m_timer.Schedule();
	}
	if(m_LastTimestampMap.empty())
	{
		m_timer.Remove();
	}

}

void
BSRScheme::LastPacketTimerExpire()
{
   /* Check for expired objects and clear the corresponding list
	* (in practice it is done with PIT timers, so the problem does not exist. But as the BSRTable is a separate structure, we should clear it somehow at the end)
	* Actually the timer delay is set to 1s by default. Normally it should be something around the Interest lifetime to be sure that the packet is expired)
	*/

	TimestampMap::iterator mit(this->m_LastTimestampMap.begin());
	for(; mit != m_LastTimestampMap.end(); ++mit)
	{
		TimestampSubMap & submap = mit->second;
		TimestampSubMap::iterator mit1(submap.begin());
		for(; mit1 != submap.end(); ++mit1)
		{
			int64_t expired = ns3::Simulator::Now().GetMicroSeconds() - mit1->second;
			if(expired >= this->m_timer_delay)
			{
				std::cout<<"Expired packet"<<std::endl;
				m_bsr.PrintBSRTable();
				ndn::Name expired_prefix = mit->first;
				shared_ptr<nfd::Face> expired_face = mit1->first;
				std::list<ns3::ndn::BSREntry> * bsr_list = m_bsr.GetBsrList(expired_prefix, expired_face);
				if(bsr_list)
					bsr_list->clear();
				TimestampSubMap::iterator mit1_copy(mit1);
				submap.erase(mit1_copy);
				mit1--;
			}
			if(submap.empty())
				break;
		}
		if(submap.empty())
		{
			TimestampMap::iterator mit_copy(mit);
			m_LastTimestampMap.erase(mit_copy);
			mit--;
		}
		if(m_LastTimestampMap.empty())
			break;
	}
// If there are still some entries in the BSRTable: reschedule the timer
	ConfigureTimer();
}

void
BSRScheme::InsertToTimestampMap(ndn::Name prefix, shared_ptr<nfd::Face> face)
{
	TimestampMap::iterator mit(m_LastTimestampMap.find(prefix));
	if(mit != m_LastTimestampMap.end())
	{
		TimestampSubMap & submap = mit->second;
		TimestampSubMap::iterator mit1(submap.find(face));
		if(mit1 != submap.end())
		{
			mit1->second = ns3::Simulator::Now().GetMicroSeconds();
			return;
		}
		else
			submap[face] = ns3::Simulator::Now().GetMicroSeconds();
	}
	else
	{
		m_LastTimestampMap[prefix][face] = ns3::Simulator::Now().GetMicroSeconds();
	}
}

void
BSRScheme::PrintTimestampMap()
{
	std::cout<<"TimestampMap============================="<<std::endl;

	TimestampMap::iterator mit(this->m_LastTimestampMap.begin());
	for(; mit != this->m_LastTimestampMap.end(); ++mit)
	{
		std::cout<<mit->first<<"\t";
		TimestampSubMap & submap = mit->second;
		TimestampSubMap::iterator mit1(submap.begin());
		for(; mit1 != submap.end(); ++mit1)
		{
			std::cout<<mit1->first<<"\t"<<mit1->second<<std::endl;
		}
	}

	std::cout<<"========================================="<<std::endl;

}

} // namespace fw
} // namespace nfd
