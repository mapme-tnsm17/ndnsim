/* in_path_caching: testing the impact of cache during producer mobility
 *
 * Author: Jordan Augé <jordan.auge@free.fr>
 *
 * TODO:
 *  - Add caching instrumentation (basic tracing not sufficient)
 *  - Add dynamic workload (Luca)
 *  - Add multipath forwarding (Luca)
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/lte-module.h"
#include "ns3/config-store.h"
#include "ns3/mobility-helper.h"


#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
// https://www.nsnam.org/docs/models/html/lte-user.html#basic-simulation-program

//#include "ndn-stopping-producer.hpp"

// for LinkStatusControl::FailLinks and LinkStatusControl::UpLinks
//#include "ns3/ndnSIM/helper/ndn-link-control-helper.hpp"

// For updating routing
#define private public
#include "ns3/ndnSIM/model/ndn-global-router.hpp"
#undef private

#include "daemon/mgmt/fib-manager.hpp"
#include "ns3/ndnSIM/model/ndn-l3-protocol.hpp"

#include "ns3/ndnSIM/helper/ndn-link-control-helper.hpp"

#include <fstream>
#include  <iomanip>

//for elastic traffic applications
//#include "ns3/ndnSIM/apps/ndn-consumer-elastic-traffic.hpp"

const std::string PREFIX = "/prefix";
//const std::string PREFIX_DOWN = "/prefix_down";

//for simuation running time logging
#include <time.h>

namespace ns3 {
namespace ndn {
 
  std::shared_ptr<std::ofstream> os(new std::ofstream());
  unsigned int fileSize=40000;
  unsigned int transitionTime=0;
  std::string scenario="-50M-to-10M-";
  double switchTime=50.0;
  double simulationStop=300.0;
void
setDefaultParameters()
{
  // setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("10Mbps")); // LINK CAPACITIES
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms")); // RTT
  Config::SetDefault("ns3::DropTailQueue::MaxPackets", StringValue("20")); // XXX BUFFER SIZE
}

NodeContainer
setupWired(NodeContainer & nodeC)
{
    NodeContainer nodeW, nodes;
    nodeW.Create (1);
    nodes.Add(nodeC.Get(0));
    nodes.Add(nodeW.Get(0));

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
    pointToPoint.Install (nodes);

    return nodeW;
}

NodeContainer
setupWifi(NodeContainer & nodeC)
{
  // wifi nodes (infrastructure)
  // https://www.nsnam.org/docs/models/html/wifi.html
  NodeContainer nodeW;
  nodeW.Create (1);

  std::string phyMode ("DsssRate1Mbps");

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  // reference loss must be changed since 802.11b is operating at 2.4GHz
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel",
          "Exponent", DoubleValue (3.0),
          "ReferenceLoss", DoubleValue (40.0459));
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add a non-QoS upper mac, and disable rate control
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
          "DataMode",StringValue (phyMode),
          "ControlMode",StringValue (phyMode));

  // Setup the rest of the upper mac
  Ssid ssid = Ssid ("wifi-default");
  // setup ap.
  wifiMac.SetType ("ns3::ApWifiMac",
          "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (wifiPhy, wifiMac, nodeC);
  NetDeviceContainer devices = apDevice;

  // setup sta.
  wifiMac.SetType ("ns3::StaWifiMac",
          "Ssid", SsidValue (ssid),
          "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice = wifi.Install (wifiPhy, wifiMac, nodeW);
  devices.Add (staDevice);

  // Configure mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  positionAlloc->Add (Vector (0.0, 5.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodeW);

  return nodeW;
}

/**
 * \brief Setup LTE links
 */
void
setupLTE()
{
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();

  // Create Node objects for the eNB(s) and the UEs
  NodeContainer enbNodes;
  enbNodes.Create (1);
  NodeContainer ueNodes;
  ueNodes.Create (2);

  // Configure the Mobility model for all the nodes:
  // The above will place all nodes at the coordinates (0,0,0). Please refer to
  // the documentation of the ns-3 mobility model for how to set your own
  // position or configure node movement.
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (enbNodes);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (ueNodes);

  // Install an LTE protocol stack on the eNB(s):
  NetDeviceContainer enbDevs;
  enbDevs = lteHelper->InstallEnbDevice (enbNodes);
  // Install an LTE protocol stack on the UEs:
  NetDeviceContainer ueDevs;
  ueDevs = lteHelper->InstallUeDevice (ueNodes);

  // Attach the UEs to an eNB. This will configure each UE according to the eNB
  // configuration, and create an RRC connection between them:
  lteHelper->Attach (ueDevs, enbDevs.Get (0));

  // Activate a data radio bearer between each UE and the eNB it is attached to:
  enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
  EpsBearer bearer (q);
  lteHelper->ActivateDataRadioBearer (ueDevs, bearer);

}


void
printWindowTrace(uint32_t appId, double curwindow, unsigned winpending)
{
   *os << std::fixed<< std::setprecision(6)<<Simulator::Now().ToDouble(Time::S) << "\t" /*<< m_node << "\t" */<< appId 
       <<"\t" <<curwindow<<"\t" <<winpending<<"\n";
}

void
setupConsumer(NodeContainer & node, const std::string & prefix, double lambda, std::string & catalog_size, std::string alpha)
{
  std::string lambda_s = boost::lexical_cast<std::string>(lambda);

  // Customer requesting the disappearing prefix

//   ndn::AppHelper h("ns3::ndn::ConsumerZipfMandelbrot");
// 
//   h.SetPrefix(prefix);
//   h.SetAttribute("Frequency", StringValue(lambda_s)); // 100 i/s
//   h.SetAttribute("Randomize", StringValue("exponential"));
//   h.SetAttribute("NumberOfContents", StringValue(catalog_size));
//   h.SetAttribute("q", StringValue("0")); // for a typical Zipf distribution
//   h.SetAttribute("s", StringValue(alpha));
//   h.Install(node);

//   ndn::AppHelper elasticTrafficConsumer("ns3::ndn::ConsumerElasticTraffic");
//   elasticTrafficConsumer.SetPrefix(prefix);
//   elasticTrafficConsumer.SetAttribute("ArrivalRate", StringValue(lambda_s)); // 100 i/s
//   elasticTrafficConsumer.SetAttribute("CatalogSize", StringValue(catalog_size));
//   //elasticTrafficConsumer.SetAttribute("q", StringValue("0")); // for a typical Zipf distribution
//   elasticTrafficConsumer.SetAttribute("s", StringValue(alpha));
//   
//   elasticTrafficConsumer.SetAttribute("WindowTraceFile",StringValue("cc-stability-window-trace-branch-P1a.txt"));
//   
//   elasticTrafficConsumer.Install(node);
     
    // ndn::AppHelper elastic("ns3::ndn::ConsumerElasticTraffic");
     //elastic.SetPrefix(prefix);
     //raaqmConsumerHelper.SetAttribute("MaxSeq",IntegerValue(50000) );
     //raaqmConsumerHelper.SetAttribute("NewRetxTimer",StringValue("10s"));
     //elastic.SetAttribute("WindowTraceFile",StringValue("results/40K.txt"));
  
  
  
   
      bool isOK=true;
      using namespace boost;
    using namespace std;
    std::string m_windowTraceFile;

 //std::string m_windowTraceFile="results/data/lambda"+lambda_s+"-catalog-"+catalog_size+"-alpha-"+alpha+"-chunks-"+std::to_string(fileSize)+"-5M-to-1M-transition-3s-raaqm";;
    if(transitionTime>0)
      m_windowTraceFile="results/data/winTrace-chunks-"+std::to_string(fileSize)+scenario+"transition-"+std::to_string(transitionTime)+"s-raaqm-loadbalance-cache";
    else
      m_windowTraceFile="results/data/winTrace-chunks-"+std::to_string(fileSize)+scenario+"raaqm-loadbalance-cache";
    //std::string m_windowTraceFile="test.txt";
    os->open(m_windowTraceFile.c_str(), std::ios_base::out | std::ios_base::trunc);

    if (!os->is_open()) {
      std::cout<<"File " << m_windowTraceFile << " cannot be opened for writing. Tracing disabled\n";
      isOK=false;
    }
   

 
  if(isOK)
  {
    std::cout<<"window traces are logged"<<"\n";
  
  *os<< "Time"
     << "\t"
     << "AppId" 
     << "\t"
     << "currentWindow"
      << "\t"
     << "pendingWindow"
     << "\n";
  }
  else
    std::cout<<"window traces are NOT logged"<<"\n";

  ndn::AppHelper raaqmConsumerHelper("ns3::ndn::ConsumerWindowRAAQM");
  
  raaqmConsumerHelper.SetAttribute("MaxSeq",IntegerValue(fileSize));
  
  ApplicationContainer newDownloadApp;
  for(int i=1;i<11;i++)
  {
    
    raaqmConsumerHelper.SetPrefix(prefix+"/OBJNUM"+std::to_string(i));
    if(i==5 || i==1)
    {
     newDownloadApp.Add(raaqmConsumerHelper.Install(node));
    }
    else
    {
    raaqmConsumerHelper.Install(node);
    }
  }
  
//     raaqmConsumerHelper.SetPrefix(prefix+"/OBJNUM"+std::to_string(10));
//   ApplicationContainer newDownloadApp=raaqmConsumerHelper.Install(node);
  
 
  //   ApplicationContainer newDownloadApp=elastic.Install(node);
     if(static_cast<bool>(os))
  {
    newDownloadApp.Get(0)->TraceConnectWithoutContext("WindowTrace", MakeCallback(&printWindowTrace));
    newDownloadApp.Get(1)->TraceConnectWithoutContext("WindowTrace", MakeCallback(&printWindowTrace));
  }

}

ApplicationContainer
setupProducer(NodeContainer & node, const std::string & prefix)
{
  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  producerHelper.SetPrefix(prefix);
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  return producerHelper.Install(node);
}

void
setupRoutingAndForwarding(NodeContainer & nodeP1)
{
  /* ROUTING: global routing helper */
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll();
  // Add /prefix origins to ndn::GlobalRouter
  ndnGlobalRoutingHelper.AddOrigins(PREFIX, nodeP1);
  // Calculate and install FIB
  ndn::GlobalRoutingHelper::CalculateRoutes();
  
  // Set BestRoute strategy (no need for ndnHelper)
  //ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/best-route"); 
  ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/load-balance");
}

void
updateRouting(NodeContainer & nodeP1)
{
//  shared_ptr<Name> prefix = make_shared<Name>(PREFIX);

//   for (NodeList::Iterator node = NodeList::Begin(); node != NodeList::End(); node++) {
// 
//     Ptr<GlobalRouter> gr = (*node)->GetObject<GlobalRouter>();
//     gr->m_localPrefixes.clear();
//     //gr->AddLocalPrefix(prefix);
//     
// 
//     Ptr<L3Protocol> l3 = (*node)->GetObject<L3Protocol>();
//     shared_ptr<nfd::FibManager> fibManager = l3->getFibManager();
// 
//     /* Since we don't know the face, we will try to remove the next hop for all
//      * faces */
//     for (auto& i : l3->getForwarder()->getFaceTable()) {
//         shared_ptr<Face> nfdFace = std::dynamic_pointer_cast<Face>(i);
// 
//         ControlParameters parameters;
//         parameters.setName(*prefix);
//         parameters.setFaceId(nfdFace->getId());
//         //parameters.setCost(metric);
// 
//         // Because this function is private, and there is no public RemoveRoute
//         // function... we need to recode everything. Crap ndnSIM...
// 
//         // BEGIN: FibHelper::RemoveNextHop(parameters, *node);
//         Block encodedParameters(parameters.wireEncode());
// 
//         Name commandName("/localhost/nfd/fib");
//         commandName.append("remove-nexthop");
//         commandName.append(encodedParameters);
// 
//         shared_ptr<Interest> command(make_shared<Interest>(commandName));
//         StackHelper::getKeyChain().sign(*command);
// 
//         fibManager->onFibRequest(*command);
//         // END: FibHelper::RemoveNextHop(parameters, *node);
// 
//     }
//   }
// 
//   // we have cleared all routes
//   //
//   // Let's first start testing without the need to reset routes
// 
//   ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
//   ndnGlobalRoutingHelper.AddOrigin(PREFIX, nodeP1.Get(0));
// 
//   ndn::GlobalRoutingHelper::CalculateRoutes();
  
  
  // Manually configure FIB routes
  ndn::FibHelper::AddRoute("LeftRoot", PREFIX, "MidBottom", 1);
    ndn::FibHelper::AddRoute("MidBottom", PREFIX, "RightBottom", 1);

     ndn::FibHelper::AddRoute("RightBottom", PREFIX, "NodeP1b", 1);

  
  /* NOTE: we need to setup the producer here, if we do it in the setup like the
   * other, then it does not answer */
  setupProducer(nodeP1, PREFIX);
}

int
main(int argc, char* argv[])
{
  double lambda = 100; // i / s

  std::string catalog_size = "10000";
  std::string alpha = "0.8";

  setDefaultParameters();

  /* ConfigStore and CommandLine parsing */
  CommandLine cmd;
  cmd.AddValue ("lambda", "Content requests arrival rate", lambda);
  cmd.AddValue ("filesize", "filesize", fileSize);
  cmd.AddValue ("transitionTime", "the routing update convergence time", transitionTime);
  cmd.Parse(argc, argv);
  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();
  cmd.Parse (argc, argv);
 

  /* Topology */
  AnnotatedTopologyReader topologyReader("", 25); // 25 ?
  topologyReader.SetFileName("src/ndnSIM/examples/topologies/two-branches.txt");
  topologyReader.Read();
//print node ID info
  std::cout<<"LeftRoot node's ID is "<< Names::Find<Node>("LeftRoot")->GetId()<<"\n";
  
  NodeContainer nodeP1a, nodeP1b, nodeC;
  nodeP1a.Add(Names::Find<Node>("NodeP1a"));
  nodeP1b.Add(Names::Find<Node>("NodeP1b"));
  nodeC.Add(Names::Find<Node>("NodeC"));

  // Adding wired/wireless customers
  //NodeContainer nodeW = setupWired(nodeC);
  NodeContainer nodeW = setupWifi(nodeC);

  /* NDN stack */
  ndn::StackHelper ndnHelper;
  //ndnHelper.SetDefaultRoutes(true);
  ndnHelper.SetOldContentStore("ns3::ndn::cs::Lru", "MaxSize", "1");
  ndnHelper.InstallAll();

  
  /*************************************************************/
  /* Traffic model */
  ApplicationContainer oldProducer=setupProducer(nodeP1a, PREFIX);
  oldProducer.Stop(Seconds(switchTime));
  
  // XXX If we setup this producer only here, it does not reply at all...
  //setupProducer(nodeP1b, PREFIX);

  setupConsumer(nodeC, PREFIX,      lambda / 2, catalog_size, alpha);
  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  producerHelper.SetPrefix(PREFIX+"/OBJNUM1");
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.Install(Names::Find<Node>("RightTop"));
  
  setupRoutingAndForwarding(nodeP1a);

  /* Events */
   //if(transitionTime>0)
   //  Simulator::Schedule(Seconds(switchTime), ndn::LinkControlHelper::FailLink, Names::Find<Node>("RightTop"), Names::Find<Node>("NodeP1a"));
  

  Simulator::Schedule(Seconds(switchTime+transitionTime), &updateRouting, nodeP1b);

// XXX Syntax error
//  Simulator::Schedule(Seconds(10.0), &setupProducer, nodeP1b, PREFIX);

  Simulator::Stop(Seconds(simulationStop));
  /***************************************************************/
  
   
   clock_t begin, end;
   double time_spent;
begin = clock(); 
if(transitionTime>0)
{
  ndn::L3RateTracer::InstallAll("results/data/rateTrace-chunks-"+std::to_string(fileSize)+scenario+"transition-"+std::to_string(transitionTime)+"s-raaqm-loadbalance-cache", Seconds(0.5));
  L2RateTracer::InstallAll("results/data/dropRateTrace-chunks-"+std::to_string(fileSize)+scenario+"transition-"+std::to_string(transitionTime)+"s-raaqm-loadbalance-cache", Seconds(0.5));
}
else
{
  ndn::L3RateTracer::InstallAll("results/data/rateTrace-chunks-"+std::to_string(fileSize)+scenario+"raaqm-loadbalance-cache", Seconds(0.5));
  L2RateTracer::InstallAll("results/data/dropRateTrace-chunks-"+std::to_string(fileSize)+scenario+"raaqm-loadbalance-cache", Seconds(0.5));
}
  Simulator::Run();
  Simulator::Destroy();
  
end = clock();
time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
std::cout<<"simulation time is "<<time_spent<<"s\n";


  return 0;
}

} // namesapce ndn
} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::ndn::main(argc, argv);
}
