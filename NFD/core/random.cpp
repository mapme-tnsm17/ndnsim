/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2015,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
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
#ifdef FIX_RANDOM
#include <ctime>
#endif // FIX_RANDOM

#include "random.hpp"
#include <boost/thread/tss.hpp>

namespace nfd {

static boost::thread_specific_ptr<boost::random::mt19937> g_rng;

boost::random::mt19937&
getGlobalRng()
{
  if (g_rng.get() == nullptr) {
#ifdef FIX_RANDOM
    // The purpose is to make face id generated more randomly so that the pathid is
    // more robust. i change the code to make the seed depend on the starting time.
    // I found this problem when I did some tests with lurch on grid5000, where I
    // found a correlation between the face IDs on each node(actually they are the
    // same on each node with same degree)
    // -- XZ
    g_rng.reset(new boost::random::mt19937(std::time(0)));
#else
    g_rng.reset(new boost::random::mt19937());
#endif // FIX_RANDOM
  }
  return *g_rng;
}

void
resetGlobalRng()
{
  g_rng.reset();
}

} // namespace nfd
