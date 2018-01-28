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

#include "bsr-scheme-entry.hpp"
#include <ostream>

namespace ns3 {
    namespace ndn {


    	BSREntry::BSREntry(shared_ptr<nfd::pit::Entry> entry, int64_t timestamp, bool expected, uint32_t retransmissions):
            m_pitEntry(entry)
    	,	m_timestamp(timestamp)
    	,	m_expected(expected)
    	,	m_nTimesRetransmitted(retransmissions)
        {}

        std::ostream & operator << (std::ostream & out, const BSREntry & bsr_entry)
        {
            out << "\tPitEntry: "      	<< bsr_entry.GetPitEntry()
                << "\tTimestamp: " 	   	<< bsr_entry.GetTimestamp()
                << "\tIsExpected: "		<< bsr_entry.GetIfExpected()
                << "\tRetransmitted: "  << bsr_entry.GetNRetransmissions()<<" times"
                <<std::endl;
            return out;
        }

        BSREntry & BSREntry::operator = (const BSREntry & bsr)
        {
            this->SetPitEntry(bsr.GetPitEntry());
            this->SetTimestamp(bsr.GetTimestamp());
            this->SetIfExpected(bsr.GetIfExpected());
            this->SetNRetransmissions(bsr.GetNRetransmissions());
            return *this;
        }

        bool operator < (const BSREntry & x, const BSREntry & y) {

        	if (x.GetTimestamp() < y.GetTimestamp()) return true;
        	if (y.GetTimestamp() < x.GetTimestamp()) return false;

            return false;
        }

        bool operator == (const BSREntry &x, const BSREntry &y) {
        	if(x.GetPitEntry() == y.GetPitEntry()
        		&& x.GetTimestamp() == y.GetTimestamp()
        		&& x.GetIfExpected() == y.GetIfExpected()
        		&& x.GetNRetransmissions() == y.GetNRetransmissions())
        		return true;
        	return false;
        }
    }
}
