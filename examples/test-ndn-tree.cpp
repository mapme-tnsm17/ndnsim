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

// ndn-tree-tracers.cpp

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"

// #include "ns3/ndnSIM/apps/ndn-consumer-elastic-traffic.hpp"

 
 #include <iomanip>
 
 #include <time.h>

namespace ns3 {

/**
 * This scenario simulates a tree topology (using topology reader module)
 *
 *    /------\      /------\      /------\      /------\
 *    |leaf-1|      |leaf-2|      |leaf-3|      |leaf-4|
 *    \------/      \------/      \------/      \------/
 *         ^          ^                ^           ^
 *         |          |                |           |
 *          \        /                  \         /
 *           \      /                    \       /    10Mbps / 1ms
 *            \    /                      \     /
 *             |  |                        |   |
 *             v  v                        v   v
 *          /-------\                    /-------\
 *          | rtr-1 |                    | rtr-2 |
 *          \-------/                    \-------/
 *                ^                        ^
 *                |                        |
 *                 \                      /  10 Mpbs / 1ms
 *                  +--------+  +--------+
 *                           |  |
 *                           v  v
 *                        /--------\
 *                        |  root  |
 *                        \--------/
 *
 *
 * To run scenario and see what is happening, use the following command:
 *
 *     ./waf --run=ndn-tree-tracers
 */
const std::string PREFIX="/prefix";
int
main(int argc, char* argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  AnnotatedTopologyReader topologyReader("", 1);
  topologyReader.SetFileName("src/ndnSIM/examples/topologies/test-topo-tree.txt");
  topologyReader.Read();

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetOldContentStore("ns3::ndn::cs::Nocache");
  ndnHelper.InstallAll();

  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll(PREFIX, "/localhost/nfd/strategy/load-balance");

//   // Installing global routing interface on all nodes
//   ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
//   ndnGlobalRoutingHelper.InstallAll();

  // Getting containers for the consumer/producer
  Ptr<Node> producer[4] = {Names::Find<Node>("leaf-1"), Names::Find<Node>("leaf-2"),
                            Names::Find<Node>("leaf-3"), Names::Find<Node>("leaf-4")};
  Ptr<Node> consumer = Names::Find<Node>("root");

    std::cout<<"\n***********************\n";
  std::cout <<std::setw(6)<<"node" <<std::setw(6)<< " id\n";
 for(int i=0; i<4;  i++)
 {
   std::cout <<std::setw(6)<<"leaf-"+std::to_string(i+1) <<std::setw(6)<<producer[i]->GetId()<< "\n";
 }
 std::cout<<std::setw(6)<<"rtr-1" <<std::setw(6)<< Names::Find<Node>("rtr-1")->GetId()<< "\n";
 std::cout<<std::setw(6)<<"rtr-2"<<std::setw(6)<< Names::Find<Node>("rtr-2")->GetId()<< "\n";
  std::cout<<std::setw(6)<<"root"<<std::setw(6)<< Names::Find<Node>("root")->GetId()<< "\n";


NodeContainer producers;
   
   
  for (int i = 0; i < 4; i++) {
    //ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
    //consumerHelper.SetAttribute("Frequency", StringValue("100")); // 100 interests a second
     producers.Add(producer[i]);
  
    // Each consumer will express unique interests /root/<leaf-name>/<seq-no>
    //consumerHelper.SetPrefix("/root/" + Names::FindName(consumers[i]));
    //consumerHelper.Install(consumers[i]);
  }
  ndn::AppHelper producerHelper("ns3::ndn::Producer");
   //ndnGlobalRoutingHelper.AddOrigins(PREFIX, producers);
  
producerHelper.SetPrefix(PREFIX);
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));//maybe 1024???
producerHelper.Install(producers);
 

  // Register /root prefix with global routing controller and
  // install producer that will satisfy Interests in /root namespace
 
 // Calculate and install FIBs
  //ndn::GlobalRoutingHelper::CalculateRoutes();
  
  
  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerWindowRAAQM");
   consumerHelper.SetAttribute("MaxSeq", IntegerValue(100)); 
   consumerHelper.SetAttribute("EnablePathReport", BooleanValue(true));
   consumerHelper.SetPrefix(PREFIX);
   consumerHelper.Install(consumer);
   //consumerHelper.SetAttribute("NewRetxTimer", StringValue("50ms"));
 
//    //second, configure the traffic modelling application:
//    ndn::AppHelper elasticTrafficHelper("ns3::ndn::ConsumerElasticTraffic");
//    elasticTrafficHelper.SetPrefix(PREFIX);
//    elasticTrafficHelper.SetAttribute("WindowTraceFile",StringValue("window-trace.txt"));
//    //elasticTrafficHelper.SetAttribute("s",StringValue("0.1"));
//    elasticTrafficHelper.SetAttribute("ArrivalRate",StringValue("20"));
//  //third, install the traffic modelling application on the node:
//     ApplicationContainer myApps=elasticTrafficHelper.Install(consumer);
   
   
   

   //finally, add the configuration of the donwloader to the traffic modelling application, by default it is the raaqm downloader with default parameter
   //ndn::ConsumerElasticTraffic *myApp=dynamic_cast<ndn::ConsumerElasticTraffic*>(myApps.Get(0).operator->());
   //myApp->SetDownloadApplication(consumerHelper);
   
   
   // Manually configure FIB routes
  ndn::FibHelper::AddRoute("root", PREFIX, "rtr-1", 1);
    ndn::FibHelper::AddRoute("root", PREFIX, "rtr-2", 1);
      ndn::FibHelper::AddRoute("rtr-1", PREFIX, "leaf-1", 1);
            ndn::FibHelper::AddRoute("rtr-1", PREFIX, "leaf-2", 1);
	          ndn::FibHelper::AddRoute("rtr-2", PREFIX, "leaf-3", 1);
      ndn::FibHelper::AddRoute("rtr-2", PREFIX, "leaf-4", 1);





  // Calculate and install FIBs
  //ndn::GlobalRoutingHelper::CalculateRoutes();

  Simulator::Stop(Seconds(30.0));

  //ndn::L3RateTracer::InstallAll("rate-trace.txt", Seconds(10));
   clock_t begin, end;
   double time_spent;
begin = clock(); 


  Simulator::Run();
  Simulator::Destroy();
  
end = clock();
time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
std::cout<<"simulation time is "<<time_spent<<"s\n";
  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
