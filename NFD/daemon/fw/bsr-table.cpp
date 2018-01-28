/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Rozhnova Natalya <natalya.rozhnova@cisco.com
*/

#include "bsr-table.hpp"
#include <ostream>

namespace ns3 {
    namespace ndn {


BSRTable::BSRTable()
{}

void
BSRTable::ClearAll()
{
	m_BsrTable.clear();
	m_BsrRtxMap.clear();
}

void
BSRTable::InsertElement(ndn::Name prefix,
						shared_ptr<nfd::Face> outFace,
						shared_ptr<nfd::pit::Entry> pitEntry,
						uint32_t retransmissions)
{
	BSRfullTable::iterator
		mit(m_BsrTable.find(prefix)),
		mend(m_BsrTable.end());

	bool found = (mit != mend);
	if (found)
	{
		BSRsubTable & submap = mit->second;
		BSRsubTable::iterator mit1(submap.find(outFace));
		found = (mit1 != submap.end());
		if(found)
		{
			bool expected;
			BSRentryList & list = mit1->second;
			if(list.empty())
				expected = true;
			else
				expected = false;

			mit1->second.push_back(BSREntry(pitEntry,
					 	 	ns3::Simulator::Now().GetMicroSeconds(),
					 	 	expected, retransmissions));
		}
	}
	if (!found)
	{
		this->m_BsrTable[prefix][outFace].push_back(BSREntry(pitEntry,
													ns3::Simulator::Now().GetMicroSeconds(),
													!found, retransmissions));;
	}
	//////
		/*
				bool expected;


			BSRfullTable::iterator
			mit(m_BsrTable.find(prefix)),
			mend(m_BsrTable.end());
			if(mit != mend)
			{
				std::cout<<"prefix "<<mit->first<<std::endl;
				BSRsubTable::iterator
				mit1(mit->second.find(outFace)),
				mend1(mit->second.end());
				if(mit1 == mend1)
				{
					std::cout<<"HERE1"<<std::endl;
					expected = true;
					this->m_BsrTable[prefix][outFace].push_back(BSREntry(pitEntry,
																			 ns3::Simulator::Now().GetMicroSeconds(),
																			 expected, retransmissions));
				}
				else
				{
					std::cout<<"HERE2"<<std::endl;
					expected = false;
					mit1->second.push_back(BSREntry(pitEntry,
													ns3::Simulator::Now().GetMicroSeconds(),
													expected, retransmissions));
				}
			}
			else
			{
				std::cout<<"HERE3"<<std::endl;
				expected = true;
				this->m_BsrTable[prefix][outFace].push_back(BSREntry(pitEntry,
															ns3::Simulator::Now().GetMicroSeconds(),
															expected, retransmissions));
			}

			std::cout<<"Printed from BSR Table Insert Element End"<<std::endl;
			this->PrintBSRTable();
		*/
}

void
BSRTable::InsertRetransmission(ndn::Name prefix,
        							 shared_ptr<nfd::Face> outFace,
        							 BSREntry & bsr_entry)
{

	BSRfullTable::iterator
			mit(m_BsrTable.find(prefix)),
			mend(m_BsrTable.end());

		bool found = (mit != mend);
		int64_t timestamp = bsr_entry.GetTimestamp();
		bsr_entry.IncreaseNRetransmissions();
		uint32_t rtx = bsr_entry.GetNRetransmissions();

		if(rtx == 1)
		{
			timestamp = ns3::Simulator::Now().GetMicroSeconds();
		}

		if (found)
		{
			BSRsubTable & submap = mit->second;
			BSRsubTable::iterator mit1(submap.find(outFace));
			found = (mit1 != submap.end());
			if(found)
			{
				bool expected;
				BSRentryList & list = mit1->second;
				if(list.empty())
					expected = true;
				else
					expected = false;

				mit1->second.push_back(BSREntry(bsr_entry.GetPitEntry(),
						 	 		   timestamp,
						 	 		   expected, rtx));
			}
		}
		if (!found)
		{
			this->m_BsrTable[prefix][outFace].push_back(BSREntry(bsr_entry.GetPitEntry(),
														timestamp,
														!found, rtx));;
		}
}

std::list<BSREntry> *
BSRTable::GetBsrList(ndn::Name prefix,
		   	   	   	 shared_ptr<nfd::Face> outFace)
{
	BSRfullTable::iterator
	mit(m_BsrTable.find(prefix)),
	mend(m_BsrTable.end());
	if(mit != mend)
	{
		BSRsubTable::iterator
		mit1(mit->second.find(outFace)),
		mend1(mit->second.end());
		if(mit1 != mend1)
		{
			std::list<BSREntry> & bsr_list = mit1->second;
			return &bsr_list;
		}
		else
			{
				std::cout<<"BSRTable::GetBsrList : outFace "<<outFace<<"not found!"<<std::endl;
				return NULL;
			}
	}
	else
	{
		std::cout<<"BSRTable::GetBsrList : prefix "<<prefix<<" not found!"<<std::endl;
		return NULL;
	}
	return NULL;
}

BSREntry *
BSRTable::GetBsrEntry(ndn::Name prefix,
        			  shared_ptr<nfd::Face> outFace,
        			  shared_ptr<nfd::pit::Entry> pitEntry)
{
	BSRfullTable::iterator
	mit(m_BsrTable.find(prefix)),
	mend(m_BsrTable.end());
	if(mit != mend)
	{
		BSRsubTable::iterator
		mit1(mit->second.find(outFace)),
		mend1(mit->second.end());
		if(mit1 != mend1)
		{
			BSRentryList & bsr_list = mit1->second;
			BSRentryList::iterator lit(bsr_list.begin());
			for(;lit != bsr_list.end(); ++lit)
			{
				if(lit->GetPitEntry() == pitEntry)
				{
					return &(*lit);
				}
			}
			std::cout<<"BSRTable::GetBsrEntry : pitEntry "<<pitEntry<<" not found"<<std::endl;
			return NULL;
		}
		else
		{
			std::cout<<"BSRTable::GetBsrEntry : outFace "<<outFace<<"not found!"<<std::endl;
			return NULL;
		}
	}
	else
	{
		std::cout<<"BSRTable::GetBsrEntry : prefix "<<prefix<<" not found!"<<std::endl;
		return NULL;
	}
	return NULL;
}

void
BSRTable::IncreaseRetransmitted(ndn::Name prefix,
        						shared_ptr<nfd::Face> outFace,
        						shared_ptr<nfd::pit::Entry> pitEntry)
{
	BSRfullTable::iterator
		mit(m_BsrTable.find(prefix)),
		mend(m_BsrTable.end());
		if(mit != mend)
		{
			BSRsubTable::iterator
			mit1(mit->second.find(outFace)),
			mend1(mit->second.end());
			if(mit1 != mend1)
			{
				BSRentryList & bsr_list = mit1->second;
				BSRentryList::iterator lit(bsr_list.begin());
				for(;lit != bsr_list.end(); ++lit)
				{
					if(lit->GetPitEntry() == pitEntry)
					{
						lit->IncreaseNRetransmissions();
						return;
					}
				}
				std::cout<<"BSRTable::IncreaseRetransmitted : PitEntry "<<pitEntry<<" not found !"<<std::endl;
			}
			std::cout<<"BSRTable::IncreaseRetransmitted : outFace "<<outFace<<" not found !"<<std::endl;
		}
		std::cout<<"BSRTable::IncreaseRetransmitted : Prefix "<<prefix<<" not found !"<<std::endl;
}

void
BSRTable::IncreaseRetransmittedByEntry(BSREntry & bsr_entry)
{
	bsr_entry.IncreaseNRetransmissions();
}

void
BSRTable::DeleteBsrEntry(ndn::Name prefix,
						 shared_ptr<nfd::Face> outFace,
						 shared_ptr<nfd::pit::Entry> pitEntry)
{
	BSRfullTable::iterator
		mit(m_BsrTable.find(prefix)),
		mend(m_BsrTable.end());
		if(mit != mend)
		{
			BSRsubTable::iterator
			mit1(mit->second.find(outFace)),
			mend1(mit->second.end());
			if(mit1 != mend1)
			{
				BSRentryList & bsr_list = mit1->second;
				BSRentryList::iterator lit(bsr_list.begin());
				for(;lit != bsr_list.end(); ++lit)
				{
					if(lit->GetPitEntry() == pitEntry)
					{
						BSRentryList::iterator lit2(lit);
						lit++;
						if(lit != bsr_list.end())
						{
							lit->SetIfExpected(true);
						}
						bsr_list.erase(lit2);
						return;
					}
				}
				std::cout<<"BSRTable::DeleteBsrEntry : PitEntry "<<pitEntry<<" not found !"<<std::endl;
			}
			std::cout<<"BSRTable::DeleteBsrEntry : outFace "<<outFace<<" not found !"<<std::endl;
		}
		std::cout<<"BSRTable::DeleteBsrEntry : Prefix "<<prefix<<" not found !"<<std::endl;
}

std::map<shared_ptr<nfd::Face>, std::list<BSREntry> > *
BSRTable::GetInterestsToRetransmit(BSRentryList & bsr_list,
									   shared_ptr<nfd::pit::Entry> pitEntry,
									   shared_ptr<nfd::Face> outFace,
									   uint32_t rtx_allowed)
{
	BSRentryList::iterator lit(bsr_list.begin());
	for(;lit != bsr_list.end(); ++lit)
	{
		if(lit->GetPitEntry() != pitEntry)
		{
			BSRentryList::iterator lit2(lit);
			BSREntry & bsr = *lit;
			if(bsr.GetNRetransmissions() +1 <= rtx_allowed)
			{
				bsr.SetIfExpected(false);
				this->m_BsrRtxMap[outFace].push_back(bsr);
			}
			bsr_list.erase(lit2);
			lit--;
			continue;
		}
		else
		{
			BSRentryList::iterator lit2(lit);
			lit++;

			if(lit != bsr_list.end())
			{
				BSREntry * bsr = &(*lit);
				bsr->SetIfExpected(true);
			}
			bsr_list.erase(lit2);

			return &this->m_BsrRtxMap;
		}
	}
	std::cout<<"BSRTable::DeleteAndGetNextTillReceived : PitEntry "<<pitEntry<<" not found !"<<std::endl;
return NULL;
}

void
BSRTable::SetNextAsNewExpectedByEntry(BSREntry & bsr_entry,
    									BSRentryList & bsr_list)
{
	BSRentryList::iterator lit(bsr_list.begin());
		for(;lit != bsr_list.end(); ++lit)
		{
			if(*lit == bsr_entry)
			{
				BSRentryList::iterator lit2(lit);
				lit++;
				lit->SetIfExpected(true);
				bsr_list.erase(lit2);
				return;
			}
		}
		std::cout<<"BSRTable::SetNextAsNewExpectedByEntry : BSR entry "<<bsr_entry<<" not found!"<<std::endl;
	return;
}

void
BSRTable::PrintBSRTable()
{
	std::cout<<"==========BSRTable=========="<<std::endl;

	BSRfullTable::iterator
	mit(m_BsrTable.begin()),
	mend(m_BsrTable.end());
	for(;mit != mend; ++mit)
	{
		std::cout<<mit->first<<"\t";

		BSRsubTable::iterator
		mit1(mit->second.begin()),
		mend1(mit->second.end());
		for(;mit1 != mend1; ++mit1)
		{
			std::cout<<mit1->first<<"\t";

			BSRentryList & bsr_list = mit1->second;
			BSRentryList::iterator lit(bsr_list.begin());
			for(;lit != bsr_list.end(); ++lit)
			{
				const shared_ptr<nfd::pit::Entry> & seq_cur = lit->GetPitEntry();
				std::cout<<"\t\t\t\t"<<*lit<<std::endl;
			}
		}
	}
	std::cout<<"==============================="<<std::endl<<std::endl;
}

void
BSRTable::PrintBSRrtxMap(BSRsubTable & bsr_rtx_map)
{
	std::cout<<"BSR retransmission map"<<std::endl;

	BSRsubTable::iterator mit(bsr_rtx_map.begin());

	for(;mit!=bsr_rtx_map.end(); ++mit)
	{
		BSRentryList::iterator lit1(mit->second.begin());
		for(;lit1 != mit->second.end(); ++lit1)
			std::cout<<mit->first<<"\t"<<*lit1<<std::endl;
	}
	std::cout<<"==============================="<<std::endl;
}

void
BSRTable::PrintBSRlist(BSRentryList & bsr_list)
{
	std::cout<<"BSR list"<<std::endl;

	std::list<ns3::ndn::BSREntry>::iterator lit(bsr_list.begin());
	for(;lit!=bsr_list.end();++lit)
		std::cout<<*lit<<std::endl;

	std::cout<<"==============================="<<std::endl;
}

/*
 *  type * x = GetBsrList(...);
 *  if(x){
 *  	type & y = *x;
 *  	type::iterator lit(y.begin())
 *  	etc...
 */


}
}
