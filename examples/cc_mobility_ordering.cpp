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
 *
 *Authors: Natalya Rozhnova <rozhnova.natalya@gmail.com>
 **/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

//#include "ns3/ndnSIM/apps/ndn-consumer-elastic-traffic.hpp"

#include "ns3/ndnSIM-module.h"
#include "helper/ndn-fib-helper.hpp"
#include "ns3/ndnSIM/apps/ndn-producer.hpp"
#include "ns3/ndnSIM/apps/ndn-consumer-cbr.hpp"

#include "ns3/waypoint.h"


using namespace std;
namespace ns3 {

NS_LOG_COMPONENT_DEFINE("ndn.CC_mobility_ordering");

std::stringstream filePlotQueueR1;
std::stringstream filePlotQueueR2;
std::stringstream filePlotQueueR3;
std::stringstream filePlotQueueB1;
std::stringstream filePlotQueueB2;
std::stringstream filePlotQueueProducer;
std::stringstream fileDataPktDropR2;


void
PktDrop(Ptr<const Packet> p)
{
	std::ofstream fPlotQueue (fileDataPktDropR2.str ().c_str (), std::ios::out|std::ios::app);
	fPlotQueue << Simulator::Now ().GetSeconds ()
			    		 << " -drop "<< p
			    		 << std::endl;
	fPlotQueue.close ();
}

void
CheckQueueSizeR1 (Ptr<Queue> queueCh, Ptr<Queue> queueI1, Ptr<Queue> queueI2)
{
	uint32_t DqtotalSizeCh = queueCh->GetNPackets();
	uint32_t DqtotalSizeChBytes = queueCh->GetNBytes();
	uint32_t DqtotalSizeI1 = queueI1->GetNPackets();
	uint32_t DqtotalSizeI1Bytes = queueI1->GetNBytes();
	uint32_t DqtotalSizeI2 = queueI2->GetNPackets();
	uint32_t DqtotalSizeI2Bytes = queueI2->GetNBytes();

 // check queue size every 1/100 of a second
  Simulator::Schedule (Seconds (0.01), &CheckQueueSizeR1, queueCh, queueI1, queueI2);

  std::ofstream fPlotQueue (filePlotQueueR1.str ().c_str (), std::ios::out|std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds ()
		     << " @Chunks "<< queueCh
		     <<"\t-Pkts\t" <<DqtotalSizeCh
		     <<"\t-Bytes\t"<<DqtotalSizeChBytes
		     <<"\t@Int "<<queueI1<<"&"<<queueI2<<"\t"
		     <<"\tIntR1-R2_Pkts\t"<<DqtotalSizeI1
		     <<"\tIntR1-R2_Bytes\t"<<DqtotalSizeI1Bytes
		     <<"\tIntR1-R3_Pkts\t"<<DqtotalSizeI2
		     <<"\tIntR1-R3_Pkts\t"<<DqtotalSizeI2Bytes
		     << std::endl;
  fPlotQueue.close ();
}

void
CheckQueueSizeR2 (Ptr<Queue> queueCh, Ptr<Queue> queueI)
{
	uint32_t DqtotalSizeCh = queueCh->GetNPackets();
	uint32_t DqtotalSizeChBytes = queueCh->GetNBytes();
	uint32_t DqtotalSizeI = queueI->GetNPackets();
	uint32_t DqtotalSizeIBytes = queueI->GetNBytes();

 // check queue size every 1/100 of a second
  Simulator::Schedule (Seconds (0.01), &CheckQueueSizeR2, queueCh, queueI);

  std::ofstream fPlotQueue (filePlotQueueR2.str ().c_str (), std::ios::out|std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds ()
		     << " @Chunks "<< queueCh
		     <<"\t-Pkts\t" <<DqtotalSizeCh
		     <<"\t-Bytes\t"<<DqtotalSizeChBytes
		     <<"\t@Int\t"<<queueI
		     <<"\tIntR2-B1_Pkts\t"<<DqtotalSizeI
		     <<"\tIntR2-B1_Bytes\t"<<DqtotalSizeIBytes
		     << std::endl;
  fPlotQueue.close ();
}

void
CheckQueueSizeR3 (Ptr<Queue> queueCh, Ptr<Queue> queueI)
{
	uint32_t DqtotalSizeCh = queueCh->GetNPackets();
	uint32_t DqtotalSizeChBytes = queueCh->GetNBytes();
	uint32_t DqtotalSizeI = queueI->GetNPackets();
	uint32_t DqtotalSizeIBytes = queueI->GetNBytes();

 // check queue size every 1/100 of a second
  Simulator::Schedule (Seconds (0.01), &CheckQueueSizeR3, queueCh, queueI);

  std::ofstream fPlotQueue (filePlotQueueR3.str ().c_str (), std::ios::out|std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds ()
		     << " @Chunks "<< queueCh
		     <<"\t-Pkts\t" <<DqtotalSizeCh
		     <<"\t-Bytes\t"<<DqtotalSizeChBytes
		     <<"\t@Int\t"<<queueI
		     <<"\tIntR3-B2_Pkts\t"<<DqtotalSizeI
		     <<"\tIntR3-B2_Bytes\t"<<DqtotalSizeIBytes
		     << std::endl;
  fPlotQueue.close ();
}

void
CheckQueueSizeB1 (Ptr<Queue> queueCh, Ptr<WifiMacQueue> queueI)
{
	uint32_t DqtotalSizeCh = queueCh->GetNPackets();
	uint32_t DqtotalSizeChBytes = queueCh->GetNBytes();
	uint32_t DqtotalSizeI = queueI->GetSize();
//	uint32_t DqtotalSizeIBytes = queueI->GetNBytes();

 // check queue size every 1/100 of a second
  Simulator::Schedule (Seconds (0.01), &CheckQueueSizeB1, queueCh, queueI);

  std::ofstream fPlotQueue (filePlotQueueB1.str ().c_str (), std::ios::out|std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds ()
		     << " @Chunks "<< queueCh
		     <<"\t-Pkts\t" <<DqtotalSizeCh
		     <<"\t-Bytes\t"<<DqtotalSizeChBytes
		     <<"\t@Int\t"<<queueI
		     <<"\tIntB1-P_Pkts\t"<<DqtotalSizeI
//		     <<"\tIntB1-P_Bytes\t"<<DqtotalSizeIBytes
		     << std::endl;
  fPlotQueue.close ();
}

void
CheckQueueSizeB2 (Ptr<Queue> queueCh, Ptr<WifiMacQueue> queueI)
{
	uint32_t DqtotalSizeCh = queueCh->GetNPackets();
	uint32_t DqtotalSizeChBytes = queueCh->GetNBytes();
	uint32_t DqtotalSizeI = queueI->GetSize();
//	uint32_t DqtotalSizeIBytes = queueI->GetNBytes();

 // check queue size every 1/100 of a second
  Simulator::Schedule (Seconds (0.01), &CheckQueueSizeB2, queueCh, queueI);

  std::ofstream fPlotQueue (filePlotQueueB2.str ().c_str (), std::ios::out|std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds ()
		     << " @Chunks "<< queueCh
		     <<"\t-Pkts\t" <<DqtotalSizeCh
		     <<"\t-Bytes\t"<<DqtotalSizeChBytes
		     <<"\t@Int\t"<<queueI
		     <<"\tIntB1-P_Pkts\t"<<DqtotalSizeI
//		     <<"\tIntB1-P_Bytes\t"<<DqtotalSizeIBytes
		     << std::endl;
  fPlotQueue.close ();
}

void
CheckQueueSizeProducer (Ptr<WifiMacQueue> queueCh)
{
	uint32_t DqtotalSizeCh = queueCh->GetSize();
//	uint32_t DqtotalSizeChBytes = queueCh->GetNBytes();

 // check queue size every 1/100 of a second
  Simulator::Schedule (Seconds (0.01), &CheckQueueSizeProducer, queueCh);

  std::ofstream fPlotQueue (filePlotQueueProducer.str ().c_str (), std::ios::out|std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds ()
		     << " @Chunks "<< queueCh
		     <<"\t-Pkts\t" <<DqtotalSizeCh
//		     <<"\t-Bytes\t"<<DqtotalSizeChBytes

		     << std::endl;
  fPlotQueue.close ();
}

void ComputeRoutes(std::string prefix, Ptr<Node> producer, bool start)
{
	if(start)
	{
		ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
		ndnGlobalRoutingHelper.InstallAll();
		ndnGlobalRoutingHelper.AddOrigins(prefix, producer);
	}
		ndn::GlobalRoutingHelper::CalculateRoutes();

//	  Simulator::Schedule (Seconds (2), &ComputeRoutes, prefix, producer, false);

}

void RecomputeRoutes(std::string prefix, Ptr<Node> producer)
{
	ndn::GlobalRoutingHelper::CalculateRoutes();
	Simulator::Schedule (Seconds (2), &RecomputeRoutes, prefix, producer);
}


void SetSignalFaces(Ptr<Node> a, Ptr<Node> b)
{
	  for (uint32_t deviceId = 0; deviceId < a->GetNDevices(); deviceId++)
	  {
	      Ptr<PointToPointNetDevice> netDevice = DynamicCast<PointToPointNetDevice>(a->GetDevice(deviceId));
	      if (netDevice == 0)
	        continue;

	      Ptr<Channel> channel = netDevice->GetChannel();
	      if (channel == 0)
	        continue;

	      if (channel->GetDevice(0)->GetNode() == b
	          || channel->GetDevice(1)->GetNode() == b)
	      {
	        Ptr<ndn::L3Protocol> ndn = a->GetObject<ndn::L3Protocol>();

//	        shared_ptr<ndn::Face> face = ndn->getFaceByNetDevice(netDevice);
//	        face->SetFaceAsSignal(true);
//	        std::cout<<"Signal Face "<<face<<" "<<face->getId()<<std::endl;
	        break;
	      }
	  }
}

void
SetAdHoc(Ptr<Node> base_station, Ptr<Node> base_station_1, Ptr<Node> producer, double range)
{
	// AD-HOC version
	  WifiHelper wifi = WifiHelper::Default();
	  wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
	  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode",
	                               StringValue("ErpOfdmRate54Mbps"));//OfdmRate24Mbps"));

	  YansWifiChannelHelper wifiChannel; // = YansWifiChannelHelper::Default ();
	  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
	  wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(range));
	//  Ptr<ExponentialRandomVariable> expVar = CreateObjectWithAttributes<ExponentialRandomVariable> ("Mean", DoubleValue (50.0));
	//  wifiChannel.AddPropagationLoss("ns3::RandomPropagationLossModel", "Variable", PointerValue (expVar) );


	  YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default();
	  wifiPhyHelper.SetChannel(wifiChannel.Create());
	//  wifiPhyHelper.Set("TxPowerStart", DoubleValue(5));
	//  wifiPhyHelper.Set("TxPowerEnd", DoubleValue(5));

	  NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default();
	  wifiMacHelper.SetType("ns3::AdhocWifiMac");

	  NetDeviceContainer wifiNetDevices = wifi.Install(wifiPhyHelper, wifiMacHelper, base_station);
	  NetDeviceContainer wifiNetDevicesP = wifi.Install(wifiPhyHelper, wifiMacHelper, producer);

	  YansWifiChannelHelper wifiChannel2; // = YansWifiChannelHelper::Default ();
	  wifiChannel2.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
	  wifiChannel2.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(range));
	//  Ptr<ExponentialRandomVariable> expVar2 = CreateObjectWithAttributes<ExponentialRandomVariable> ("Mean", DoubleValue (50.0));
	//  wifiChannel2.AddPropagationLoss("ns3::RandomPropagationLossModel", "Variable", PointerValue (expVar2) );

	  YansWifiPhyHelper wifiPhyHelper2 = YansWifiPhyHelper::Default();
	  wifiPhyHelper2.SetChannel(wifiChannel2.Create());

	  NetDeviceContainer wifiNetDevicesBS1 = wifi.Install(wifiPhyHelper2, wifiMacHelper, base_station_1);
	  NetDeviceContainer wifiNetDevicesP2 = wifi.Install(wifiPhyHelper2, wifiMacHelper, producer);

}

void
SetWifi(Ptr<Node> base_station, Ptr<Node> base_station_1, Ptr<Node> producer, double range, double ploss)
{
	YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
	YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
	phy.SetChannel (channel.Create ());

	WifiHelper wifi = WifiHelper::Default ();
	wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
	NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();

	wifi.SetRemoteStationManager
	("ns3::ConstantRateWifiManager","DataMode", StringValue
			("ErpOfdmRate54Mbps"), "ControlMode", StringValue ("ErpOfdmRate6Mbps"),
			"RtsCtsThreshold", UintegerValue (10000), "IsLowLatency" , BooleanValue
			(false), "NonUnicastMode" , StringValue ("ErpOfdmRate54Mbps"));//, "MaxSlrc", UintegerValue(0));

	YansWifiChannelHelper wifiChannel;
	wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
	// AP range has to be lower than half the distance between APs
	wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(range));
	Ptr<ExponentialRandomVariable> expVar = CreateObjectWithAttributes<ExponentialRandomVariable> ("Mean", DoubleValue(ploss));
	wifiChannel.AddPropagationLoss("ns3::RandomPropagationLossModel", "Variable", PointerValue (expVar) );



	YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
	wifiPhy.SetChannel (wifiChannel.Create ());

	Ssid ssid = Ssid ("wifi-default");
	YansWifiChannelHelper wifiChannel2;
	wifiChannel2.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
	// AP range has to be lower than half the distance between APs
	wifiChannel2.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(range));
	Ptr<ExponentialRandomVariable> expVar2 = CreateObjectWithAttributes<ExponentialRandomVariable> ("Mean", DoubleValue (ploss));
 	wifiChannel2.AddPropagationLoss("ns3::RandomPropagationLossModel", "Variable", PointerValue (expVar2) );


	YansWifiPhyHelper wifiPhy2 =  YansWifiPhyHelper::Default ();
	wifiPhy2.SetChannel (wifiChannel2.Create ());

	wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
	NetDeviceContainer BaseStationDevice = wifi.Install (wifiPhy, wifiMac, base_station);
	NetDeviceContainer BaseStationDevice2 = wifi.Install (wifiPhy2, wifiMac, base_station_1);

	wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue(ssid),
			"ActiveProbing", BooleanValue(false));
	NetDeviceContainer ProducerDevice = wifi.Install (wifiPhy, wifiMac, producer);
	NetDeviceContainer ProducerDevice2 = wifi.Install (wifiPhy2, wifiMac, producer);

}

void
SetWiFin(Ptr<Node> base_station, Ptr<Node> producer, double range)
{
    Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("990000"));
    // disable rts cts all the time.
    Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("99000"));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());
    phy.Set ("ShortGuardEnabled",BooleanValue(true));

    WifiHelper wifi = WifiHelper::Default ();
    wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);
    HtWifiMacHelper mac = HtWifiMacHelper::Default ();

    Ssid ssid = Ssid ("wifi-default");
    double datarate = 0;
    StringValue DataRate;

    DataRate = StringValue("OfdmRate72_2MbpsBW20MHz");
    datarate = 72.2;

    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", DataRate,
                                    "ControlMode", DataRate);
    mac.SetType ("ns3::StaWifiMac",
                  "Ssid", SsidValue (ssid),
                  "ActiveProbing", BooleanValue (false));

    NetDeviceContainer ProducerDevice = wifi.Install (phy, mac, producer);

    mac.SetType ("ns3::ApWifiMac",
                  "Ssid", SsidValue (ssid));

 	NetDeviceContainer BaseStationDevice = wifi.Install (phy, mac, base_station);



}

void
SetMobilityStrategy(std::string strategy, MobilityHelper& helper)
{
	if(strategy == "ns3::RandomDirection2dMobilityModel")
	{
		helper.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
				"Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"),
				"Speed", StringValue ("ns3::ConstantRandomVariable[Constant=5.0]"),
				"Bounds", StringValue ("80|120|70|90"));
//				"Bounds", StringValue ("80|120|90|90"));
		return;
	}
	if(strategy == "ns3::RandomWalk2dMobilityModel")
	{
		helper.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
				"Mode", StringValue ("Time"),
				"Time", StringValue ("2s"),
				"Speed", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]"),
				"Bounds", StringValue ("80|120|89|91"));
		return;
	}
	if(strategy == "ns3::ConstantPositionMobilityModel")
	{
		helper.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
		return;
	}
	return;
}

void InstallRaaqm(Ptr<Node> node,
					int sigma,
					bool path_report,
					bool abort_mode,
					std::string new_retr_timer,
					std::string retr_limit,
					std::string lambda,
					std::string catalog_size,
					std::string power,
					std::string observation,
					std::string prefix,
					std::string pathOut,
					bool rtt_red)
{
	std::cout<<"RTTreduction"<<rtt_red<<std::endl;

	  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerRAAQM");
	  consumerHelper.SetPrefix(prefix);
	  consumerHelper.SetAttribute("MaxSeq", IntegerValue(sigma));
	  consumerHelper.SetAttribute("EnablePathReport", BooleanValue(path_report));
	  consumerHelper.SetAttribute("EnableAbortMode",BooleanValue(abort_mode));
//	  consumerHelper.SetAttribute("NewRetxTimer", StringValue(new_retr_timer));
	  consumerHelper.SetAttribute("RetrxLimit", StringValue(retr_limit));
	  consumerHelper.SetAttribute("ObservationPeriod", StringValue(observation));
	  consumerHelper.SetAttribute("Beta", StringValue("0.8"));
	  consumerHelper.SetAttribute("DropFactor", StringValue("0.005"));
	  consumerHelper.SetAttribute("PathForTrace", StringValue(pathOut));
	  consumerHelper.SetAttribute("DefaultInterestLifeTime", StringValue(new_retr_timer));
	  consumerHelper.SetAttribute("SeedValue", UintegerValue(123));
	  consumerHelper.SetAttribute("InitialWindow", DoubleValue(30.0));
	  consumerHelper.SetAttribute("UseRTTreduction", BooleanValue(rtt_red));
	  consumerHelper.SetAttribute("EnableTracing", BooleanValue(true));

	  consumerHelper.Install(node);
/*
	  Ptr<ndn::ConsumerWindowRAAQM> app = DynamicCast<ndn::ConsumerWindowRAAQM>(node->GetApplication(0));

	  app->SetTraceFileName(pathOut, "ConsumerRAAQM.plotme");
*/
/*
	  ndn::AppHelper elasticTrafficHelper("ns3::ndn::ConsumerElasticTraffic");
	  elasticTrafficHelper.SetPrefix(prefix);
	  elasticTrafficHelper.SetAttribute("ArrivalRate", StringValue(lambda)); // 100 i/s
	  elasticTrafficHelper.SetAttribute("CatalogSize", StringValue(catalog_size));
	  elasticTrafficHelper.SetAttribute("s", StringValue(power));
	  elasticTrafficHelper.SetAttribute("WindowTraceFile", StringValue("WindowTraceFile.plotme"));
	  elasticTrafficHelper.SetAttribute("FlowStatsTraceFile", StringValue("FlowStatsTraceFile.plotme"));

	  ApplicationContainer myApps=elasticTrafficHelper.Install(node);

	  for (uint32_t i = 0; i < myApps.GetN(); i++) {
	         ndn::ConsumerElasticTraffic *myApp=dynamic_cast<ndn::ConsumerElasticTraffic*>(myApps.Get(i).operator->());
	         myApp->SetDownloadApplication(consumerHelper);
	         myApp->SetPopularityType("zipf");
	  }
*/

}

int
main(int argc, char* argv[])
{
	SeedManager::SetSeed (12345);
	std::string RSmin("0"), RSmax("0");
	int sigma = 100;
	bool abort_mode = false;
	bool path_report = false;
	std::string new_retr_timer = "50ms";
	std::string retr_limit = "1";
	std::string lambda = "90";
	std::string catalog_size = "1";
	std::string power = "0.8";
	std::string payload = "1200";
	std::string prefix = "/prefix";
	std::string observation = "0.1";
	std::string pathOut = ".";
	bool trace_queues = false;
	bool rtt_red = false;
	double ploss = 15.0;
	std::string r1r2capacity = "20Mbps";
	std::string wiredQLength = "10000";
	uint32_t wirelessQLength = 10000;

	CommandLine cmd;
	cmd.AddValue ("MaxSeq", "Maximum sequence number to request", sigma);
	cmd.AddValue ("Ab_mode", "Enable abort mode [true/false]", abort_mode);
	cmd.AddValue ("PathRep", "EnablePathReport", path_report);
	cmd.AddValue ("NewRetrTimer", "Timeout defining how frequent retransmission timeouts should be checked [e.g. 10ms]", new_retr_timer);
	cmd.AddValue ("RetrxLimit", "RetrxLimit=n means maximum n-1 retransmit is allowed", retr_limit);
	cmd.AddValue ("ArrivalRate", "File download rate [files/second]", lambda);
	cmd.AddValue ("CatalogSize", "Number of the objects in the catalog", catalog_size);
	cmd.AddValue ("Power", "Distribution law power parameter", power);
	cmd.AddValue ("Payload", "Producer payload", payload);
	cmd.AddValue ("Prefix", "Prefix name", prefix);
	cmd.AddValue ("ObPeriod", "Observation period for tracing in seconds", observation);
	cmd.AddValue ("PathOut", "Path for output trace files", pathOut);
	cmd.AddValue ("TraceQueue", "Trace the queue size of every router", trace_queues);
	cmd.AddValue ("RTTred", "Use RTT reduction scheme", rtt_red);
	cmd.AddValue ("LossProbability", "Mean value for exponential loss distribution",  ploss);
	cmd.AddValue ("R1R2Capacity", "Link capacity for the link netween the nodes R1 and R2", r1r2capacity);
	cmd.AddValue ("WiredQL", "To set a wired bottleneck queue length", wiredQLength);
	cmd.AddValue ("WirelessQL", "To set a wireless bottleneck queue length", wirelessQLength);

	cmd.Parse (argc, argv);

//Create nodes and configure their location------------------------------------------------------------------------

  NodeContainer wireless;
  wireless.Create(3);
  NodeContainer wired;
  wired.Create(5);
  Ptr<Node> consumer = wired.Get(0);
  Ptr<Node> r1 = wired.Get(1);
  Ptr<Node> r2 = wired.Get(2);
  Ptr<Node> r3 = wired.Get(3);
//  Ptr<Node> cross_consumer = wired.Get(5);
  Ptr<Node> base_station = wireless.Get(0);
  Ptr<Node> base_station_1 = wireless.Get(1);
  Ptr<Node> producer = wireless.Get(2);

  NodeContainer base_stations;
  base_stations.Add(base_station);
  base_stations.Add(base_station_1);


  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (100.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (50.0),
                                   "DeltaY", DoubleValue (20.0),
                                   "GridWidth", UintegerValue (1),
                                   "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");


  MobilityHelper mobility_r;
  mobility_r.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (80.0),
                                   "MinY", DoubleValue (60.0),
                                   "DeltaX", DoubleValue (40.0),
                                   "DeltaY", DoubleValue (20.0),
                                   "GridWidth", UintegerValue (2),
                                   "LayoutType", StringValue ("RowFirst"));

  mobility_r.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  MobilityHelper mobility_p;
  mobility_p.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (90.0),
                                   "MinY", DoubleValue (90.0),
                                   "DeltaX", DoubleValue (50.0),
                                   "DeltaY", DoubleValue (20.0),
                                   "GridWidth", UintegerValue (1),
                                   "LayoutType", StringValue ("RowFirst"));

//  SetMobilityStrategy("ns3::RandomDirection2dMobilityModel", mobility_p);
  SetMobilityStrategy(" ns3::ConstantPositionMobilityModel", mobility_p);
//  SetMobilityStrategy("ns3::WaypointMobilityModel", mobility_p);

//END of location configuration----------------------------------------------------------------------------------------------


//  SetWiFin(base_station, producer, 20);
  std::cout<<"ploss "<<ploss<<std::endl;

  SetWifi(base_station, base_station_1, producer, 20, ploss);
//SetAdHoc(base_station, base_station_1, producer, 200);

      mobility.Install(consumer);
      mobility.Install(r1);
      mobility_r.Install(r2);
      mobility_r.Install(r3);
      mobility_r.Install(base_stations);
      mobility_p.Install(producer);
/*      Ptr<WaypointMobilityModel> model = producer->GetObject<WaypointMobilityModel>();
      NS_ASSERT (model);
      model->AddWaypoint(Waypoint(Seconds(2.0), Vector(120,90)));
*/

// Setup wired -----------------------------------------------------------------------------------------
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("300Mbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::DropTailQueue::MaxPackets", StringValue("10000"));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("30Mbps"));

  p2p.Install(consumer, r1);

  std::cout<<"Queue Wired "<<wiredQLength<<std::endl;
  Config::SetDefault("ns3::DropTailQueue::MaxPackets", StringValue(wiredQLength));

  std::cout<<"r1r2capacity "<<r1r2capacity<<std::endl;
  PointToPointHelper p2pr1r2;
  p2pr1r2.SetDeviceAttribute ("DataRate", StringValue (r1r2capacity));//5Mbps"));
  p2pr1r2.Install(r1, r2);

  PointToPointHelper p2pr1r3;
  p2pr1r3.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  p2pr1r3.Install(r1, r3);

  PointToPointHelper p2pb1b2;
  p2pb1b2.SetDeviceAttribute ("DataRate", StringValue ("200Mbps"));
  p2pb1b2.Install(r2, base_station);
  p2pb1b2.Install(r3, base_station_1);

  PointToPointHelper p2pbase;
  p2pbase.SetDeviceAttribute ("DataRate", StringValue ("500Mbps"));
  NetDeviceContainer signal_faces = p2pbase.Install(base_station, base_station_1);
/*
  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute ("DataRate", StringValue ("54Mbps"));

  p2p2.Install(base_station_1, producer);
*/
  // 3. Install NDN stack
  NS_LOG_INFO("Installing NDN stack");
  ndn::StackHelper ndnHelper;
  ndnHelper.SetOldContentStore("ns3::ndn::cs::Lru", "MaxSize", "1");
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.Install(wired);
  ndnHelper.Install(wireless);


  // Set strategy
//  ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/load-balance");
//    ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/best-route/%FD%02");
//  ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/broadcast");
  ndn::StrategyChoiceHelper::Install(consumer, "/", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::Install(r1, "/", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::Install(r2, "/", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::Install(r3, "/", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::Install(producer, "/", "/localhost/nfd/strategy/best-route");
//  ndn::StrategyChoiceHelper::Install(base_stations, "/", "/localhost/nfd/strategy/mobility");

  ndn::StrategyChoiceHelper::Install(base_stations, "/", "/localhost/nfd/strategy/bsrscheme");
//  ndn::StrategyChoiceHelper::AllowBSRetransmissions(base_station, "/localhost/nfd/strategy/bsrscheme", 3);
  ndn::StrategyChoiceHelper helper;
  helper.AllowBSRetransmissions(base_station, "/localhost/nfd/strategy/bsrscheme", 0);

  SetSignalFaces(base_station, base_station_1);
  SetSignalFaces(base_station_1, base_station);

//START the Consumer configuration-------------------------------------------------------------------------------------------
/*
  NS_LOG_INFO("Consumer configuration");
  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
    consumerHelper.SetPrefix(prefix);
    consumerHelper.SetAttribute("Frequency", StringValue("10000"));
//    consumerHelper.SetAttribute("MaxSeq", IntegerValue(100));
    consumerHelper.Install(consumer);

    Ptr<ndn::ConsumerCbr> app_cbr = DynamicCast<ndn::ConsumerCbr>(consumer->GetApplication(0));
    app_cbr->SetTraceFileName(pathOut, "ConsumerCbr.plotme");
*/

  InstallRaaqm( consumer,
		  	    sigma,
		  	    path_report,
		  	    abort_mode,
		  	    new_retr_timer,
		  	    retr_limit,
		  	    lambda,
		  	    catalog_size,
		  	    power,
		  	    observation,
		  	    prefix,
		  	    pathOut,
		  	    rtt_red);

//START the producer configuration----------------------------------------------------------------------

  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  producerHelper.SetPrefix(prefix);
  producerHelper.SetAttribute("PayloadSize", StringValue(payload));
//  producerHelper.SetAttribute("UpdateInterval", StringValue("0.01"));
  producerHelper.Install(producer);

//  Ptr<ndn::Producer> app = DynamicCast<ndn::Producer>(producer->GetApplication(0));
//  app->SetBSNodes(base_station, base_station_1);

  Ptr<ndn::L3Protocol> ndnBS1W = base_station->GetObject<ndn::L3Protocol>();
  shared_ptr<ndn::Face> faceBS1W = ndnBS1W->getFaceByNetDevice(base_station->GetDevice(0));
  faceBS1W->SetFaceAsWireless();
/*
  nfd::Forwarder& forwarder = *ndnBS1W->getForwarder();
  forwarder.SetNRetransmissions(3);
  std::cout<<&forwarder<<" RTX "<<forwarder.GetNRetransmissions()<<std::endl;
*/
//END of producer configuration-------------------------------------------------------------------------

//Installing routes-------------------------------------------------------------------------------------

//  ComputeRoutes(prefix, producer, true);
//  RecomputeRoutes(prefix, producer);
//  ndn::FibHelper::AddRoute	(base_station, prefix, producer, 1);

// Configure the outputs
//if(trace_queues){

  Ptr<PointToPointNetDevice> p2pndC1 = DynamicCast<PointToPointNetDevice> (consumer->GetDevice(0));
  Ptr<Queue> queueC1R1 = p2pndC1->GetQueue();
  std::cout<<"Consumer p2p queue "<<queueC1R1<<std::endl;

	  std::cout<<"PathOut "<<pathOut<<std::endl;
  filePlotQueueR1 << pathOut << "/" << "queues_r1";
  remove (filePlotQueueR1.str ().c_str ());
  Ptr<PointToPointNetDevice> p2pnd = DynamicCast<PointToPointNetDevice> (r1->GetDevice(0));
  Ptr<Queue> queue = p2pnd->GetQueue();
  Ptr<PointToPointNetDevice> p2pndr1r2 = DynamicCast<PointToPointNetDevice> (r1->GetDevice(1));
  Ptr<Queue> queue1 = p2pndr1r2->GetQueue();
//  queue1->SetAttribute("MaxPackets", StringValue("10000"));
  Ptr<PointToPointNetDevice> p2pndr1r3 = DynamicCast<PointToPointNetDevice> (r1->GetDevice(2));
  Ptr<Queue> queue2 = p2pndr1r3->GetQueue();
  Simulator::ScheduleNow (&CheckQueueSizeR1, queue, queue1, queue2);

  std::cout<<"R1 queues: "<<queue<<" "<<queue1<<" "<<queue2<<std::endl;

  filePlotQueueR2 << pathOut << "/" << "queues_r2";
  fileDataPktDropR2 << pathOut << "/" << "drop_r2";
  remove (filePlotQueueR2.str ().c_str ());
  remove (fileDataPktDropR2.str ().c_str ());
  Ptr<PointToPointNetDevice> p2pndR2 = DynamicCast<PointToPointNetDevice> (r2->GetDevice(0));
  Ptr<Queue> queueR2R1 = p2pndR2->GetQueue();
  queueR2R1->TraceConnectWithoutContext("Drop", MakeCallback(&PktDrop));
//		  Config::ConnectWithoutContext( "/NodeList/[i]/DeviceList/[i]/$ns3::PointToPointNetDevice/TxQueue/Drop", MakeCallback(&));
  Ptr<PointToPointNetDevice> p2pndR2B1 = DynamicCast<PointToPointNetDevice> (r2->GetDevice(1));
  Ptr<Queue> queueR2B1 = p2pndR2B1->GetQueue();
  Simulator::ScheduleNow (&CheckQueueSizeR2, queueR2R1, queueR2B1);
  std::cout<<"R2 queues: "<<queueR2R1<<" "<<queueR2B1<<std::endl;


  filePlotQueueR3 << pathOut << "/" << "queues_r3";
  remove (filePlotQueueR3.str ().c_str ());
  Ptr<PointToPointNetDevice> p2pndR3 = DynamicCast<PointToPointNetDevice> (r3->GetDevice(0));
  Ptr<Queue> queueR3R1 = p2pndR3->GetQueue();
  Ptr<PointToPointNetDevice> p2pndR3B2 = DynamicCast<PointToPointNetDevice> (r3->GetDevice(1));
  Ptr<Queue> queueR3B2 = p2pndR3B2->GetQueue();
  Simulator::ScheduleNow (&CheckQueueSizeR3, queueR3R1, queueR3B2);
  std::cout<<"R3 queues: "<<queueR3R1<<" "<<queueR3B2<<std::endl;

  filePlotQueueB1 << pathOut << "/" << "queues_B1";
  remove (filePlotQueueB1.str ().c_str ());
  Ptr<PointToPointNetDevice> p2pndB1 = DynamicCast<PointToPointNetDevice> (base_station->GetDevice(1));
  Ptr<Queue> queueB1R2 = p2pndB1->GetQueue();
  Ptr<WifiNetDevice> p2pndB1P = DynamicCast<WifiNetDevice> (base_station->GetDevice(0));
  Ptr<ns3::AdhocWifiMac> wifimacB1= StaticCast<ns3::AdhocWifiMac>(p2pndB1P->GetMac());
  PointerValue ptr;
  wifimacB1->GetAttribute ("DcaTxop",ptr);
  Ptr<DcaTxop> dcaB1 = ptr.Get<DcaTxop> ();
  Ptr<WifiMacQueue> queueB1P = dcaB1->GetQueue();
  std::cout<<"QueueB1 "<<wirelessQLength<<std::endl;
  queueB1P->SetMaxSize(wirelessQLength);
  Simulator::ScheduleNow (&CheckQueueSizeB1, queueB1R2, queueB1P);
  std::cout<<"B1 p2p queue: "<<queueB1R2<<std::endl;
/*
  filePlotQueueB2 << pathOut << "/" << "queues_B2";
  remove (filePlotQueueB2.str ().c_str ());
  Ptr<PointToPointNetDevice> p2pndB2 = DynamicCast<PointToPointNetDevice> (base_station_1->GetDevice(1));
  Ptr<Queue> queueB2R3 = p2pndB2->GetQueue();
  Ptr<WifiNetDevice> p2pndB2P = DynamicCast<WifiNetDevice> (base_station_1->GetDevice(0));
  Ptr<ns3::AdhocWifiMac> wifimacB2= StaticCast<ns3::AdhocWifiMac>(p2pndB2P->GetMac());
  PointerValue ptr2;
  wifimacB2->GetAttribute ("DcaTxop",ptr2);
  Ptr<DcaTxop> dcaB2 = ptr2.Get<DcaTxop> ();
  Ptr<WifiMacQueue> queueB2P = dcaB2->GetQueue();
  queueB2P->SetMaxSize(100);
  Simulator::ScheduleNow (&CheckQueueSizeB2, queueB2R3, queueB2P);
*/
  filePlotQueueProducer << pathOut << "/" << "queues_p";
   remove (filePlotQueueProducer.str ().c_str ());
    Ptr<WifiNetDevice> p2pndP = DynamicCast<WifiNetDevice> (producer->GetDevice(0));
     Ptr<ns3::AdhocWifiMac> wifimacP= StaticCast<ns3::AdhocWifiMac>(p2pndP->GetMac());
     PointerValue ptrP;
     wifimacP->GetAttribute ("DcaTxop",ptrP);
     Ptr<DcaTxop> dcaP = ptrP.Get<DcaTxop> ();
     Ptr<WifiMacQueue> queueP = dcaP->GetQueue();
     queueP->SetMaxSize(1000000);
      Simulator::ScheduleNow (&CheckQueueSizeProducer, queueP);


//}
// End of configuration


  Simulator::Stop(Seconds(300.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}
} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
