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

/* NOTE: Many #ifdef NDNSIM are present in this file to allow the merge of both regular NFD and ndnSIM flavour of NFD, and subsequently the maintainance of a single codebases for developing exntesions to NFD. This way, the current file can just be copied from NFD (the smaster codebase) to ndnSIM.
 *
 * XXX Since nfd.cpp does not seem to be used at all in simulations, do we need
 * to maintain the differences ?
 */
#include "nfd.hpp"

#include "core/global-io.hpp"
#include "core/logger-factory.hpp"
#include "core/privilege-helper.hpp"
#include "core/config-file.hpp"
#include "fw/forwarder.hpp"

#ifdef CONF_FILE
#ifndef NDNSIM
#include <boost/property_tree/info_parser.hpp>
#include <fstream>
#endif // !NDNSIM
#ifdef MAPME
#include "fw/forwarder-mapme.hpp"
#endif //MAPME
#ifdef ANCHOR
#include "fw/forwarder-anchor.hpp"
#endif // ANCHOR
#ifdef KITE
#include "fw/forwarder-kite.cpp"
#endif //KITE
#endif //CONF_FILE

#include "face/null-face.hpp"
#ifdef NDNSIM
#include "mgmt/internal-face.hpp"
#else
#include "face/internal-face.hpp"
#endif // NDNSIM
#include "mgmt/fib-manager.hpp"
#include "mgmt/face-manager.hpp"
#include "mgmt/strategy-choice-manager.hpp"
#ifdef CACHE_EXTENSIONS
#include "mgmt/content-store-manager.hpp"
#endif // CACHE_EXTENSIONS
#include "mgmt/general-config-section.hpp"
#include "mgmt/tables-config-section.hpp"

#ifdef NDNSIM
#include "mgmt/status-server.hpp"
#else
#include "mgmt/forwarder-status-manager.hpp"
#include "mgmt/command-validator.hpp"
#include <ndn-cxx/mgmt/dispatcher.hpp>
#endif // NDNSIM

namespace nfd {

NFD_LOG_INIT("Nfd");

static const std::string INTERNAL_CONFIG = "internal://nfd.conf";

#ifndef NDNSIM
static inline ndn::util::NetworkMonitor*
makeNetworkMonitor()
{
  try {
    return new ndn::util::NetworkMonitor(getGlobalIoService());
  }
  catch (const ndn::util::NetworkMonitor::Error& e) {
    NFD_LOG_WARN(e.what());
    return nullptr;
  }
}
#endif // !NDNSIM

Nfd::Nfd(const std::string& configFile, ndn::KeyChain& keyChain)
  : m_configFile(configFile)
  , m_keyChain(keyChain)
#ifdef NDNSIM
  , m_networkMonitor(getGlobalIoService())
#else
  , m_networkMonitor(makeNetworkMonitor())
#endif // NDNSIM
{
}

Nfd::Nfd(const ConfigSection& config, ndn::KeyChain& keyChain)
  : m_configSection(config)
  , m_keyChain(keyChain)
#ifdef NDNSIM
  , m_networkMonitor(getGlobalIoService())
#else
  , m_networkMonitor(makeNetworkMonitor())
#endif // NDNSIM
{
}

Nfd::~Nfd()
{
  // It is necessary to explicitly define the destructor, because some member variables (e.g.,
  // unique_ptr<Forwarder>) are forward-declared, but implicitly declared destructor requires
  // complete types for all members when instantiated.
}

void
Nfd::initialize()
{
  initializeLogging();

/* NOTE: The following code has not been made compatible with ndnSIM since this
 * nfd.cpp files is not used in the simulator at all, but instead the same
 * functionality is reimplemented in model/ndn-l3-protocol.cpp.
 */
#ifndef NDNSIM

#ifdef CONF_FILE
  // only responsible of parsing the configuration file
  initializeMobility();

  switch(m_mobility_scheme) {
    case MS_VANILLA: {
      NFD_LOG_INFO("Forwarder: regular");
      m_forwarder.reset(new Forwarder());
      break;
    }

#ifdef MAPME
    case MS_MAPME:
      NFD_LOG_INFO("Forwarder: MapMe Tu=" << m_Tu);
      m_forwarder.reset(new ForwarderMapMe(m_Tu));
      break;
#endif // MAPME

#ifdef ANCHOR
    case MS_ANCHOR:
      NFD_LOG_INFO("Forwarder: Anchor");
      m_forwarder.reset(new ForwarderAnchor());
      break;
#endif // ANCHOR

#ifdef KITE
    case MS_KITE:
      NFD_LOG_INFO("Forwarder: KITE");
      m_forwarder.reset(new ForwarderKite());
      break;
#endif // KITE
  }
#else
  m_forwarder.reset(new Forwarder());
#endif // CONF_FILE

#endif // !NDNSIM

  initializeManagement();

  m_forwarder->getFaceTable().addReserved(make_shared<NullFace>(), FACEID_NULL);
  m_forwarder->getFaceTable().addReserved(make_shared<NullFace>(FaceUri("contentstore://")),
                                          FACEID_CONTENT_STORE);

  PrivilegeHelper::drop();

/* NOTE: XXX Are the following #ifdef NDNSIM needed for compilation purposes ?
 */
#ifndef NDNSIM
  // XXX This is a patch added by Mauro.
  if (m_networkMonitor) {
#endif // !NDNSIM

#ifdef NDNSIM
  m_networkMonitor.onNetworkStateChanged.connect([this] {
#else
  m_networkMonitor->onNetworkStateChanged.connect([this] {
#endif // NDNSIM
      // delay stages, so if multiple events are triggered in short sequence,
      // only one auto-detection procedure is triggered
      m_reloadConfigEvent = scheduler::schedule(time::seconds(5),
        [this] {
          NFD_LOG_INFO("Network change detected, reloading face section of the config file...");
          this->reloadConfigFileFaceSection();
        });
    });
#ifndef NDNSIM
  }
#endif // !NDNSIM
}

void
Nfd::initializeLogging()
{
  ConfigFile config(&ConfigFile::ignoreUnknownSection);
  LoggerFactory::getInstance().setConfigFile(config);

  if (!m_configFile.empty()) {
    config.parse(m_configFile, true);
    config.parse(m_configFile, false);
  }
  else {
    config.parse(m_configSection, true, INTERNAL_CONFIG);
    config.parse(m_configSection, false, INTERNAL_CONFIG);
  }
}

#ifndef NDNSIM
#ifdef CONF_FILE
void
Nfd::initializeMobility()
{
  // Read the configuration a first time to get general information about the
  // mobility scheme to use, which defaults to 'vanilla'.
  // NOTE: We do not use the ConfigFile class due to restrictions in its
  // interface, but we borrow most of the code for this first test.
  ConfigSection m_global;
  if (!m_configFile.empty()) {
    std::ifstream inputFile;
    inputFile.open(m_configFile.c_str());
    if (!inputFile.good() || !inputFile.is_open())
      {
        std::string msg = "Failed to read configuration file: ";
        msg += m_configFile;
        BOOST_THROW_EXCEPTION(ConfigFile::Error(msg));
      }
    try {
      boost::property_tree::read_info(inputFile, m_global);
    }
    catch (const boost::property_tree::info_parser_error& error) {
      std::stringstream msg;
      msg << "Failed to parse configuration file";
      msg << " " << m_configFile;
      msg << " " << error.message() << " line " << error.line();
      BOOST_THROW_EXCEPTION(ConfigFile::Error(msg.str()));
    }
    inputFile.close();
  }
  else {
    m_global = m_configSection;
  }

  for (nfd::ConfigSection::const_iterator i = m_global.begin(); i != m_global.end(); ++i) {
    const std::string& sectionName = i->first;
    const nfd::ConfigSection& section = i->second;

    if (sectionName == "mobility") {
      // protocol
      try {
        std::string mobility = section.get<std::string>("protocol");
        if (mobility == MS_VANILLA_S) {
          m_mobility_scheme = MS_VANILLA;
        }
#ifdef MAPME
        else if (mobility == MS_MAPME_S) {
          m_mobility_scheme = MS_MAPME;
        }
// DEPRECATED|        else if (mobility == MS_MAPME_OLD_S) {
// DEPRECATED|          m_mobility_scheme = MS_MAPME_OLD;
// DEPRECATED|        }
#endif // MAPME
#ifdef KITE
        else if (mobility == MS_KITE_S) {
          m_mobility_scheme = MS_KITE;
        }
#endif // KITE
        else if (mobility.empty()) {
          BOOST_THROW_EXCEPTION(ConfigFile::Error("Invalid value for \"protocol\""
                                                  " in \"mobility\" section"));

        }
        else {
          //throw std::runtime_error("Unknown mobility parameter : " + mobility);
          BOOST_THROW_EXCEPTION(ConfigFile::Error("Invalid value for \"protocol\""
                                                  " in \"mobility\" section"));
        }
        // XXX /// m_mobility = mobility;
      }
      catch (const std::runtime_error& e) { }

#ifdef MAPME
      // MAPME - Tu
      try {
        std::string Tu = section.get<std::string>("Tu");
        m_Tu = (!Tu.empty()) ? atoi(Tu.c_str()) : MAPME_DEFAULT_TU;
      }
      catch (const std::runtime_error& e) { }
#endif // MAPME
    }
  }
}
#endif // CONF_FILE
#endif // !NDNSIM

static inline void
ignoreRibAndLogSections(const std::string& filename, const std::string& sectionName,
                        const ConfigSection& section, bool isDryRun)
{
  // Ignore "log" and "rib" sections, but raise an error if we're missing a
  // handler for an NFD section.
  if (sectionName == "rib" || sectionName == "log"
#ifdef CONF_FILE
      || sectionName == "mobility"
#endif // CONF_FILE
  ) {
    // do nothing
  } else {
    // missing NFD section
    ConfigFile::throwErrorOnUnknownSection(filename, sectionName, section, isDryRun);
  }
}

void
Nfd::initializeManagement()
{
/* NOTE: This merge of both codebases allows to directly copy file content between NFD to ndnSIM 
 */
#ifdef NDNSIM
  m_internalFace = make_shared<InternalFace>();

  m_fibManager.reset(new FibManager(m_forwarder->getFib(),
                                    bind(&Forwarder::getFace, m_forwarder.get(), _1),
                                    m_internalFace, m_keyChain));

  m_faceManager.reset(new FaceManager(m_forwarder->getFaceTable(), m_internalFace, m_keyChain));

  m_strategyChoiceManager.reset(new StrategyChoiceManager(m_forwarder->getStrategyChoice(),
                                                          m_internalFace, m_keyChain));

  m_statusServer.reset(new StatusServer(m_internalFace, *m_forwarder, m_keyChain));

  ConfigFile config(&ignoreRibAndLogSections);
  general::setConfigFile(config);

  TablesConfigSection tablesConfig(m_forwarder->getCs(),
                                   m_forwarder->getPit(),
                                   m_forwarder->getFib(),
                                   m_forwarder->getStrategyChoice(),
                                   m_forwarder->getMeasurements());
  tablesConfig.setConfigFile(config);

  m_internalFace->getValidator().setConfigFile(config);

  m_forwarder->getFaceTable().addReserved(m_internalFace, FACEID_INTERNAL_FACE);

  m_faceManager->setConfigFile(config);
#else
  std::tie(m_internalFace, m_internalClientFace) = face::makeInternalFace(m_keyChain);
  m_forwarder->getFaceTable().addReserved(m_internalFace, FACEID_INTERNAL_FACE);
  m_dispatcher.reset(new ndn::mgmt::Dispatcher(*m_internalClientFace, m_keyChain));

  m_validator.reset(new CommandValidator());

  m_fibManager.reset(new FibManager(m_forwarder->getFib(),
                                    bind(&Forwarder::getFace, m_forwarder.get(), _1),
                                    *m_dispatcher,
                                    *m_validator));

  m_faceManager.reset(new FaceManager(m_forwarder->getFaceTable(),
                                      *m_dispatcher,
                                      *m_validator));

  m_strategyChoiceManager.reset(new StrategyChoiceManager(m_forwarder->getStrategyChoice(),
                                                          *m_dispatcher,
                                                          *m_validator));

  m_forwarderStatusManager.reset(new ForwarderStatusManager(*m_forwarder, *m_dispatcher));

#ifdef CACHE_EXTENSIONS
  m_contentStoreManager.reset(new ContentStoreManager(m_forwarder->getCs(), 
                                                      *m_dispatcher,
                                                      *m_validator));
#endif // CACHE_EXTENSIONS

  ConfigFile config(&ignoreRibAndLogSections);
  general::setConfigFile(config);

  TablesConfigSection tablesConfig(m_forwarder->getCs(),
                                   m_forwarder->getPit(),
                                   m_forwarder->getFib(),
                                   m_forwarder->getStrategyChoice(),
                                   m_forwarder->getMeasurements(),
                                   m_forwarder->getNetworkRegionTable());
  tablesConfig.setConfigFile(config);

  m_validator->setConfigFile(config);

  m_faceManager->setConfigFile(config);
#endif // NDNSIM

  // parse config file
  if (!m_configFile.empty()) {
    config.parse(m_configFile, true);
    config.parse(m_configFile, false);
  }
  else {
    config.parse(m_configSection, true, INTERNAL_CONFIG);
    config.parse(m_configSection, false, INTERNAL_CONFIG);
  }

  tablesConfig.ensureTablesAreConfigured();

  // add FIB entry for NFD Management Protocol

/* NOTE: This merge of both codebases allows to directly copy file content between NFD to ndnSIM 
 */
#ifdef NDNSIM
  shared_ptr<fib::Entry> entry = m_forwarder->getFib().insert("/localhost/nfd").first;
  entry->addNextHop(m_internalFace, 0);
#else
  Name topPrefix("/localhost/nfd");
  auto entry = m_forwarder->getFib().insert(topPrefix).first;
  entry->addNextHop(m_internalFace, 0);
  m_dispatcher->addTopPrefix(topPrefix, false);
#endif // NDNSIM
}

void
Nfd::reloadConfigFile()
{
  // Logging
  initializeLogging();
  /// \todo Reopen log file
  
  // XXX Missing mobility support !

  // Other stuff
  ConfigFile config(&ignoreRibAndLogSections);

  general::setConfigFile(config);

  TablesConfigSection tablesConfig(m_forwarder->getCs(),
                                   m_forwarder->getPit(),
                                   m_forwarder->getFib(),
                                   m_forwarder->getStrategyChoice(),
/* NOTE: This merge of both codebases allows to directly copy file content between NFD to ndnSIM 
 */
#ifdef NDNSIM
                                   m_forwarder->getMeasurements());
#else
                                   m_forwarder->getMeasurements(),
                                   m_forwarder->getNetworkRegionTable());
#endif // NDNSIM

  tablesConfig.setConfigFile(config);

/* NOTE: This merge of both codebases allows to directly copy file content between NFD to ndnSIM 
 */
#ifdef NDNSIM
  m_internalFace->getValidator().setConfigFile(config);
#else
  m_validator->setConfigFile(config);
#endif // NDNSIM
  m_faceManager->setConfigFile(config);

  if (!m_configFile.empty()) {
    config.parse(m_configFile, false);
  }
  else {
    config.parse(m_configSection, false, INTERNAL_CONFIG);
  }
}

void
Nfd::reloadConfigFileFaceSection()
{
  // reload only face_system section of the config file to re-initialize multicast faces
  ConfigFile config(&ConfigFile::ignoreUnknownSection);
  m_faceManager->setConfigFile(config);

  if (!m_configFile.empty()) {
    config.parse(m_configFile, false);
  }
  else {
    config.parse(m_configSection, false, INTERNAL_CONFIG);
  }
}

} // namespace nfd
