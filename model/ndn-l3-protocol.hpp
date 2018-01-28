/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#ifndef NDN_L3_PROTOCOL_H
#define NDN_L3_PROTOCOL_H

#include "ns3/ndnSIM/model/ndn-common.hpp"
#include "ns3/ndnSIM/model/ndn-face.hpp"

#ifdef CONF_FILE
#include "ns3/ndnSIM/NFD/core/config-file.hpp"
// NONSENSE?|#ifdef MAPME /* for constants */
// NONSENSE?|#include "ns3/ndnSIM/NFD/daemon/fw/forwarder-mapme.hpp"
// NONSENSE?|#endif // MAPME
// NONSENSE?|#ifdef ANCHOR /* for constants */
// NONSENSE?|#include "ns3/ndnSIM/NFD/daemon/fw/forwarder-anchor.hpp"
// NONSENSE?|#endif // ANCHOR
#endif // CONF_FILE

#include <list>
#include <vector>

#include "ns3/ptr.h"
#include "ns3/net-device.h"
#include "ns3/nstime.h"
#include "ns3/traced-callback.h"

#include <boost/property_tree/ptree_fwd.hpp>

namespace nfd {
class Forwarder;
class FibManager;
class StrategyChoiceManager;
typedef boost::property_tree::ptree ConfigSection;
namespace pit {
class Entry;
} // namespace pit
} // namespace nfd

namespace ns3 {

class Packet;
class Node;
class Header;

namespace ndn {

/**
 * \defgroup ndn ndnSIM: NDN simulation module
 *
 * This is a modular implementation of NDN protocol for NS-3
 */
/**
 * \ingroup ndn
 * \brief Implementation network-layer of NDN stack
 *
 * This class defines the API to manipulate the following aspects of
 * the NDN stack implementation:
 * -# register a face (Face-derived object) for use by the NDN
 *    layer
 *
 * Each Face-derived object has conceptually a single NDN
 * interface associated with it.
 *
 * In addition, this class defines NDN packet coding constants
 *
 * \see Face, ForwardingStrategy
 */
class L3Protocol : boost::noncopyable, public Object {
public:
  /**
   * \brief Interface ID
   *
   * \return interface ID
   */
  static TypeId
  GetTypeId();

#ifdef CONF_FILE

#define MS_MAPME_S "mapme"
#define MS_ANCHOR_S "anchor"
#define MS_KITE_S "kite"
#define MS_VANILLA_S "vanilla"

  enum mobility_scheme_e {
    MS_VANILLA,
#ifdef MAPME
    MS_MAPME,
#endif // MAPME
#ifdef ANCHOR
    MS_ANCHOR,
#endif // ANCHOR
#ifdef KITE
    MS_KITE
#endif // KITE
  };
#endif // CONF_FILE

  static const uint16_t ETHERNET_FRAME_TYPE; ///< @brief Ethernet Frame Type of Ndn
  static const uint16_t IP_STACK_PORT;       ///< @brief TCP/UDP port for NDN stack
  // static const uint16_t IP_PROTOCOL_TYPE;    ///< \brief IP protocol type of NDN

  /**
   * \brief Default constructor. Creates an empty stack without forwarding strategy set
   */
  L3Protocol();

  virtual ~L3Protocol();

  /**
   * \brief Get smart pointer to nfd::Forwarder installed on the node
   */
  shared_ptr<nfd::Forwarder>
  getForwarder();

  /**
   * \brief Get smart pointer to nfd::FibManager, used by node's NFD
   */
  shared_ptr<nfd::FibManager>
  getFibManager();

  /**
   * \brief Get smart pointer to nfd::StrategyChoiceManager, used by node's NFD
   */
  shared_ptr<nfd::StrategyChoiceManager>
  getStrategyChoiceManager();

  /**
   * \brief Add face to NDN stack
   *
   * \param face smart pointer to Face-derived object (AppFace, NetDeviceFace)
   * \return nfd::FaceId
   *
   * \see AppFace, NetDeviceFace
   */
  nfd::FaceId
  addFace(shared_ptr<Face> face);

  /**
   * \brief Get face by face ID
   * \param face The face ID number
   * \returns The Face associated with the Ndn face number.
   */
  shared_ptr<Face>
  getFaceById(nfd::FaceId face) const;

#ifdef NETWORK_DYNAMICS
  virtual void
  removeFace(shared_ptr<Face> face);
#else
  // /**
  //  * \brief Remove face from ndn stack (remove callbacks)
  //  */
  // virtual void
  // removeFace(shared_ptr<Face> face);
#endif // NETWORK_DYNAMICS

  /**
   * \brief Get face for NetDevice
   */
  shared_ptr<Face>
  getFaceByNetDevice(Ptr<NetDevice> netDevice) const;

  /**
   * \brief Get NFD config (boost::property_tree)
   */
  nfd::ConfigSection&
  getConfig();

public: // Workaround for python bindings
  static Ptr<L3Protocol>
  getL3Protocol(Ptr<Object> node);

#ifdef CONF_FILE
  static Ptr<const AttributeChecker> s_enum_checker; 
#endif // CONF_FILE

public:
  typedef void (*InterestTraceCallback)(const Interest&, const Face&);
  typedef void (*DataTraceCallback)(const Data&, const Face&);

  typedef void (*SatisfiedInterestsCallback)(const nfd::pit::Entry& pitEntry, const Face& inFace, const Data& data);
  typedef void (*TimedOutInterestsCallback)(const nfd::pit::Entry& pitEntry);

protected:
  virtual void
  DoDispose(void); ///< @brief Do cleanup

  /**
   * This function will notify other components connected to the node that a new stack member is now
   * connected
   * This will be used to notify Layer 3 protocol of layer 4 protocol stack to connect them
   * together.
   */
  virtual void
  NotifyNewAggregate();

private:
  void
  initialize();

#ifdef CONF_FILE
  void
  initializeMobility();
#endif // CONF_FILE

  void
  initializeManagement();

  void
  initializeRibManager();

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;

  // These objects are aggregated, but for optimization, get them here
  Ptr<Node> m_node; ///< \brief node on which ndn stack is installed
  
#ifdef CONF_FILE
  enum mobility_scheme_e m_mobility_scheme;
#ifdef MAPME
  uint64_t m_Tu;
#endif // MAPME
#endif // CONF_FILE

  TracedCallback<const Interest&, const Face&>
    m_inInterests; ///< @brief trace of incoming Interests
  TracedCallback<const Interest&, const Face&>
    m_outInterests; ///< @brief Transmitted interests trace

  TracedCallback<const Data&, const Face&> m_outData; ///< @brief trace of outgoing Data
  TracedCallback<const Data&, const Face&> m_inData;  ///< @brief trace of incoming Data

  TracedCallback<const nfd::pit::Entry&, const Face&/*in face*/, const Data&> m_satisfiedInterests;
  TracedCallback<const nfd::pit::Entry&> m_timedOutInterests;
};

} // namespace ndn
} // namespace ns3

#endif /* NDN_L3_PROTOCOL_H */
