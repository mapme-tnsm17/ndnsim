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

#ifndef NDN_BSR_TABLE_HPP
#define NDN_BSR_TABLE_HPP

//#include "ns3/core-module.h"
//#include "ns3/network-module.h"
//#include <boost/shared_ptr.hpp>

#include "ns3/ptr.h"
#include "ns3/ndnSIM-module.h"
#include "bsr-scheme-entry.hpp"
#include <ostream>
#include <map>
#include <list>

namespace ns3 {
    namespace ndn {

        class BSRTable
        {
    		typedef std::list<BSREntry> BSRentryList;
    		typedef std::map<ndn::Name, std::map<shared_ptr<nfd::Face>, BSRentryList> > BSRfullTable;
    		typedef std::map<shared_ptr<nfd::Face>, BSRentryList > BSRsubTable;

        	private:

        		BSRfullTable m_BsrTable;
        		BSRsubTable m_BsrRtxMap;

            public:
        		/* Insert
        		 * Get BSR by name and face
        		 * Increase number of RTX
        		 * Delete
        		 *
        		 */
        		BSRTable();

        		void
        		InsertElement(ndn::Name prefix,
        					  shared_ptr<nfd::Face> outFace,
        					  shared_ptr<nfd::pit::Entry> pitEntry,
        					  uint32_t retransmissions);

        		void
        		InsertRetransmission(ndn::Name prefix,
        							 shared_ptr<nfd::Face> outFace,
        							 BSREntry & bsr_entry);

        		std::list<BSREntry> *
        		GetBsrList(ndn::Name prefix,
        				   shared_ptr<nfd::Face> outFace);

        		BSREntry *
        		GetBsrEntry(ndn::Name prefix,
        					shared_ptr<nfd::Face> outFace,
        					shared_ptr<nfd::pit::Entry> pitEntry);

        		void
        		IncreaseRetransmitted(ndn::Name prefix,
        							  shared_ptr<nfd::Face> outFace,
        							  shared_ptr<nfd::pit::Entry> pitEntry);

        		void
        		IncreaseRetransmittedByEntry(BSREntry & bsr_entry);

        		void
        		DeleteBsrEntry(ndn::Name prefix,
        					   shared_ptr< nfd::Face> outFace,
        					   shared_ptr<nfd::pit::Entry> pitEntry);

        		void
        		DeleteBsrEntryWithList(BSRentryList & bsr_list,
        							   BSREntry & bsr_entry);

        		BSRsubTable *
        		GetInterestsToRetransmit(BSRentryList & bsr_list,
        								 shared_ptr<nfd::pit::Entry> pitEntry,
        								 shared_ptr< nfd::Face> outFace,
        								 uint32_t rtx_allowed);

        		void
        		SetNextAsNewExpectedByEntry(BSREntry & bsr_entry,
        									BSRentryList & bsr_list);
        		void
        		PrintBSRTable();

        		void
        		PrintBSRrtxMap(BSRsubTable & bsr_rtx_map);

        		void
        		PrintBSRlist(BSRentryList & bsr_list);

        		void
        		ClearAll();
};
    }
}
#endif
