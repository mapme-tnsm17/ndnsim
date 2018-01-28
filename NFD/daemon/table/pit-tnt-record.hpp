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

#ifndef NFD_DAEMON_TABLE_PIT_TNT_RECORD_HPP
#define NFD_DAEMON_TABLE_PIT_TNT_RECORD_HPP

#ifdef KITE

#include "face/face.hpp"
#include "pit-entry.hpp"
namespace nfd {

  namespace pit {
/**
 *   \brief a record for tracing interest. since only one of the interests with the same name and trace name
 *          shoud be pulled each time, a tracing record in principle should only contain one interest.
 *          main difference is that it contains weak_ptr to interest instead of shared_ptr to interest.
 *   
 */ 
class TntRecord
{
public:
  
  TntRecord();
  
  TntRecord(weak_ptr<const Interest> interestWeak
           ,time::steady_clock::TimePoint expiry
           ,weak_ptr<Face> inFaceWeak
	   ,weak_ptr<pit::Entry> pitEntryWeak);
  bool
  isValid() const;
  
  weak_ptr<const Interest>
  getInterest() const;
  
  time::steady_clock::TimePoint
  getExpiry() const;
  
  weak_ptr<Face>
  getInFace() const;
  
  weak_ptr<pit::Entry>
  getPitEntry() const;
  
  
    
private:
  weak_ptr<const Interest> m_interest;
  time::steady_clock::TimePoint m_expiry;
  weak_ptr<Face> m_inface;
  weak_ptr<pit::Entry> m_pitEntry; //the corresponding pit entry of the interest
};

inline
TntRecord::TntRecord()
{
  
}

inline
TntRecord::TntRecord(weak_ptr<const Interest> interestWeak
           ,time::steady_clock::TimePoint expiry
           ,weak_ptr<Face> inFaceWeak
	   ,weak_ptr<pit::Entry> pitEntryWeak)
         : m_interest(interestWeak)
	 , m_expiry(expiry)
	 , m_inface(inFaceWeak)
	 , m_pitEntry(pitEntryWeak)
{
	   
}

inline bool
TntRecord::isValid() const
{
  return static_cast<bool>(m_interest.lock())  //interest exists
      && static_cast<bool>(m_pitEntry.lock())  //pit entry exists
      && m_expiry > time::steady_clock::now();  //interest not expired
}

inline weak_ptr<const Interest>
TntRecord::getInterest() const
{
  return m_interest;
}

inline time::steady_clock::TimePoint
TntRecord::getExpiry() const
{
  return m_expiry;
}

inline weak_ptr<Face>
TntRecord::getInFace() const
{
  return m_inface;
}

inline weak_ptr<pit::Entry>
TntRecord::getPitEntry() const
{
  return m_pitEntry;
}



}// end namespace pit
}// end namespace nfd

#endif // KITE

#endif // NFD_DAEMON_FW_PIT_TNT_RECORD_HPP
