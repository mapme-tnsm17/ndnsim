/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014,  IRT SystemX,  Orange Labs Networks, Bell Labs
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

#include "load-balance-strategy.hpp"
#include "core/random.hpp"
#include "core/logger.hpp"
#include <boost/random/uniform_int_distribution.hpp>
#include <ndn-cxx/util/time.hpp>
#include <boost/foreach.hpp>
#include <boost/concept_check.hpp>
#include <stdlib.h>
#include <iomanip>



namespace nfd {
namespace fw {

NFD_LOG_INIT("LoadBalanceStrategy");
  
const Name LoadBalanceStrategy::STRATEGY_NAME("ndn:/localhost/nfd/strategy/load-balance/%FD%01");
NFD_REGISTER_STRATEGY(LoadBalanceStrategy);

const time::milliseconds LoadBalanceStrategy::MEASUREMENTS_LIFETIME = time::milliseconds(16000);
const boost::random::uniform_real_distribution<double> LoadBalanceStrategy::MeasurementsEntryInfo::dist;
const double LoadBalanceStrategy::MeasurementsEntryInfo::m_alpha=0.9;
//const std::string LoadBalanceStrategy::prefixOfControlInterest("localhost");

LoadBalanceStrategy::LoadBalanceStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder, name)
{
}

LoadBalanceStrategy::~LoadBalanceStrategy()
{
  
}

LoadBalanceStrategy::MeasurementsEntryInfo::MeasurementsEntryInfo()
:weightSum(0),
 totalForwardedInterests(0),
 //***********change the number to the split state ratio interval we want****************
 reportInterval(getenv("NFD_STATS_INTERVAL")==NULL?1000000:atol(getenv("NFD_STATS_INTERVAL")))
{
  if(getenv("NFD_STATS_INTERVAL")==NULL)
    NFD_LOG_DEBUG("cannot get NFD_STATS_INTERVAL");
}

LoadBalanceStrategy::MeasurementsEntryInfo::~MeasurementsEntryInfo()
{
  scheduler::cancel(this->splitStatesReport);
}




void
LoadBalanceStrategy::afterReceiveInterest(const Face& inFace,
                                  const Interest& interest,
                                  shared_ptr<fib::Entry> fibEntry,
                                  shared_ptr<pit::Entry> pitEntry)
{
//code below unnecessary in NND 0.3.0
/**
  //for local scope interest(control interest), using default strategy)
  if(interest.getName().at(0).toUri()==prefixOfControlInterest)
  {


    this->BestRouteStrategy2::afterReceiveInterest(inFace, interest, fibEntry, pitEntry);
    return;
  }
*/

  //if no face available in the FIB,reject pending,which means delete the pit
  NFD_LOG_TRACE("after receive interest=" << interest <<" from " << inFace.getId());
  const fib::NextHopList& nexthops = fibEntry->getNextHops();
  if (nexthops.size() == 0) 
  {
    NFD_LOG_TRACE(interest <<",reject interest because no entry in the FIB" );
    this->rejectPendingInterest(pitEntry); 
    return;
  }
  
  ///for logging: list the available faces in fib
   for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it)
         {
	  NFD_LOG_TRACE("available faces in fib: "<< it->getFace()->getId() );
         } 
         
       
  //get the measurement that is monitoring this PIT     
  shared_ptr<measurements::Entry> measurementsEntry=this->getMeasurementsEntry(pitEntry);
  NFD_LOG_TRACE("measurement entry at"<<measurementsEntry->getName());

    ///extend the measurementlifetime
  const time::milliseconds& lifetime=interest.getInterestLifetime();
  this->getMeasurements().extendLifetime(*measurementsEntry, std::max(MEASUREMENTS_LIFETIME,lifetime) );
  
  bool alreadyForwarded = pitEntry->hasUnexpiredOutRecords();
  if(!alreadyForwarded)
  {
    //get the real data structure that is stored in the measurement entry.
    shared_ptr<MeasurementsEntryInfo> info = measurementsEntry->getStrategyInfo<MeasurementsEntryInfo>();
    if (!static_cast<bool>(info))///info lost
       {
         info = make_shared<MeasurementsEntryInfo>();
         ///load the available faces that are listed in the fib
	 NFD_LOG_TRACE("MeasurementsEntryInfo not found, start to initialize it based on fib");
         for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it)
         {
          shared_ptr<Face> availableOutFace = it->getFace();
          info->addNextHopStateEntry(availableOutFace->getId());
         } 
         measurementsEntry->setStrategyInfo(info);
	 //schedule split ratio state report for this prefix:
	 //****************split states report is disabled, to make the simulation run faster*****************
	 info->scheduleAndReport(make_shared<const Name>(measurementsEntry->getName()));
       }
       else
       {
	 NFD_LOG_TRACE("MeasurementsEntryInfo is already there,syncronize it with fib");
	 //syncronize with the fibEntry
	 //NOTE: addNextHopStateEntry() will check internally and only new nexthops will be added to the nextHopStates
	for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it)
         {
          shared_ptr<Face> availableOutFace = it->getFace();
          info->addNextHopStateEntry(availableOutFace->getId());
         } 
       }
    
    

 
     BOOST_ASSERT(info->size()==nexthops.size());
     
     
     
     //compute the outfaceid
     FaceId inFaceId=inFace.getId();
     FaceId outFaceId=info->weightedRandom(inFaceId);
    // FaceId outFaceId=info->random(inFaceId);
     if(outFaceId<0)
     {
       NFD_LOG_TRACE("new interest="<<interest<<
       ",reject interest because no outface available according to weightedRandom");
       this->rejectPendingInterest(pitEntry);
     }
     else///send the interest
     {
       bool beSent=false;
       shared_ptr<Face> outFace=this->getFace(outFaceId);
       if (static_cast<bool>(outFace) && fibEntry->hasNextHop(outFace) && !pitEntry->violatesScope(*outFace) )//send interest with wieghted random algorithm
        {
	 NFD_LOG_TRACE("new interest="<<interest <<"with loadbalancing, send interest to face="<< outFaceId );
         this->sendInterest(pitEntry,outFace);
	 beSent=true;
        }
       else///the information in MeasurementsEntryInfo is not up to date or something bad happened, instead use the second approach
	 ///: use the first nexthop that is not equal to inFace, actually this is the same as default strategy of NDN
        {
	  for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it)
	  {
	     NFD_LOG_TRACE("using second approach(use the first nexthop) to forward new interest" );
            outFace = it->getFace();
	    outFaceId=outFace->getId();
           if(outFaceId!=inFaceId && !pitEntry->violatesScope(*outFace) )
	   {
	     NFD_LOG_TRACE("new interest="<<interest <<"with first nexthop, send to face="<< outFaceId );
	     this->sendInterest(pitEntry, outFace);
	     beSent=true;
	     break;
	   }
          }
	}
      if(beSent)
       {
      ///update the pending information and schedule a timer
	 
         shared_ptr<PitEntryInfo> pitEntryInfo =pitEntry->getOrCreateStrategyInfo<PitEntryInfo>();
           //update splitStates TODO remove the next line
       // info->updateSplitStates(outFaceId);
	 
           //update pending information
          info->updatePendingAndWeight(outFaceId,true);
           //set or update timer
           pitEntryInfo->outRecordTimeOut=scheduler::schedule(lifetime,
 		bind(&LoadBalanceStrategy::MeasurementsEntryInfo::updatePendingAndWeight,info.get(),outFaceId,false));

       }
      else
      {
	 this->rejectPendingInterest(pitEntry); 
      }
     } 
  }
  else
  {
    NFD_LOG_TRACE("old interest="<<interest<<
    "outRecord exists, just pending");
  }
}

void
LoadBalanceStrategy::beforeSatisfyInterest(shared_ptr<pit::Entry> pitEntry,
                                          const Face& inFace, const Data& data)
{
//code below unnecessary in NDN 0.3.0
/**
  if(pitEntry->getName().at(0).toUri()==prefixOfControlInterest)
  {
    ///use the default strategy behavior instead, which is to do nothing.
    return;
  }
 *
 */ 
  NFD_LOG_TRACE("before satisfy interest="<< pitEntry->getName() << " from " << inFace.getId());
  //get the measurement entry for the pit
  shared_ptr<measurements::Entry> measurementsEntry=this->getMeasurementsEntry(pitEntry);
  NFD_LOG_TRACE("measurement entry at"<<measurementsEntry->getName());
  shared_ptr<MeasurementsEntryInfo> info = measurementsEntry->getStrategyInfo<MeasurementsEntryInfo>();
  
  if (!static_cast<bool>(info))
  {
    NFD_LOG_TRACE("before satisfy interest=" << pitEntry->getName() << ",strategy measurement info lost");
  }
  else
  {
    NFD_LOG_TRACE("before satisfy interest=" << pitEntry->getName() << ",strategy measurement info found,updating now");
    FaceId inFaceId=inFace.getId();
    //pit::OutRecordCollection::const_iterator outRecordIterator=
    //pitEntry->getOutRecord(const_cast<Face&>(inFace).shared_from_this());
    //if the data arrives from a face unkown or it arrives too late we will not update the pending
  //  if(outRecordIterator!=pitEntry->getOutRecords().end() && 
  //    (outRecordIterator->getExpiry()>=time::steady_clock::now()) )
  //  {
      NFD_LOG_TRACE("outRecord exsit and valid, try to cancel the timer now");
      shared_ptr<PitEntryInfo> pitEntryInfo = pitEntry->getStrategyInfo<PitEntryInfo>();
      if (static_cast<bool>(pitEntryInfo)) 
      {
        scheduler::cancel(pitEntryInfo->outRecordTimeOut);
      }
      else
      {
	NFD_LOG_TRACE("Timer lost, so no need to cancel the timer");
      }
      info->updatePendingAndWeight(inFaceId,false);

 //   }
 //   else
 //   {
 //     NFD_LOG_TRACE("outRecord expired or unsolicited data arrived");
  //  }
  }
  
}

shared_ptr<measurements::Entry>
LoadBalanceStrategy::getMeasurementsEntry(shared_ptr<pit::Entry> pitEntry)
{
  shared_ptr<measurements::Entry> measurementsEntry = this->getMeasurements().get(*pitEntry);
  int i=1;
  while(i<=PREFIX_RELATIVE_LEVEL)
  {
      shared_ptr<measurements::Entry> parentMeasurementsEntry = this->getMeasurements().getParent(*measurementsEntry);
      if(!static_cast<bool>(parentMeasurementsEntry))
	break;
      measurementsEntry=parentMeasurementsEntry;
      i++;
  }
      return measurementsEntry;
      
}

  


void 
LoadBalanceStrategy::MeasurementsEntryInfo::SplitStates::init(LoadBalanceStrategy::MeasurementsEntryInfo::SplitStates::TimePoint start)
{
  startTime=start;
  winCounter=0;
  samples=0;
  m_timeWindowMax=0.1;//10.0;
  alpha=0.99;
  rate=0.0;
  //startTime=0.0;
  //previousTime=0.0;
  previousTime=start;
}

bool 
LoadBalanceStrategy::MeasurementsEntryInfo::SplitStates::timeCounter(TimePoint now, bool sum)
{
  //TODO the count of time difference is in nanoseconds, we need to make sure it is in the same unit as m_timeWindowMax
  unsigned int new_measure = (unsigned int)floor((now - startTime).count()/m_timeWindowMax);
  if ( winCounter == new_measure )
  {
     samples+=sum;
     return false;
  }
  ewma();
  samples = 0;
  winCounter++;
  timeCounter(now, sum);
  return true;
}

LoadBalanceStrategy::MeasurementsEntryInfo::NextHopStateEntry::NextHopStateEntry(unsigned int n_pending,
										 double n_averagePending,
										 double n_weight,
										 unsigned int n_forwardedInterests)
:pending(n_pending)
,averagePending(n_averagePending)
,weight(n_weight)
,forwardedInterests(n_forwardedInterests)
{}

void
LoadBalanceStrategy::MeasurementsEntryInfo::addNextHopStateEntry(FaceId newFaceId)
{
  if(nextHopStates.find(newFaceId)!=nextHopStates.end())
  {
    return;
  }
  else
  {
    NextHopStateEntry newStateEntry(0,//pending
				    0,//averagePending
				    1,//weight
				    0//forwardedInterests
		                    );
				
    //newStateEntry.splitStates.init(time::steady_clock::now());
    nextHopStates[newFaceId]=newStateEntry;
    weightSum+=1;
  }
}

void
LoadBalanceStrategy::MeasurementsEntryInfo::removeNextHopStateEntry(FaceId oldFaceId)
{
  boost::unordered_map<FaceId, NextHopStateEntry>::iterator it=nextHopStates.find(oldFaceId);
  
  if(it!=nextHopStates.end())
  {
   weightSum-=nextHopStates[oldFaceId].weight;
   nextHopStates.erase(it);
  }
}

void
LoadBalanceStrategy::MeasurementsEntryInfo::updatePendingAndWeight(FaceId faceId,bool flag)
{
  
  boost::unordered_map<FaceId, NextHopStateEntry>::iterator it=nextHopStates.find(faceId);
  if(it==nextHopStates.end())
  {
    NFD_LOG_TRACE("the face (" << faceId <<") to be update is not found in the measurementEntry");
    return;
  }
  else
  {
    
    NextHopStateEntry& entry=it->second;
    
    ///update the  No. of pending interests
     if(flag==true)
     {
       entry.pending++;
       //***********splite states update is disabled to speed up simulation****************
       entry.forwardedInterests++;
       totalForwardedInterests++;
     }
     else
     {
       if (entry.pending>0)
         entry.pending--;
     }
  
     ///update weight
     weightSum-=entry.weight;
     entry.ewma(m_alpha);  
     if (entry.averagePending==0.0)
       entry.averagePending=0.1;
     entry.weight=float(1)/entry.averagePending;
     weightSum+=entry.weight;
     NFD_LOG_TRACE("weight up to date,the pending becomes "<<entry.pending);
    
  }
  
}



FaceId
LoadBalanceStrategy::MeasurementsEntryInfo::weightedRandom(FaceId inFaceId)
{
  ///generate random number between 0 and 1
      double r=dist(getGlobalRng());
      NFD_LOG_TRACE("start weighted random algorithm,r=" << r);
      NFD_LOG_TRACE("availabe faces are:"<<nextHopStates.size() );
      double cum=0;
      
      boost::unordered_map<FaceId, NextHopStateEntry>::iterator it=nextHopStates.find(inFaceId);
      if(it!=nextHopStates.end())
      {
	double newWeightSum=weightSum-((it->second).weight);
	if(newWeightSum<0.0000001)
	  return -1;
	
	BOOST_FOREACH(StateMap::value_type kv, nextHopStates)
       {
	 NFD_LOG_TRACE("weighted random,newweightSum=" << newWeightSum);
	 if(kv.first!=inFaceId)
	 {
	   cum+=kv.second.weight/newWeightSum;
	   NFD_LOG_TRACE("cum="<<cum);
	   if(cum>r)
	   {
	    NFD_LOG_TRACE("weighted random,face="<<kv.first<<"is OK");
	    return kv.first;
	   }
	   NFD_LOG_TRACE("weighted random,inface="<< inFaceId <<",face="<<kv.first<<" is not OK");
	 }      
       }
      }
      else
      {
	BOOST_FOREACH(StateMap::value_type kv, nextHopStates)
       {
	
	   NFD_LOG_TRACE("weighted random,weightSum=" << weightSum);
	   cum+=kv.second.weight/weightSum;
	   NFD_LOG_TRACE("cum="<<cum);
	   if(cum>r)
	    return kv.first;
	   NFD_LOG_TRACE("weighted random,inface="<< inFaceId <<",face="<<kv.first<<" is not OK");
       }
      }
      
      return -1;///no face available
}

FaceId
LoadBalanceStrategy::MeasurementsEntryInfo::random(FaceId inFaceId)
{
  int available;
  bool shouldFilterOutInFace=nextHopStates.find(inFaceId)==nextHopStates.end();
  if(shouldFilterOutInFace)
    available=nextHopStates.size();
  else
    available=nextHopStates.size()-1;
  if(available==0)
    return -1;
  
  boost::random::uniform_int_distribution<FaceId> dist(0, available-1);
  int r=dist(getGlobalRng());
  
  int i=0;
  BOOST_FOREACH(StateMap::value_type kv, nextHopStates)
       {
	 if((!shouldFilterOutInFace)||(kv.first!=inFaceId))
	 {
	   if(i>=r)
	    return kv.first;
	   i++;
	 } 
       }
       return -1;//bad things happened,cannot find available faces
}

void
LoadBalanceStrategy::MeasurementsEntryInfo::scheduleAndReport(shared_ptr<const Name> prefix)
{
  if(nextHopStates.size()!=0)
  {
    boost::unordered_map<FaceId, NextHopStateEntry>::iterator it;
    for(it=nextHopStates.begin();it!=nextHopStates.end();++it)
    {
     NFD_LOG_INFO("nfd.splitStates."<<__LINE__
     <<" prefix: ndn:"<< *prefix <<" faceid: "<< it->first<<" fwd_int: "<<it->second.forwardedInterests
     <<" tot_prefix: "<<totalForwardedInterests<<" average_pi_num: "<<it->second.averagePending
     <<" pi_num: "<<it->second.pending);
     
//       std::cout<<"nfd.splitStates."<<__LINE__
//      <<" prefix: ndn:"<< *prefix <<" faceid: "<< it->first<<" fwd_int: "<<it->second.forwardedInterests
//      <<" tot_prefix: "<<totalForwardedInterests<<" average_pi_num: "<<it->second.averagePending
//      <<" pi_num: "<<it->second.pending<<"\n";

     it->second.forwardedInterests=0;
    }
    
    totalForwardedInterests=0;
    ///schedule the next reportInterval
    splitStatesReport=scheduler::schedule(reportInterval,
 		bind(&LoadBalanceStrategy::MeasurementsEntryInfo::scheduleAndReport,this,prefix));
    
    
  }
    
}
 

LoadBalanceStrategy::PitEntryInfo::~PitEntryInfo()
{
   scheduler::cancel(this->outRecordTimeOut);
}

} // namespace fw
} // namespace nfd
