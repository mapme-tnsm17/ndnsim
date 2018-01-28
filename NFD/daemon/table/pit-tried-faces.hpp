/*
 *  Copyright (c) 2015,  Cisco Systems Inc.
 *  Author: Natalya Rozhnova <natalya.rozhnova@cisco.com>
 */

#ifndef NDN_TRIED_TABLE_HPP
#define NDN_TRIED_TABLE_HPP

//#include "ns3/core-module.h"
//#include "ns3/network-module.h"
//#include <boost/shared_ptr.hpp>

#include "ns3/ptr.h"
//#include "ns3/ndnSIM-module.h"
#include <ostream>
#include <list>

#include "ns3/ndnSIM/model/ndn-common.hpp"
#include "ns3/ndnSIM/NFD/daemon/face/face.hpp"

namespace nfd {
class TriedFaces
{
	typedef std::list<shared_ptr<Face>> TriedFacesList;

private:

	TriedFacesList m_triedFaces;

public:

	TriedFaces();

	void
	InsertElement(shared_ptr<Face> outFace);

	bool
	AlreadyTried(shared_ptr<Face> face);

	void
	RemoveTried(shared_ptr<Face> face);

	void
	PrintTriedFaces();

	bool
	Empty();

	void
	ClearAll();
};
}
#endif
