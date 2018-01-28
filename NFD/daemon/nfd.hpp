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

#ifndef NFD_DAEMON_NFD_HPP
#define NFD_DAEMON_NFD_HPP

#include "common.hpp"
#include "core/config-file.hpp"
#include "core/scheduler.hpp"
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/network-monitor.hpp>

#ifndef NDNSIM
namespace ndn {

class Face;

namespace mgmt {

class Dispatcher;

}
}
#endif // !NDNSIM

namespace nfd {

#ifndef NDNSIM
class Face;
#endif // !NDNSIM
class Forwarder;
#ifdef NDNSIM
class InternalFace;
#endif // NDNSIM
class FibManager;
class FaceManager;
class StrategyChoiceManager;
#ifdef NDNSIM
class StatusServer;
#else
class ForwarderStatusManager;
#ifdef CACHE_EXTENSIONS
class ContentStoreManager;
#endif // CACHE_EXTENSIONS
class CommandValidator;
#endif // NDNSIM

#ifndef NDNSIM
#ifdef CONF_FILE
#define MS_MAPME_S "mapme"
#define MS_ANCHOR_S "anchor"
#define MS_KITE_S "kite"
#define MS_VANILLA_S "vanilla"
#endif // !NDNSIM

enum mobility_scheme_e {                                                                                                             
#ifdef MAPME
  MS_MAPME,                                                                                                                          
#endif // MAPME
#ifdef ANCHOR
  MS_ANCHOR,
#endif // ANCHOR
#ifdef KITE
  MS_KITE,
#endif // KITE
  MS_VANILLA
};                                                                                                                                   
#endif // CONF_FILE    

/**
 * \brief Class representing NFD instance
 * This class can be used to initialize all components of NFD
 */
class Nfd : noncopyable
{
public:
  /**
   * \brief Create NFD instance using absolute or relative path to \p configFile
   */
  Nfd(const std::string& configFile, ndn::KeyChain& keyChain);

  /**
   * \brief Create NFD instance using a parsed ConfigSection \p config
   * This version of the constructor is more appropriate for integrated environments,
   * such as NS-3 or android.
   * \note When using this version of the constructor, error messages will include
   *      "internal://nfd.conf" when referring to configuration errors.
   */
  Nfd(const ConfigSection& config, ndn::KeyChain& keyChain);

  /**
   * \brief Destructor
   */
  ~Nfd();

  /**
   * \brief Perform initialization of NFD instance
   * After initialization, NFD instance can be started by invoking run on globalIoService
   */
  void
  initialize();

  /**
   * \brief Reload configuration file and apply update (if any)
   */
  void
  reloadConfigFile();

#ifndef NDNSIM
#ifdef CONF_FILE
  void
  initializeMobility();
#endif // CONF_FILE
#endif // !NDNSIM

private:
  void
  initializeLogging();

  void
  initializeManagement();

  void
  reloadConfigFileFaceSection();

private:
  std::string m_configFile;
  ConfigSection m_configSection;

#ifndef NDNSIM
#ifdef CONF_FILE
  enum mobility_scheme_e m_mobility_scheme;
#ifdef MAPME
  uint64_t m_Tu;
#endif // MAPME
#endif // CONF_FILE
#endif // !NDNSIM

  unique_ptr<Forwarder> m_forwarder;

#ifdef NDNSIM

  shared_ptr<InternalFace>          m_internalFace;
  unique_ptr<FibManager>            m_fibManager;
  unique_ptr<FaceManager>           m_faceManager;
  unique_ptr<StrategyChoiceManager> m_strategyChoiceManager;
  unique_ptr<StatusServer>          m_statusServer;

  ndn::KeyChain&                    m_keyChain;

  ndn::util::NetworkMonitor         m_networkMonitor;
#else
  ndn::KeyChain&               m_keyChain;
  shared_ptr<Face>             m_internalFace;
  shared_ptr<ndn::Face>        m_internalClientFace;
  unique_ptr<CommandValidator> m_validator;

  unique_ptr<ndn::mgmt::Dispatcher>  m_dispatcher;
  unique_ptr<FibManager>             m_fibManager;
  unique_ptr<FaceManager>            m_faceManager;
  unique_ptr<StrategyChoiceManager>  m_strategyChoiceManager;
  unique_ptr<ForwarderStatusManager> m_forwarderStatusManager;
#ifdef CACHE_EXTENSIONS
  unique_ptr<ContentStoreManager>    m_contentStoreManager;
#endif // CACHE_EXTENSIONS

  unique_ptr<ndn::util::NetworkMonitor> m_networkMonitor;
#endif // NDNSIM
  scheduler::ScopedEventId              m_reloadConfigEvent;
};

} // namespace nfd

#endif // NFD_DAEMON_NFD_HPP
