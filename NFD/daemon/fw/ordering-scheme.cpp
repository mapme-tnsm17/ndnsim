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
#include "ordering-scheme.hpp"

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

NFD_LOG_INIT("OrderingScheme");
const Name OrderingScheme::STRATEGY_NAME("ndn:/localhost/nfd/strategy/ordering");
NFD_REGISTER_STRATEGY(OrderingScheme);

OrderingScheme::OrderingScheme(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder, name)
  , m_retransmit(false)
  , m_loss_count(0)
  , m_WirelessQueueSize(0)
  , m_PrevQueueSize(0)
  , m_QueueEvolution(0)
  , m_count_sent(0)
  , m_count_queue(1)
  , m_MaxRtxAllowed(3)
  , m_timer_delay(1.0)
{
}

OrderingScheme::~OrderingScheme()
{
}

static inline bool
predicate_PitEntry_canForwardTo_NextHop(shared_ptr<pit::Entry> pitEntry,
                                        const fib::NextHop& nexthop)
{
  return pitEntry->canForwardTo(*nexthop.getFace());
}


void
OrderingScheme::afterReceiveInterest(const Face& inFace,
                   const Interest& interest,
                   shared_ptr<fib::Entry> fibEntry,
                   shared_ptr<pit::Entry> pitEntry)
{
	if (pitEntry->hasUnexpiredOutRecords())
	{
		// not a new Interest, don't forward
		return;
	}

	this->SendInterest(interest, fibEntry, pitEntry);
}

void
OrderingScheme::ClearAllWithPrefix(const ndn::Name& name)
{
	std::cout<<"Clear before"<<std::endl;
	this->PrintExpectedMap();
	this->PrintOrderingMap();
	this->PrintTimestampMap();

	this->m_SendingOrder.erase(name.getPrefix(-1));
	this->m_ExpectedChunk.erase(name.getPrefix(-1));
//	this->m_SendingTimestamps.erase(name);

	std::cout<<"Clear after"<<std::endl;
	this->PrintExpectedMap();
	this->PrintOrderingMap();
	this->PrintTimestampMap();

}

void
OrderingScheme::SendInterest(const Interest& interest,
		shared_ptr<fib::Entry> fibEntry,
		shared_ptr<pit::Entry> pitEntry)
{
//	std::cout<<"SendInterest"<<std::endl;

	const fib::NextHopList& nexthops = fibEntry->getNextHops();

	if(!this->GetInterestToRetransmit())
	{
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
			this->InsertInterestOrder(interest, pitEntry);
		}
	}
	else
	{
//		std::cout<<"Retransmission! Send Interest "<<pitEntry<<" fibEntry "<<fibEntry<<std::endl;

		for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it)
		{
			shared_ptr<Face> outFace = it->getFace();

			this->sendInterest(pitEntry, outFace);

			break;
			//return;
		}
	}

	ConfigureTimer(interest.getName().getPrefix(-1));
}

void
OrderingScheme::beforeSatisfyInterest(shared_ptr<pit::Entry> pitEntry,
	  	    const Face& inFace,
	  	    const Data& data)
{
	//Here we should put the data management in order to check whether the gotten data is the expected one
	if(inFace.GetWirelessFace())
	{
		//get BSREntry, check the expected flag for normal ordering, check # of rtx for retransmission info and use the timestamp to compute the RTT
		if(this->NormalOrdering(data.getName(), pitEntry))
		{
			if(this->IsRetransmission(data.getName(), pitEntry))
			{
				// Setting a RTT tag to reduce the RTT on consumer side

				uint64_t timestamp = this->GetTimestamp(pitEntry);
				uint64_t wireless_rtt = ns3::Simulator::Now().GetMicroSeconds() - timestamp;

				// Now set tag with wireless rtt value
				auto ns3PacketTag = data.getTag<ns3::ndn::Ns3PacketTag>();
				if (ns3PacketTag != nullptr)
				{
					ns3::ndn::RTTTag rtt_tag;
					rtt_tag.SetLabel(wireless_rtt);
					ns3PacketTag->getPacket()->AddPacketTag(rtt_tag);
//					std::cout<<"Time "<<ns3::Simulator::Now().GetMicroSeconds()<<" RTT "<<wireless_rtt<<" timestamp "<<timestamp<<" Packet "<<data.getName()<<std::endl;
				}
				else
					std::cout<<"NO TAG"<<std::endl;
			}

			this->SetNewExpected(data.getName(), pitEntry);
		}
		else
		{
//			if(this->m_MaxRtxAllowed != 0)
				this->RetransmitAllTillReceived(data.getName(), pitEntry, 0 );
		}
	}

	ConfigureTimer(data.getName().getPrefix(-1));

//	m_count_sent++;
//	std::cout<<"sent "<<m_count_sent<<" "<<pitEntry<<std::endl;
}

void
OrderingScheme::ConfigureTimer(ndn::Name prefix)
{
	if(m_timer.IsRunning())
		m_timer.Remove();

	m_timer.SetFunction(&OrderingScheme::LastPacketTimerExpire, this);
	m_timer.SetArguments(prefix);
	m_timer.SetDelay(ns3::Seconds(m_timer_delay));
	m_timer.Schedule();
}

void
OrderingScheme::LastPacketTimerExpire(ndn::Name prefix)
{
	std::cout<<"OrderingScheme : Timer expired! "<<std::endl;

	SendingMap::iterator
		mit(this->m_SendingOrder.find(prefix)),
		mend(this->m_SendingOrder.end());
		if(mit != mend)
		{
			SendMapWithRTXNo & seq_list = mit->second;
			std::cout<<"List size "<<seq_list.size()<<std::endl;
			this->m_loss_count += seq_list.size();
			std::cout<<ns3::Simulator::Now().GetSeconds()<<" -wlft "<<m_loss_count<<std::endl;
		}

	this->m_SendingOrder.erase(prefix);
	this->m_ExpectedChunk.erase(prefix);

}

void
OrderingScheme::InsertInterestOrder(const Interest& interest, shared_ptr<pit::Entry> pitEntry)
{
//	std::cout<<"InsertInterestOrder"<<std::endl;

	if(this->m_SendingOrder.empty() || this->m_ExpectedChunk.empty())
		this->m_ExpectedChunk[interest.getName().getPrefix(-1)] = pitEntry;

	this->m_SendingOrder[interest.getName().getPrefix(-1)].push_back(PitRtxPair(pitEntry, 0));

	//TODO PRINT TIMESTAMPS!!!
}

void
OrderingScheme::SetTimestampForRetransmitted(shared_ptr<pit::Entry> pitEntry)
{
	this->m_SendingTimestamps[pitEntry] = ns3::Simulator::Now().GetMicroSeconds();
}

uint64_t
OrderingScheme::GetTimestamp(shared_ptr<pit::Entry> pitEntry)
{
	TimestampMap::iterator
		tit(this->m_SendingTimestamps.find(pitEntry)),
		tend(this->m_SendingTimestamps.end());
	if(tit != tend)
	{
		uint64_t timestamp = tit->second;
		this->m_SendingTimestamps.erase(tit);
		return timestamp;
	}
	return 0.0;
}

bool
OrderingScheme::DeleteTimestamp(shared_ptr<pit::Entry> pitEntry)
{
	TimestampMap::iterator
	tit(this->m_SendingTimestamps.find(pitEntry)),
	tend(this->m_SendingTimestamps.end());
	if(tit != tend)
	{
		this->m_SendingTimestamps.erase(tit);
		return true;
	}
	if(this->m_MaxRtxAllowed != 0)
	{
		std::cout<<"Hummm... You try to delete a timestamp that doesn't exist"<<std::endl;
		exit(404);
	}
return false;
}

// To check whether the arrived data packet is the expected one
bool
OrderingScheme::NormalOrdering(ndn::Name name, shared_ptr<pit::Entry> pitEntry)
{
//	std::cout<<"NormalOrdering "<<pitEntry<<std::endl;

	if(this->m_SendingOrder[name.getPrefix(-1)].empty())
	{
		std::cout<<"Hummm... It seems that you haven't sent any Interest but got the Data packets..."<<std::endl;
		exit(1);
	}

	NameMap::iterator
		mit(this->m_ExpectedChunk.find(name.getPrefix(-1))),
		mend(this->m_ExpectedChunk.end());
	if(mit != mend)
	{
		if(mit->second == pitEntry)
			return true;
	}
	else
		{
			std::cout<<"Hummmm, it seems that you are not waiting for any Chunk but got it..."<<std::endl;
			exit(2);
		}

	return false;
}

void
OrderingScheme::SetNewExpected(ndn::Name name, shared_ptr<pit::Entry> pitEntry)
{
//	std::cout<<"SetNewExpected"<<std::endl;

	bool found=false;
	SendingMap::iterator
	mit(this->m_SendingOrder.find(name.getPrefix(-1))),
	mend(this->m_SendingOrder.end());
	if(mit != mend)
	{
		SendMapWithRTXNo & seq_list = mit->second;
		SendMapWithRTXNo::iterator lit  = seq_list.begin(),
				lend = seq_list.end();
		for(;lit != lend; ++lit)
		{
			if((*lit).first == pitEntry)
			{
				found = true;
			}
			if (found)
			{
				SendMapWithRTXNo::iterator lit2(lit);
				lit++;
				if (lit == seq_list.end())
				{
					// If no next
					NameMap::iterator
					expit(this->m_ExpectedChunk.find(name.getPrefix(-1))),
					expend(this->m_ExpectedChunk.end());
					if(expit != expend)
					{
						this->m_ExpectedChunk.erase(expit);
					}
					// Should we do anything if no next ?
				}
				else
				{
					//set new expected
					this->m_ExpectedChunk[name.getPrefix(-1)] = (*lit).first;
				}
				seq_list.erase(lit2);
				break;
			}
		}
	}
}

bool
OrderingScheme::IsRetransmission(ndn::Name name, shared_ptr<pit::Entry> pitEntry)
{
//	std::cout<<"IsRetransmission"<<std::endl;

	SendingMap::iterator mit(this->m_SendingOrder.find(name.getPrefix(-1))),
			mend(this->m_SendingOrder.end());
	if(mit != mend)
	{
		SendMapWithRTXNo & seq_list = mit->second;
		SendMapWithRTXNo::iterator lit  = seq_list.begin();
		for(;lit != seq_list.end(); ++lit)
		{
			if((*lit).first == pitEntry)
			{
				uint32_t & rtx = (*lit).second;
				if(rtx > 0)
					return true;
			}
		}
	}
	return false;
}

shared_ptr<pit::Entry>
OrderingScheme::GetExpected(ndn::Name name)
{
//	std::cout<<"GetExpected"<<std::endl;

	NameMap::iterator
	mit(this->m_ExpectedChunk.find(name.getPrefix(-1))),
	mend(this->m_ExpectedChunk.end());
	if(mit != mend)
	{
		return mit->second;
	}
	else
		{
			std::cout<<"Hummmm, the list of expected chunks is empty..."<<std::endl;
			exit(3);
		}
	return 0;
}
//To retransmit all packets from expected to received
void
OrderingScheme::RetransmitAllTillReceived(ndn::Name name, shared_ptr<pit::Entry> pitEntry, int32_t n)
{
	shared_ptr<pit::Entry> expected = GetExpected(name);
	bool expected_found = false;

	SendingMap::iterator
	mit(this->m_SendingOrder.find(name.getPrefix(-1))),
	mend(this->m_SendingOrder.end());
	if(mit != mend)
	{
		SendMapWithRTXNo & seq_list = mit->second;
		SendMapWithRTXNo::iterator lit;
		for (lit = seq_list.begin(); lit != seq_list.end(); ++lit)
		{
			const shared_ptr<pit::Entry> & seq_cur = (*lit).first;
			if ((seq_cur == expected || (expected_found == true && seq_cur != pitEntry)))
			{
//				std::cout<<"RetransmitAllTillReceived: seq_cur "<<seq_cur<<" pitEntry "<<pitEntry<<" RTX flag "<<lit->second<<std::endl;
				if((*lit).second < m_MaxRtxAllowed)
				{
					const shared_ptr<pit::Entry> entry = seq_cur;
					uint32_t rtx = (*lit).second;
					RetransmitInterest(seq_cur);
					if(rtx == 0)
						this->SetTimestampForRetransmitted(seq_cur);

					SendMapWithRTXNo::iterator lit2(lit);
					seq_list.erase(lit2);// !!! delete and put it again at the end
					lit--;

					rtx++;
					seq_list.push_back(PitRtxPair(entry, rtx));
				}
				else
				{
					this->DeleteTimestamp(seq_cur);

					SendMapWithRTXNo::iterator lit2(lit);
					seq_list.erase(lit2);
					lit--;
					// count wireless losses
					m_loss_count++;
					std::cout<<ns3::Simulator::Now().GetSeconds()<<" -wlf "<<m_loss_count<<std::endl;
				}
				expected_found = true;
				continue;
			}
			if(seq_cur == pitEntry)
			{
				this->SetNewExpected(name, pitEntry);
				break;
			}
		}
	}
}

void
OrderingScheme::RetransmitInterest(shared_ptr<pit::Entry> pitEntry)
{
//	std::cout<<"RetransmitInterest"<<std::endl;

	const Interest& rtx = pitEntry->getInterest();

//	std::cout<<"pitEntry "<<pitEntry<<" Expired "<<pitEntry->hasUnexpiredOutRecords()/*<<" record "<<record*/<<std::endl;
	shared_ptr<fib::Entry> fibEntry =this->getFIB().findLongestPrefixMatch(*pitEntry);

	if(!static_cast<bool>(fibEntry))
	{
		std::cout<<this<<" Cannot find matching fib entry"<<std::endl;
		return;
	}

	this->InterestToRetransmit(true);
	this->SendInterest(rtx, fibEntry, pitEntry);
	this->InterestToRetransmit(false);

//	m_loss_count++;
//	std::cout<<"loss_count "<<m_loss_count<<std::endl;
}

void
OrderingScheme::InterestToRetransmit(bool val)
{
//	std::cout<<"InterestToRentransmit"<<std::endl;

	m_retransmit = val;
}

bool
OrderingScheme::GetInterestToRetransmit()
{
	return m_retransmit;
}

void
OrderingScheme::SetNumberOfRetransmissions(uint32_t num)
{
	m_MaxRtxAllowed = num;
}

void
OrderingScheme::PrintOrderingMap()
{
	std::cout<<"==========OrderingMap=========="<<std::endl;
	SendingMap::iterator
	mit(this->m_SendingOrder.begin()),
	mend(this->m_SendingOrder.end());
	for(;mit != mend; ++mit)
	{
		std::cout<<mit->first<<std::endl;
		SendMapWithRTXNo & seq_list = mit->second;
		SendMapWithRTXNo::iterator lit  = seq_list.begin();
		for(;lit != seq_list.end(); ++lit)
		{
			const shared_ptr<pit::Entry> & seq_cur = (*lit).first;
			std::cout<<"\t\t"<<seq_cur<<"\t"<<(*lit).second<<std::endl;
		}
	}

	std::cout<<"==============================="<<std::endl<<std::endl;
}

void
OrderingScheme::PrintExpectedMap()
{
	std::cout<<"==========ExpectedMap=========="<<std::endl;
	NameMap::iterator
		mit(this->m_ExpectedChunk.begin()),
		mend(this->m_ExpectedChunk.end());
	if(mit != mend)
	{
		std::cout<<mit->first<<"\t"<<mit->second<<std::endl;
	}
	std::cout<<"==============================="<<std::endl<<std::endl;
}

void
OrderingScheme::PrintTimestampMap()
{
	std::cout<<"==========TimestampMap=========="<<std::endl;
	TimestampMap::iterator
		mit(this->m_SendingTimestamps.begin()),
		mend(this->m_SendingTimestamps.end());
	if(mit != mend)
	{
		std::cout<<mit->first<<"\t"<<mit->second<<std::endl;
	}
	std::cout<<"==============================="<<std::endl<<std::endl;
}

} // namespace fw
} // namespace nfd
