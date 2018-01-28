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

#ifndef NFD_DAEMON_FW_LOAD_BALANCE_STRATEGY_HPP
#define NFD_DAEMON_FW_LOAD_BALANCE_STRATEGY_HPP

//using best-route-strategy2 as an helper is for control interest(/localhost) is no longer necessary in NDN 0.3.0
//Thanks to the introduction of strategy choice table configuration in the nfd.conf file.
//#include "best-route-strategy2.hpp" 

#include "strategy.hpp"
#include <boost/unordered_map.hpp> 
#include <boost/random/uniform_real_distribution.hpp>

namespace nfd {
namespace fw {

/** \brief a randomized load balance forwarding strategy
 * in current implementation, for each pit , only one interest is allowed to be forwarded at any time, in the sense
 * that when new interest arrives, it will check if there's an unexpired outRecord, if yes, it will be just pending 
 * not forwarded
 */

//using best-route-strategy2 as an helper is for control interest(/localhost) is no longer necessary in NDN 0.3.0
//Thanks to the introduction of strategy choice table configuration in the nfd.conf file.

class LoadBalanceStrategy : public Strategy
{
public:
  LoadBalanceStrategy(Forwarder& forwarder, const Name& name = STRATEGY_NAME);
  
  virtual
  ~LoadBalanceStrategy();
  
  virtual void
  afterReceiveInterest(const Face& inFace,
                       const Interest& interest,
                       shared_ptr<fib::Entry> fibEntry,
                       shared_ptr<pit::Entry> pitEntry);
  
  virtual void
  beforeSatisfyInterest(shared_ptr<pit::Entry> pitEntry,
                               const Face& inFace, const Data& data);
  
  //not overide the beforeExpirePendingInterest() function, not useful here
  
protected:
  ///@brief data stored in measurements::Entry
  class MeasurementsEntryInfo : public StrategyInfo
  {
  protected:
    class NextHopStateEntry;
   public:
    MeasurementsEntryInfo();
    ~MeasurementsEntryInfo();

static constexpr int
    getTypeId()
    {
      return 2000;
    }

    
    typedef boost::unordered_map<FaceId, NextHopStateEntry> StateMap;


    /**
     * @brief add a new nexthop for a prefix to the measurementEntryInfo
     * @param newFaceId the faceId of the face to be added
     */ 
    void addNextHopStateEntry(FaceId newFaceId);
    
    /**
     * @brief remove a nexthop for a prefix to the measurementEntryInfo
     * @param oldFaceId the faceId of the face to be removed
     */
    void removeNextHopStateEntry(FaceId oldFaceId);
    
    /**
     * @brief update number of pending interest and weight for a particular face
     * @param faceId the faceId for the nextHopStateEntry that should be updated
     * @param flag a flag to indicate the type of update, true means to increment the number of pending interests,
     * false means to decrement the number of pending interests
     */ 
    void updatePendingAndWeight(FaceId faceId,bool flag);
/*    
    **
     * overload:
     * @brief update number of pending interest and weight for a particular prefix and face
     * @param it an iterator point to the nextHopStateEntry that should be updated(
     * @param flag a flag to indicate the type of update, true means to increment the number of pending interests,
     * false means to decrement the number of pending interests
     * 
    void updatePendingAndWeight(boost::unordered_map<FaceId, NextHopStateEntry>::iterator it,bool flag);
*/    
    /**
     * @brief get the Id of the best face to which interest should be forwarded to by using random weight algorithm
     * @param inFace the incoming face of the interest to be forwarded
     * @return the proposed faceId that will be used to send the interest, if no face available, it will return -1
     */
    FaceId weightedRandom(FaceId inFaceId);
    
     /**
     * @brief get the Id of the best face to which interest should be forwarded to by using random algorithm
     * @param inFace the incoming face of the interest to be forwarded
     * @return the proposed faceId that will be used to send the interest, if no face available, it will return -1
     */
    FaceId random(FaceId inFaceId);
    
    /**
     * @brief update split states
     */
    void updateSplitStates(FaceId faceId);
    
     /**
      * @brief report the split ratio status for a prefix and renew the number of forwarded interests, then finally
      * schedule the next report
      */
      void scheduleAndReport(shared_ptr<const Name> prefix);
    
    /**
     * @brief return the number of the stateEntry(or number of nexthops) stored in measurementEntryInfo
     */
    size_t size();
        
    protected:
    ///1. subsub class for defining the MeasurementsEntryInfo class
      //TODO this data structure seems to be unused, better to remove it, currently this one is not actually integrated 
      //into the real processing
       class SplitStates ///what is split stats?
       {
         public:
          typedef time::steady_clock::TimePoint TimePoint;  
          ///@brief initialization of the splitSate
          void init(TimePoint start);     
          ///@brief 
          bool timeCounter(TimePoint now, bool sum);
      
         private:
          ///@brief ewma updating the rate,after the other states are updated.
          void ewma();

          unsigned int winCounter;
          unsigned int samples;
          double alpha;      
          double m_timeWindowMax;
          double rate;
          TimePoint startTime;
          TimePoint previousTime;
       };//end subsub class splitStates 
  
   ///subsub class stateEntry for each outgoing face of a fib entry
       struct NextHopStateEntry
       {
	 ///FaceId has been saved as the key of a NextHopStateEntry the unordered map
	 ///, so we eliminate it from the NextHopStateEntry
         //FaceId outFaceId;
         unsigned int pending;
         double averagePending;
         double weight;
	 unsigned int forwardedInterests;
         //SplitStates splitStates;
	 /**
	  * @brief shortcut for average pending update
	  * @param alpha the ratio for ewma computation
	  */
	 void ewma(double alpha);
	 ///@brief constructor
	 NextHopStateEntry(unsigned int n_pending=0,
			   double n_averagePending=0,
		           double n_weight=1,
		           unsigned int n_forwardedInterests=0);
       };//end subsub class NextHopStateEntry
  
  private:
    //unordered map for NextHopStateEntry
    //boost::unordered_map<FaceId, NextHopStateEntry>
    StateMap nextHopStates;
    double weightSum;
    unsigned int totalForwardedInterests;
    const time::microseconds reportInterval;
    // notification interval for the split ratio states report
    scheduler::EventId splitStatesReport;
    const Name m_prefix;
    ///@brief alpha should be prefix specific,currently it is static for simplicity(same for all measurement entry)
    static const double m_alpha;
    static const boost::random::uniform_real_distribution<double> dist;
  };//end sub class measurementEntryInfo 
  
  ///@brief subclass for storing timer in the pit to accout for the timeout of an interest
  class PitEntryInfo: public StrategyInfo
  {
  public:
static constexpr int
    getTypeId()
    {
      return 2001;
    }

    /// timer that expires when the outRecord expires
    scheduler::EventId outRecordTimeOut;
    ~PitEntryInfo();
  };
  
public:
  static const Name STRATEGY_NAME;

protected:
  static const int PREFIX_RELATIVE_LEVEL=1;
  static const time::milliseconds MEASUREMENTS_LIFETIME;///now default to 16 seconds
  //static const std::string prefixOfControlInterest;///used to determine whether to use the default strategy, in NDN 0.3.0 not necessary.
  /**
   * @brief get(or insert and then get) the measurementEntry at the right position that corresponds the given pitEntry 
   */
  shared_ptr<measurements::Entry> getMeasurementsEntry(shared_ptr<pit::Entry> pitEntry);
  
  //static const int reTry=0;
  
};//end LoadBalanceStrategy class

inline void
LoadBalanceStrategy::MeasurementsEntryInfo::SplitStates::ewma()
{
  double sample=(double)samples/m_timeWindowMax;
  rate = alpha * rate + (1-alpha) * sample;
}

inline void
LoadBalanceStrategy::MeasurementsEntryInfo::NextHopStateEntry::ewma(double alpha)
{
  averagePending=alpha*averagePending+(1-alpha)*pending;
}

// inline void
// LoadBalanceStrategy::MeasurementsEntryInfo::updateSplitStates(FaceId faceId)
// {
//   boost::unordered_map<FaceId, NextHopStateEntry>::iterator it=nextHopStates.find(faceId);
//   if(it!=nextHopStates.end())
//     (it->second).splitStates.timeCounter(time::steady_clock::now(),true);
// }

inline size_t 
LoadBalanceStrategy::MeasurementsEntryInfo::size()
{
  return nextHopStates.size();
}

} // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_LOAD_BALANCE_STRATEGY_HPP
