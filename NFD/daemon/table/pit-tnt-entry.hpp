/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2015 SystemX
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

#ifndef NFD_DAEMON_TABLE_PIT_TNT_ENTRY_HPP
#define NFD_DAEMON_TABLE_PIT_TNT_ENTRY_HPP

#ifdef KITE

#include "fw/strategy-info.hpp"
#include "face/face.hpp"
#include "pit-entry.hpp"
#include<boost/unordered_map.hpp> 

#include "pit-tnt-record.hpp"

/**
 *   \brief a wrapper class for a list of Tracing Pit Entries. The wrapping is for storing the data structure
 *   
 */ 

namespace nfd {
  namespace pit {
    class TntEntry: public fw::StrategyInfo
{
  public:
    
    TntEntry(const Name& tracingName);
    
    TntEntry();
    
    static constexpr int
    getTypeId()
    {
      return 135246;
    }
  
    typedef boost::unordered_map<std::string, TntRecord> TracingInterestList;
    
    const TracingInterestList&
    getTracingInterests() const;
    
    void
    addTracingRecord(weak_ptr<const Interest> interestWeak
                    ,weak_ptr<Face> inFaceWeak
                    ,weak_ptr<pit::Entry> pitEntryWeek);
    void
    removeTracingRecord(TracingInterestList::const_iterator it);

   
  private:
    TracingInterestList m_tracingInterests;
    Name m_tracingName;
   
  }; 
  
inline 
TntEntry::TntEntry(const Name& tracingName)
                  :m_tracingName(tracingName)
{
}

inline 
TntEntry::TntEntry()
{
  
}

 
inline const TntEntry::TracingInterestList&
TntEntry::getTracingInterests() const
{
  return m_tracingInterests;
}

inline void
TntEntry::removeTracingRecord(TracingInterestList::const_iterator it)
{
  m_tracingInterests.erase(it);
}

inline void
TntEntry::addTracingRecord(weak_ptr<const Interest> interestWeak
                    ,weak_ptr<Face> inFaceWeak
                    ,weak_ptr<pit::Entry> pitEntryWeek)
{
  shared_ptr<const Interest> interest=interestWeak.lock();
  if(!static_cast<bool>(interest) || !static_cast<bool>(pitEntryWeek.lock()) )//not valid record
  {
    std::cerr<<"   invalid pointers, tracing record is not added\n";
    return;
  }
  
  if(interest->getTraceName()!=m_tracingName)//trace name not match
  {
    std::cerr<<"tntEntry_tracingName=!=record_traceName, tracing record is not added"<<"\n";
    return;
  } 

  std::string InterestNameString=interest->getName().toUri();
  time::steady_clock::TimePoint newExpiry=time::steady_clock::now()+interest->getInterestLifetime();
  
  
  if(m_tracingInterests.find(interest->getName().toUri())==m_tracingInterests.end()
  || m_tracingInterests[InterestNameString].getExpiry() < newExpiry)
    m_tracingInterests[InterestNameString]=TntRecord(interestWeak
	                                            ,newExpiry
	                                            ,inFaceWeak
					            ,pitEntryWeek
					            );
    
}
   
}// end namespace pit
} //end namespace nfd

#endif // KITE

#endif // NFD_DAEMON_FW_PIT_TNT_ENTRY_HPP
