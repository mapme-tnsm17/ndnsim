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
 */

#ifndef NFD_DAEMON_FW_FORWARDER_HPP
#define NFD_DAEMON_FW_FORWARDER_HPP

#include "common.hpp"
#ifdef CONF_FILE
#include "config-file.hpp"
#endif // CONF_FILE
#include "core/scheduler.hpp"
#include "forwarder-counters.hpp"
#include "face-table.hpp"
#include "table/fib.hpp"
#include "table/pit.hpp"
#include "table/cs.hpp"
#include "table/measurements.hpp"
#include "table/strategy-choice.hpp"
#include "table/dead-nonce-list.hpp"
#ifndef NDNSIM
#include "table/network-region-table.hpp"
#endif // !NDNSIM

#ifdef NDNSIM
#include "ns3/ndnSIM/model/cs/ndn-content-store.hpp"
#endif // NDNSIM


namespace nfd {

namespace fw {
class Strategy;
} // namespace fw

#ifdef NDNSIM
class NullFace;
#endif // NDNSIM

/** \brief main class of NFD
 *
 *  Forwarder owns all faces and tables, and implements forwarding pipelines.
 */
class Forwarder
{
public:

  Forwarder();

#ifndef CONF_FILE
  VIRTUAL_WITH_TESTS
#else
  virtual
#endif // !CONF_FILE
  ~Forwarder();

  const ForwarderCounters&
  getCounters() const;

public: // faces
  FaceTable&
  getFaceTable();

  /** \brief get existing Face
   *
   *  shortcut to .getFaceTable().get(face)
   */
  shared_ptr<Face>
  getFace(FaceId id) const;

  /** \brief add new Face
   *
   *  shortcut to .getFaceTable().add(face)
   */
  void
  addFace(shared_ptr<Face> face);

public: // forwarding entrypoints and tables
#ifdef NDNSIM
  void
  onInterest(Face& face, const Interest& interest);

  void
  onData(Face& face, const Data& data);
#else
  /** \brief start incoming Interest processing
   *  \param interest the incoming Interest, must be created with make_shared
   */
  void
  startProcessInterest(Face& face, const Interest& interest);

  /** \brief start incoming Data processing
   *  \param data the incoming Data, must be created with make_shared
   */
  void
  startProcessData(Face& face, const Data& data);

  /** \brief start incoming Nack processing
   *  \param nack the incoming Nack, must be created with make_shared
   */
  void
  startProcessNack(Face& face, const lp::Nack& nack);

#endif // NDNSIM

  NameTree&
  getNameTree();

  Fib&
  getFib();
  
  Pit&
  getPit();

  Cs&
  getCs();

  Measurements&
  getMeasurements();

  StrategyChoice&
  getStrategyChoice();

  DeadNonceList&
  getDeadNonceList();


#ifndef NDNSIM
  NetworkRegionTable&
  getNetworkRegionTable();
#endif // !NDNSIM

#ifdef NDNSIM
public: // allow enabling ndnSIM content store (will be removed in the future)
  void
  setCsFromNdnSim(ns3::Ptr<ns3::ndn::ContentStore> cs);

public:
  /** \brief trigger before PIT entry is satisfied
   *  \sa Strategy::beforeSatisfyInterest
   */
  signal::Signal<Forwarder, pit::Entry, Face, Data> beforeSatisfyInterest;

  /** \brief trigger before PIT entry expires
   *  \sa Strategy::beforeExpirePendingInterest
   */
  signal::Signal<Forwarder, pit::Entry> beforeExpirePendingInterest;
#endif // NDNSIM

#ifdef MAPME
PUBLIC_WITH_TESTS_ELSE_PROTECTED:
#else
#ifdef ANCHOR
PUBLIC_WITH_TESTS_ELSE_PROTECTED:
#else
PUBLIC_WITH_TESTS_ELSE_PRIVATE: // pipelines
#endif // ANCHOR
#endif // MAPME
  /** \brief incoming Interest pipeline
   */
#ifdef NDNSIM
  virtual void
#else
#ifndef CONF_FILE
  VIRTUAL_WITH_TESTS
#else
  virtual
#endif
  void
#endif // NDNSIM
  onIncomingInterest(Face& inFace, const Interest& interest);

  /** \brief Content Store miss pipeline
  */
#ifdef NDNSIM
  virtual void
#else
#ifndef CONF_FILE
  VIRTUAL_WITH_TESTS
#else
  virtual
#endif
  void
#endif // NDNSIM
  onContentStoreMiss(const Face& inFace, shared_ptr<pit::Entry> pitEntry, const Interest& interest);

  /** \brief Content Store hit pipeline
  */
#ifndef NDNSIM
  // XXX Doublecheck this
  VIRTUAL_WITH_TESTS
#endif // !NDNSIM
  void
  onContentStoreHit(const Face& inFace, shared_ptr<pit::Entry> pitEntry,
                    const Interest& interest, const Data& data);

  /** \brief Interest loop pipeline
   */
  VIRTUAL_WITH_TESTS void
  onInterestLoop(Face& inFace, const Interest& interest,
                 shared_ptr<pit::Entry> pitEntry);

  /** \brief outgoing Interest pipeline
   */
#ifdef NDNSIM
  // Needed for logging signalling overhead in forwarders
  virtual void
#else
  VIRTUAL_WITH_TESTS void
#endif // NDNSM
  onOutgoingInterest(shared_ptr<pit::Entry> pitEntry, Face& outFace,
                     bool wantNewNonce = false);

  /** \brief Interest reject pipeline
   */
  VIRTUAL_WITH_TESTS void
  onInterestReject(shared_ptr<pit::Entry> pitEntry);

  /** \brief Interest unsatisfied pipeline
   */
  VIRTUAL_WITH_TESTS void
  onInterestUnsatisfied(shared_ptr<pit::Entry> pitEntry);

  /** \brief Interest finalize pipeline
   *  \param isSatisfied whether the Interest has been satisfied
   *  \param dataFreshnessPeriod FreshnessPeriod of satisfying Data
   */
  VIRTUAL_WITH_TESTS void
  onInterestFinalize(shared_ptr<pit::Entry> pitEntry, bool isSatisfied,
                     const time::milliseconds& dataFreshnessPeriod = time::milliseconds(-1));

  /** \brief incoming Data pipeline
   */
#ifdef BUGFIXES
  // Needed for canceling retransmissions
  virtual void
#else
  VIRTUAL_WITH_TESTS void
#endif // BUGFIXES
  onIncomingData(Face& inFace, const Data& data);

  /** \brief Data unsolicited pipeline
   */
  VIRTUAL_WITH_TESTS void
  onDataUnsolicited(Face& inFace, const Data& data);

  /** \brief outgoing Data pipeline
   */
  VIRTUAL_WITH_TESTS void
  onOutgoingData(const Data& data, Face& outFace);

#ifndef NDNSIM
  /** \brief incoming Nack pipeline
   */
  VIRTUAL_WITH_TESTS void
  onIncomingNack(Face& inFace, const lp::Nack& nack);

  /** \brief outgoing Nack pipeline
   */
  VIRTUAL_WITH_TESTS void
  onOutgoingNack(shared_ptr<pit::Entry> pitEntry, const Face& outFace, const lp::NackHeader& nack);
#endif // !NDNSIM


#ifdef MAPME
protected:
#else
PROTECTED_WITH_TESTS_ELSE_PRIVATE:
#endif // MAPME
  VIRTUAL_WITH_TESTS void
  setUnsatisfyTimer(shared_ptr<pit::Entry> pitEntry);

  VIRTUAL_WITH_TESTS void
  setStragglerTimer(shared_ptr<pit::Entry> pitEntry, bool isSatisfied,
                    const time::milliseconds& dataFreshnessPeriod = time::milliseconds(-1));

  VIRTUAL_WITH_TESTS void
  cancelUnsatisfyAndStragglerTimer(shared_ptr<pit::Entry> pitEntry);

  /** \brief insert Nonce to Dead Nonce List if necessary
   *  \param upstream if null, insert Nonces from all OutRecords;
   *                  if not null, insert Nonce only on the OutRecord of this face
   */
  VIRTUAL_WITH_TESTS void
  insertDeadNonceList(pit::Entry& pitEntry, bool isSatisfied,
                      const time::milliseconds& dataFreshnessPeriod,
                      Face* upstream);

  /// call trigger (method) on the effective strategy of pitEntry
#ifdef WITH_TESTS
  virtual void
  dispatchToStrategy(shared_ptr<pit::Entry> pitEntry, function<void(fw::Strategy*)> trigger);
#else
  template<class Function>
  void
  dispatchToStrategy(shared_ptr<pit::Entry> pitEntry, Function trigger);
#endif

#ifdef MAPME
protected:
#else
private:
#endif
  ForwarderCounters m_counters;

  FaceTable m_faceTable;

  // tables
  NameTree       m_nameTree;
  Fib            m_fib;
  Pit            m_pit;
  Cs             m_cs;
  Measurements   m_measurements;
  StrategyChoice m_strategyChoice;
  DeadNonceList  m_deadNonceList;


#ifdef NDNSIM
  shared_ptr<NullFace> m_csFace;
  ns3::Ptr<ns3::ndn::ContentStore> m_csFromNdnSim;
#else
  NetworkRegionTable m_networkRegionTable;
#endif // NDNSIM

  static const Name LOCALHOST_NAME;

  // allow Strategy (base class) to enter pipelines
  friend class fw::Strategy;

#ifdef MAPME
public:
  bool m_removeFibEntries = true;
#endif // MAPME
  
};

inline const ForwarderCounters&
Forwarder::getCounters() const
{
  return m_counters;
}

inline FaceTable&
Forwarder::getFaceTable()
{
  return m_faceTable;
}

inline shared_ptr<Face>
Forwarder::getFace(FaceId id) const
{
  return m_faceTable.get(id);
}

inline void
Forwarder::addFace(shared_ptr<Face> face)
{
  m_faceTable.add(face);
}

#ifdef NDNSIM
inline void
Forwarder::onInterest(Face& face, const Interest& interest)
{
  this->onIncomingInterest(face, interest);
}

inline void
Forwarder::onData(Face& face, const Data& data)
{
  this->onIncomingData(face, data);
}
#endif // NDNSIM

inline NameTree&
Forwarder::getNameTree()
{
  return m_nameTree;
}

inline Fib&
Forwarder::getFib()
{
  return m_fib;
}

inline Pit&
Forwarder::getPit()
{
  return m_pit;
}

inline Cs&
Forwarder::getCs()
{
  return m_cs;
}

inline Measurements&
Forwarder::getMeasurements()
{
  return m_measurements;
}

inline StrategyChoice&
Forwarder::getStrategyChoice()
{
  return m_strategyChoice;
}

inline DeadNonceList&
Forwarder::getDeadNonceList()
{
  return m_deadNonceList;
}

#ifndef NDNSIM
inline NetworkRegionTable&
Forwarder::getNetworkRegionTable()
{
  return m_networkRegionTable;
}
#endif // !NDNSIM

#ifdef NDNSIM
inline void
Forwarder::setCsFromNdnSim(ns3::Ptr<ns3::ndn::ContentStore> cs)
{
  m_csFromNdnSim = cs;
}
#endif // NDNSIM

#ifdef WITH_TESTS
inline void
Forwarder::dispatchToStrategy(shared_ptr<pit::Entry> pitEntry, function<void(fw::Strategy*)> trigger)
#else
template<class Function>
inline void
Forwarder::dispatchToStrategy(shared_ptr<pit::Entry> pitEntry, Function trigger)
#endif
{
  fw::Strategy& strategy = m_strategyChoice.findEffectiveStrategy(*pitEntry);
  trigger(&strategy);
}


} // namespace nfd

#endif // NFD_DAEMON_FW_FORWARDER_HPP
