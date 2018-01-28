/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 SEBASTIEN DERONNE
 *
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
 * Author: Sebastien Deronne <sebastien.deronne@gmail.com>
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ndnSIM/apps/ndn-consumer-cbr.hpp"
#include "ns3/ndnSIM/helper/ndn-global-routing-helper.hpp"
#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/ndnSIM/helper/ndn-app-helper.hpp"
#include "ns3/ndnSIM-module.h"


//#include "ns3/ndnSIM/model/ndn-wifi-net-device-face.hpp"
//#include "ns3/ndnSIM/model/ndn-net-device-face.hpp"
//#include "ns3/ndnSIM/model/ndn-l3-protocol.hpp"



#include "ns3/pointer.h"

#include "boost/lexical_cast.hpp"
// This is a simple example in order to show how to configure an IEEE 802.11ac Wi-Fi network.
//
// It ouputs the UDP or TCP goodput for every VHT bitrate value, which depends on the MCS value (0 to 9, where 9 is
// forbidden when the channel width is 20 MHz), the channel width (20, 40, 80 or 160 MHz) and the guard interval (long
// or short). The PHY bitrate is constant over all the simulation run. The user can also specify the distance between
// the access point and the station: the larger the distance the smaller the goodput.
//
// The simulation assumes a single station in an infrastructure network:
//
//  STA     AP
//    *     *
//    |     |
//   n1     n2
//
//Packets in this simulation aren't marked with a QosTag so they are considered
//belonging to BestEffort Access Class (AC_BE).




NS_LOG_COMPONENT_DEFINE ("ndn-vht-wifi-network-ConsumerCbr");

using namespace ns3;
uint64_t nReceivedData=0;

void
onReceiveData(ns3::ndn::shared_ptr<const ns3::ndn::Data> data, Ptr<ns3::ndn::App> app, ns3::ndn::shared_ptr<ns3::ndn::Face> face)
{
 nReceivedData++;
 //std::cout<<"dataSize="<<data->wireEncode().size();
 //std::cout<<"Time="<<Simulator::Now()<<", data="<<data->getName()<<", "<< (data->getContent()).value_size()<<", "<<nReceivedBytes<<"\n";
}


int main (int argc, char *argv[])
{
  

  double simulationTime = 5; //seconds
  double distance = 1.0; //meters
  double frequency = 5.0;
  int i=7;

  CommandLine cmd;
  cmd.AddValue ("frequency", "Whether working in the 2.4 or 5.0 GHz band (other values gets rejected)", frequency);
  cmd.AddValue ("distance", "Distance in meters between the station and the access point", distance);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("MCS", "MCS for the physical rate", i);
  cmd.Parse (argc,argv);

              uint32_t payloadSize; //
             
             /* 1500 bytes data packets(approximately, it could be 1499 or 1500 
	      * depends on the length of sequencenumber in data name)
	      */
              payloadSize = 1461; 
           

              NodeContainer wifiStaNode;
              wifiStaNode.Create (1);
              NodeContainer wifiApNode;
              wifiApNode.Create (1);
	      

	      //for 802.11n wifi
              YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
              YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
	        phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

              phy.SetChannel (channel.Create ());
	      

              // Set guard interval
              phy.Set ("ShortGuardEnabled", BooleanValue (1));
	      
	       //phy.Set ("GreenfieldEnabled",BooleanValue(true));

              WifiHelper wifi = WifiHelper::Default ();
	      
	      if (frequency == 5.0)
                {
                  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);
                }
              else if (frequency == 2.4)
                {
                  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);
                  Config::SetDefault ("ns3::LogDistancePropagationLossModel::ReferenceLoss", DoubleValue (40.046));
                }
              else
                {
                  std::cout<<"Wrong frequency value!"<<std::endl;
                  return 0;
                }
                
             HtWifiMacHelper mac = HtWifiMacHelper::Default ();
	     


	     /* no rate adaptation:
              StringValue DataRate = HtWifiMacHelper::DataRateForMcs (i);
              wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", DataRate
					   );
	      */

	     /* 802.11n minstrel rate adaptation */
	     wifi.SetRemoteStationManager ("ns3::MinstrelHtWifiManager"
                           //                  ,"LookAroundRate",DoubleValue(10),"SampleColumn",DoubleValue(30)
                                               );
                
	     Ssid ssid = Ssid ("ns3-80211ac");
	      

	      //for 802.11n 2-LEVEL data frame aggregation: 
	     int nMpdu=7;
	     int nMsdu=4;	     
	     
	     /*for station: */     
	     // -------------frame aggregation(aggregate data packets at producer)----------
	     mac.SetMpduAggregatorForAc (AC_BE, "ns3::MpduStandardAggregator"
	     ,"MaxAmpduSize", UintegerValue (7 * (4 * (payloadSize + 100)))
	      );
              mac.SetMsduAggregatorForAc (AC_BE, "ns3::MsduStandardAggregator"
	      ,"MaxAmsduSize", UintegerValue (4 * (payloadSize + 100))
	      );
	      /* NOTE: the block ack mechanism in current ns3 release is not robust,
	       * if runtime error occurs in the simulation, try to remove the following 
	       * line of code, 
	       * without block ack we can still achieve a maximum
	       * throughput of 70Mbits/s in 802.11n with NDN 
	       */
	      mac.SetBlockAckThresholdForAc (AC_BE, 2);
	     //------------------------------------------------------------------------------           
              mac.SetType ("ns3::StaWifiMac",
                           "Ssid", SsidValue (ssid),
                           "ActiveProbing", BooleanValue (false));
              NetDeviceContainer staDevice;
              staDevice = wifi.Install (phy, mac, wifiStaNode);
	      
	      
	      
	      /*for AP */
	      // -------------frame aggregation(aggregate interest packets at consumer)-------
	      mac.SetMpduAggregatorForAc (AC_BE, "ns3::MpduStandardAggregator"
	     ,"MaxAmpduSize", UintegerValue (nMpdu * (nMsdu * (64 + 10)))
	      );
              mac.SetMsduAggregatorForAc (AC_BE, "ns3::MsduStandardAggregator"
	      ,"MaxAmsduSize", UintegerValue (nMsdu * (64 + 10))
	      );
	      //------------------------------------------------------------------------------
              mac.SetType ("ns3::ApWifiMac",
                           "Ssid", SsidValue (ssid));
              NetDeviceContainer apDevice;
              apDevice = wifi.Install (phy, mac, wifiApNode);

              // Set channel width
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (40));

              // mobility.
              MobilityHelper mobility;
              Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

              positionAlloc->Add (Vector (0.0, 0.0, 0.0));
              positionAlloc->Add (Vector (distance, 0.0, 0.0));
              mobility.SetPositionAllocator (positionAlloc);

              mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

              mobility.Install (wifiApNode);
              mobility.Install (wifiStaNode);
	      
	       /* NDN stack */
	       ns3::ndn::StackHelper ndnHelper;
	       ndnHelper.SetOldContentStore("ns3::ndn::cs::Lru", "MaxSize", "1");
       
	       /* 
		* overwrite the wifi netdevice face implementation. 
		* should be called before ndn stack is installed
		*//*
	       ndnHelper.AddNetDeviceFaceCreateCallback(WifiNetDevice::GetTypeId()
	                                                ,MakeCallback(&ns3::ndn::wifiFace::wifiNetDeviceCallBack));
	       */
	       //install ndn stack	       
	       ndnHelper.InstallAll(); 
	       ns3::ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/best-route");
	       
	        /* Routing */
		ns3::ndn::GlobalRoutingHelper myGlobalRoutingHelper;
		// This compute routes
		myGlobalRoutingHelper.InstallAll();
		
		
	//**************install applications******************

	std::string prefix="/prefix";
	
	//install consumer
	ns3::ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
	consumerHelper.SetAttribute("Frequency",StringValue("10000.0"));
	consumerHelper.SetPrefix(prefix);
	ApplicationContainer myApps = consumerHelper.Install (wifiApNode);
	myApps.Start (Seconds (1.0));
	myApps.Stop (Seconds (simulationTime + 1));

	//install producer
        //set origin of prefix to producer NodeContainer
	myGlobalRoutingHelper.AddOrigins(prefix, wifiStaNode);
	ns3::ndn::AppHelper producerHelper("ns3::ndn::Producer");
	producerHelper.SetPrefix(prefix);
	producerHelper.SetAttribute("PayloadSize", StringValue(boost::lexical_cast<std::string>(payloadSize)));
	ApplicationContainer producer=producerHelper.Install(wifiStaNode);
	producer.Start (Seconds (0.0));
	producer.Stop (Seconds (simulationTime + 1));
	Config::ConnectWithoutContext("/NodeList/" + boost::lexical_cast<std::string>(wifiApNode.Get(0)->GetId())
                                  + "/ApplicationList/*/$ns3::ndn::App/ReceivedDatas",
                                MakeCallback(&onReceiveData));   
	
	//routing
	ns3::ndn::GlobalRoutingHelper::CalculateRoutes();

	Simulator::Stop (Seconds (simulationTime+1));

	//layer 3 trace:
	//ns3::ndn::L3RateTracer::InstallAll("rate-trace.txt", Seconds(simulationTime+0.999));

	//layer 2 congestion drop:
	//  L2RateTracer::InstallAll("drop-trace2.txt", Seconds(0.1));

	Simulator::Run ();

	Simulator::Destroy ();

	double throughput = 0;

	throughput = nReceivedData * payloadSize * 8 / (simulationTime * 1000000.0); //Mbit/s

	std::cout << distance<< "\t" << throughput << "\t"<<i << std::endl;
		 
  return 0;
}


