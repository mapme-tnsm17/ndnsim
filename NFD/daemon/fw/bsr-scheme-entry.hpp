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
 * Authors: Rozhnova Natalya <natalya.rozhnova@cisco.com>
 */

#ifndef NDN_BSR_ENTRY_HPP
#define NDN_BSR_ENTRY_HPP

//#include "ns3/core-module.h"
//#include "ns3/network-module.h"
//#include <boost/shared_ptr.hpp>

#include "ns3/ndnSIM-module.h"
#include <ostream>

namespace ns3 {
    namespace ndn {

        class BSREntry
        {
            private:

        		shared_ptr<nfd::pit::Entry> m_pitEntry;		/**< Corresponding PIT entry*/
                int64_t  m_timestamp;     					/**< The sending timestamp value, in microseconds */
                bool m_expected;       	   					/**< Whether the packet is the next expected ?*/
                uint32_t m_nTimesRetransmitted; 	   		/**< How many times this packet has already been retransmitted */

            public:
                // Constructors

                BSREntry(shared_ptr<nfd::pit::Entry> entry, int64_t timestamp, bool expected, uint32_t retransmissions);

                // Getters/Setters

                inline shared_ptr<nfd::pit::Entry> GetPitEntry() const {
                	return this->m_pitEntry;
                }

                inline void SetPitEntry(shared_ptr<nfd::pit::Entry> entry) {
                	this->m_pitEntry = entry;
                }

                inline int64_t GetTimestamp() const {
                    return this->m_timestamp;
                }

                inline void SetTimestamp(int64_t timestamp) {
                    this->m_timestamp = timestamp;
                }

                inline bool GetIfExpected() const {
                    return this->m_expected;
                }

                inline void SetIfExpected(bool expected) {
                	this->m_expected = expected;
                }

                inline uint32_t GetNRetransmissions() const {
                	return this->m_nTimesRetransmitted;
                }

                inline void SetNRetransmissions(uint32_t retransmissions){
                	this->m_nTimesRetransmitted = retransmissions;
                }

                inline void IncreaseNRetransmissions() {
                	++(this->m_nTimesRetransmitted);
                }

                // Operators

                BSREntry & operator = (const BSREntry & bsr);

        };

        /**
         * \brief Write the content of an BSREntry instance.
         * \param out The output stream.
         * \param entry The BSREntry instance we want to write.
         */

        std::ostream & operator << (std::ostream & out, const BSREntry & entry);

        /**
         * \brief Compare two BSREntry instances
         * \param x The left operand
         * \param y The right operand
         * \param return true if x is less than y
         */

        bool operator < (const BSREntry & x, const BSREntry & y);

        bool operator == (const BSREntry &x, const BSREntry &y);
    } // end namespace ndn
} // end namespace ns3

#endif
