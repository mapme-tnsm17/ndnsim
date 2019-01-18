/**
 *
 * TODO
 *
 * - Integrate remaining time in refreshTraceToAnchor (as specified in the
 *   paper)... Why do they do this ? Note that we use NFD schedule, and not NS
 *   one, so things might differ to get the remaining time.
 *
 *   UPDATE: remaining time is implemented in an equivalent way in onFaceAdded,
 *   since we don't update the timer for mobility events
 *
 *
 * NOTES
 *
 * - ??? How do we agree on a trace name ? shall it be specified per-prefix.
 *   the client might just need to know the prefix under which the producer is
 *   publishing.
 * - XXX A traced interest (setting a trace) should not tap into the cache, but should be hooked
 * beforehand.
 * - Note that we cannot use forwarding strategies in that implementation
 * - tracing interest = leaves a trace in the PIT, inRecords hold where to go
 *   (?) don't we erase such entries in the PIT when an interest comes in ?
 *   (?) how does the producer know which interests to send ???
 *   (!) note that we have a trace name which is a circuit identifying a
 *   producer/a content. Both consumer and producer need to agree on a content
 *   (!) KITE == tunnel in PIT ! what is the different between KITE and
 *   ForwardingHint ???
 * - tracing interest: interest that should follow a trace. Note that non
 *   conformant traced interests are dropped, let's be sure that KITE does not
 *   say "might" instead of "should".
 * - We have a tracedPitEntry left which might be invalid and is never cleaned
 *   up
 * - XXX we forward traced interests using the forwarding strategy... broken for
 *   LB ! 
 *
 * - Multiple pending traces
 *   One solution is to use the flag KITE_FLAG_FOLLOW_TRACE_AND_FIB so that at
 *   least one packet reaches the Anchor, and has the latest road to the
 *   producer...
 *
 *
 * IMPLEMENTATION SPECIFICS
 * 
 * - in our test with kite to support real-time traffic, we found several problems
 *   and fixed them in order to make fair comparison with mapme, In particualr, 
 *   they include: 1) traced interests are also sent immediately after each handover 
 *   other than periodically sent. 2) tracing interests follow both FIB and traces
 *   to avoid being guided only to out-dated producer locations 3) acknowledgement
 *   retransmission of traced interests are enabled, to avoid large handoff latency
 *   in case of traced interest packet loss.
 * 
 * - The trace name in KITE paper should be specified in the application. In our
 *   code, we instead hook the interest transmission from an application
 *   (equivalent), and construct the trace name in a deterministic way (instead
 *   of accepting random trace name, again equivalent).
 * - We assume we have a default route in the FIB that we only use to get the
 *   traceName, since we prefix all consumer interests with KITE_BASE_TRACENAME.
 *   This means we need at least one GlobalRouting update after the producer
 *   connects, just like for MapMe, which is done by the ConnectivityManager.
 *
 *
 * CHANGELOG
 *
 * v1.0 - Initial release
 *
 *
 * DEPENDENCIES
 *
 * - ndn-cxx patch: KITE
 * - NFD/ndnSIM extension : CONF_FILE
 *
 *
 * TESTS
 *
 * - NFD
 *
 * - ndnSIM
 *     2.23
 * 
 * 
 * LAST MERGED COMMIT (TO DEL AFTER UNIFICATION OF CODEBASES)

 * commit 368811137b0f9e775213777863cc1842b67d879a
 * Author: zeng xuan <xuan.zeng@irt-systemx.fr>
 * Date:   Wed May 4 12:33:52 2016 +0200
 * 
 *  fix to kite
 *
 */

#ifdef KITE

#include "core/logger.hpp"
#include "face/null-face.hpp"
#include "forwarder.hpp"
#include "strategy.hpp"

#include "table/pit-tnt-entry.hpp"
#include "forwarder-kite.hpp"

#include <vector>
#include <boost/algorithm/string/predicate.hpp>

#define MS2NS(x) time::nanoseconds(x * 1000000)
#define IS_TRACED(interest) ((interest.getTraceForwardingFlag() != 255) && (interest.getTraceForwardingFlag() & 1))
#define SEQ_NA 0
#define TTL_NA 255
#define TTL_INIT 0
#define RETX_NA 255
#define RETX_INIT 0

namespace nfd {

NFD_LOG_INIT("ForwarderKite");

using fw::Strategy;

/*------------------------------------------------------------------------------
 * ForwarderKite : forwarding pipeline - Constructor & accessors
 *----------------------------------------------------------------------------*/

ForwarderKite::ForwarderKite()
  : Forwarder()
#ifdef WITH_RELIABLE_RETRANSMISSION
  , m_ack_seq(0)
#endif
  , m_isPull(false)
  
#ifdef NDNSIM
  , m_onSpecialInterest(ns3::MakeNullCallback<void, std::string, uint64_t, uint64_t, uint64_t>())
  , m_onProcessing(ns3::MakeNullCallback<void, std::string, uint64_t, uint64_t, uint64_t>())
#endif // NDNSIM

{
  getFaceTable().onAdd.connect(bind(&ForwarderKite::onFaceAdded, this, _1));
  getFaceTable().onRemove.connect(bind(&ForwarderKite::onFaceRemoved, this, _1));

  m_fib.onAdd.connect(bind(&ForwarderKite::onFibEntryChanged, this, _1));
  m_fib.onUpdate.connect(bind(&ForwarderKite::onFibEntryChanged, this, _1));
  m_fib.onRemove.connect(bind(&ForwarderKite::onFibEntryChanged, this, _1));
}



/*------------------------------------------------------------------------------
 * Event handling
 *----------------------------------------------------------------------------*/

void
ForwarderKite::updateTraceToAnchor(weak_ptr<Face> wpFace)
{
  /* ON MOBILITY EVENT
   * update trace, without changing timers.
   *
   * in this case, prefix is empty and we need to reannounce all prefixes.
   *
   * NOTE: This is equivalent to implementing remaining_time !
   * XXX but lifetime of updated packet has to be modified.
   */
  for (auto & it : m_pendingUpdates) {
    shared_ptr<Interest> interest = make_shared<Interest>();
    interest->setName(Name(it.first));
    interest->setInterestLifetime(time::milliseconds(KITE_DEFAULT_RETX));
    interest->setTraceForwardingFlag(KITE_FLAG_TRACEABLE);
    
    // NOTE:this is trick to avoid traced interest to be satisfied by any data packets,
    //equivalent to put traced interests into a separate TFT rather than simply in PIT.
    interest->setMinSuffixComponents(100);

    shared_ptr<Face> face = wpFace.lock();
    NFD_LOG_INFO("Update trace to anchor for it.first" << it.first << " on face=" << face->getDescription());
    
    if(!static_cast<bool>(face)) //check the validity of the face before sending
      return;
#ifdef WITH_RELIABLE_RETRANSMISSION
    this->sendReliableTracedInterest(wpFace, it.first, interest);
#else
    face->sendInterest(*interest);
#endif
    auto it_seq = m_seq.find(it.first);
    uint64_t seq = (it_seq != m_seq.end()) ? it_seq->second : -1;
    m_seq[it.first] = ++seq;
    m_retx[it.first] = RETX_INIT;
    m_onSpecialInterest(it.first, seq, TTL_INIT, RETX_INIT);
  }
}

#ifdef WITH_RELIABLE_RETRANSMISSION
void 
ForwarderKite::sendReliableTracedInterest(weak_ptr<Face> wpFace, std::string prefix, shared_ptr<Interest> interest)
{
  scheduler::cancel(m_pendingRetx[prefix]);
  shared_ptr<Face> face = wpFace.lock();
  if(!static_cast<bool>(face)) //check the validity of the face before sending
    return;
//    std::cout<<time::steady_clock::now()<<", send reliable traced interest, for prefix="<<prefix<<", at="<<(void*)this<<"\n";
  interest->refreshNonce();
  face->sendInterest(*interest);

  // logging traced interest transmission at producer
  // LOG: creation of traced interest
  // Let's set seq number here

  m_pendingRetx[prefix] = scheduler::schedule(MS2NS(KITE_RELIABLE_RETX),
      bind(&ForwarderKite::sendReliableTracedInterest, this,wpFace, prefix, interest));
}
#endif

void
ForwarderKite::refreshTraceToAnchor(std::string prefix)
{  
  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setName( Name(prefix));
  interest->setInterestLifetime(time::milliseconds(KITE_DEFAULT_RETX));
  interest->setTraceForwardingFlag(KITE_FLAG_TRACEABLE);
  
  interest->setMinSuffixComponents(100);
  
  

  // ON TICKS : Loop over all non local faces...
  // NOTE: initially, it is possible that there is no face if the producer is
  // not connected...
  for (auto face : getFaceTable()) {
    if (face->isLocal())
      continue;
    NFD_LOG_INFO("Setup trace to anchor for prefix " << prefix << " on face=" << face->getDescription());

#ifdef WITH_RELIABLE_RETRANSMISSION
    this->sendReliableTracedInterest(face,prefix,interest);
#else
    face->sendInterest(*interest);
#endif
  auto it_seq = m_seq.find(prefix);
  uint64_t seq = (it_seq != m_seq.end()) ? it_seq->second : 0;
// Here we don't increase seq number since we did not move... it is just a
// refresh = overhead for maintaining the current position.
//  m_seq[prefix] = ++seq;
  auto it_retx = m_retx.find(prefix);
  uint64_t retx = (it_retx != m_retx.end()) ? it_retx->second : 0;
  m_retx[prefix] = ++retx;
  m_onSpecialInterest(prefix, seq, TTL_INIT, retx);
  }

  // ..., clear any pending update  ...
  auto it = m_pendingUpdates.find(prefix);
  if (it != m_pendingUpdates.end()) {
    scheduler::cancel(it->second);
    // XXX m_pendingUpdates.erase(it);
  }

  // ... and schedule next one
  m_pendingUpdates[prefix] = scheduler::schedule(MS2NS(KITE_DEFAULT_RETX / KITE_DEFAULT_RETX_FACTOR),
      bind(&ForwarderKite::refreshTraceToAnchor, this, prefix));
  
}

void
ForwarderKite::onFaceAdded(shared_ptr<Face> face)
{
  /* Trigger re-sending the traced interests immediately after a mobility event,
   * so that the trace gets past the mobile node to the base station, then up to
   * the anchor.
   * 
   * NOTE:
   * - Previous traces are still present in the network, hence the need to use
   *   the KITE_FLAG_FOLLOW_TRACE_AND_FIB which duplicates the interest on both
   *   the trace and towards the anchor...
   */
  
  // Ignore local faces
  if (face->isLocal())
    return;

  NFD_LOG_INFO("*** onFaceAdded, mobility event, resending all prefixes - face=" << face->getDescription());

  scheduler::schedule(MS2NS(0), bind(&ForwarderKite::updateTraceToAnchor, this,
       face));
}

void
ForwarderKite::onFaceRemoved(shared_ptr<Face> face)
{
//  std::cout << "TODO" << std::endl;
}

// maybe we could hook the FIB instead XXX XXX much better XXX XXX 
void
ForwarderKite::onFibEntryChanged(shared_ptr<fib::Entry> fibEntry)
{
  for (auto & it : fibEntry->getNextHops()) {
    if (it.getFace()->getLocalUri() == FaceUri("appFace://")) {
      std::string prefix = fibEntry->getPrefix().toUri();
      //NOTE: we need to skip the anchor application, which is not a real producer application:

      /* The node is a producer for prefix fibEntry->getPrefix() */
      if (m_pendingUpdates.find(prefix) == m_pendingUpdates.end()) {
          /* We discovered a new prefix for which we are producer */
          NFD_LOG_INFO("We discovered a new prefix for which we are producer =" << prefix << ". sending trace");
          refreshTraceToAnchor(prefix);
          break;
      }
    }
  }
}

/*-----------------------------------------------------------------------------
 * Overloaded functions
 *----------------------------------------------------------------------------*/

void
ForwarderKite::onContentStoreMiss(const Face& inFace, shared_ptr<pit::Entry>
    pitEntry, const Interest& interest)
{
  shared_ptr<Face> face = const_pointer_cast<Face>(inFace.shared_from_this());
  // insert InRecord
  // XXX For a traced interest, this will make up the trace !!
  pitEntry->insertOrUpdateInRecord(face, interest);

  // set PIT unsatisfy timer
  this->setUnsatisfyTimer(pitEntry);

  bool isForwardedAsTracing = false;
  std::vector<shared_ptr<Face> > traceForwardingOutFaces;

  // We should have : tracename, traceonly and traceable flags...
  // TFT: all traced interests with traceable flag
  //   tracing interests forwarded according to the TFT
  // TNT
  //   traced interests will pull the corresponding tracing interests from the
  //   TNT
  // pull = get pending interests after movement


  /* HOOK : TRACING INTERESTS = consumer interests following a trace
   * traceName should be set in the header
   *
   * we create a temp interest to create a pit entry just to check whether it
   * was existing before = to check for the presence of a trace.
   * if trace follow trace otherwise follow FIB ... based on what...
   *
   * If the PIT entry was already existing, this means someone left a trace
   */
  if (!interest.getTraceName().empty()) {
    NFD_LOG_INFO("1) processed as tracing interest follow traceName=" << interest.getTraceName());
    shared_ptr<Interest> TmpInterest = make_shared<Interest>(interest.getTraceName());

    TmpInterest->setMinSuffixComponents(100);


    std::pair<shared_ptr<pit::Entry>, bool> tfEntryElement = m_pit.insert(*TmpInterest);
    shared_ptr<pit::Entry> tracedPitEntry = tfEntryElement.first;

    // We search for the trace in the PIT !

    if (tfEntryElement.second) { // isNew
      // The second element of the pair is a boolean indicating if the insertion
      // happened (True) or if the call returned an existing PIT entry (False)
      //
      // No trace, we won't be able to forward this interest. Let's wait for a
      // new trace eventually.
      //
      // Note that even if we had forwarded it, it could be an old trace. That's
      // also why we forward on all of them when we find more than one.
      NFD_LOG_TRACE("traced pit entry not found, tracename="<< interest.getTraceName());
    } else {
      NFD_LOG_TRACE("traced pit entry is found" << ", traced pitEntry=" << (void*)(tracedPitEntry.get()) );
      const pit::InRecordCollection& inRecords = tracedPitEntry->getInRecords();

      NFD_LOG_TRACE("traced pitEntry's inrecord size=" << inRecords.size());
      //for(pit::InRecordCollection::const_iterator it=inRecords.begin(); it!=inRecords.end(); ++it) {
      for (auto & it : inRecords) {
        if (!IS_TRACED(it.getInterest())) {
          NFD_LOG_TRACE("meet traced interest, inFace=" << it.getFace()->getId() <<", not traceable");

          continue; /*NOTE: instead of dropping the interest, we continue the processing of interest*/
        }

        shared_ptr<Face> incomingFace = it.getFace();
        NFD_LOG_TRACE("meet traced interest, inFace=" << incomingFace->getId() <<", traceable");

        time::steady_clock::TimePoint now = time::steady_clock::now();
        if (it.getExpiry() < now) {
          NFD_LOG_TRACE("expried");
        } else {
          NFD_LOG_TRACE("not expried");
          if (&inFace == incomingFace.get())
            NFD_LOG_TRACE("its inFace=" << incomingFace->getId() <<", equal to inface of tracing interest, not forwardable to");
          else if(incomingFace->getId() == -1)
            NFD_LOG_TRACE("its inFace=" << incomingFace->getId() <<", now it is down, not forwardable to this face");
          else {
            if (pitEntry->canForwardTo(*incomingFace)) { //if the tracing interest is forwardable to this incoming face
              NFD_LOG_TRACE("tracing interest is forwardable to its inface=" << incomingFace->getId() << ", to be forwarded ...");
              isForwardedAsTracing = true;
///////////
// My changes.
            }
          }
        }
      }
    }
//////////

    //NOTE: check if pullbit is set
    if(m_isPull) {
      NFD_LOG_INFO("insert tracing interest into the TNT(Trace Name Table) entry" << interest.getName());

      //get the TNT entry:
      shared_ptr<pit::TntEntry> pitTntEntry = tracedPitEntry->getStrategyInfo<pit::TntEntry>();
      if (!static_cast<bool>(pitTntEntry)) {
        NFD_LOG_TRACE("TNT entry not found , create it");
        pitTntEntry=make_shared<pit::TntEntry>(tracedPitEntry->getName());
        tracedPitEntry->setStrategyInfo(pitTntEntry);
      }
      NFD_LOG_TRACE("add tracing record");
      // add tracing record:
      pitTntEntry->addTracingRecord(weak_ptr<const Interest>(interest.shared_from_this()),
      weak_ptr<Face>(face), weak_ptr<pit::Entry>(pitEntry));
    } // end m_isPull

    //    NFD_LOG_INFO("add tracing record, successful");
  } // END HOOK : TRACING INTEREST

  /* HOOK : TRACED INTEREST
   *
   * This will leave a trace in the PIT ! it is addressed towards the prefix,
   * advertized by the anchor...
   *
   * Composed of all trace names.
   * TFT contains the trace
   * TNT pending interests
   */
  if (IS_TRACED(interest)) {
    m_onProcessing(interest.getName().toUri(), SEQ_NA, TTL_NA, RETX_NA);
    NFD_LOG_INFO("2) ForwarderKite processing traced interest, creating trace, name=" << interest.getName() << ", traceable" <<", pitEntry=" <<(void*)(pitEntry.get()) << " - inFace=" << inFace.getDescription() );

//  std::cout<<"receiving traced interest="<<interest.getName()<<", at="<<(void*)this<<"\n";
    //NOTE: check if pull bit is set
    if(m_isPull){
      //get TNT (trace name table) entry
      // XXX it might be that we store everything in the same place... -- jordan
      //
      shared_ptr<pit::TntEntry> pitTntEntry = pitEntry->getStrategyInfo<pit::TntEntry>();
      if (static_cast<bool>(pitTntEntry)) {
        NFD_LOG_TRACE("TNT entry (tracing interests) are found");

        // TODO for (auto & it : pitTntEntry->getTracingInterests()) {
        const pit::TntEntry::TracingInterestList& tracingInterestRecords = pitTntEntry->getTracingInterests();
        pit::TntEntry::TracingInterestList::const_iterator it;
        for (it=tracingInterestRecords.begin(); it != tracingInterestRecords.end(); ++it) {

          const pit::TntRecord& TracingRecord = it->second;
          if (!TracingRecord.isValid()) {
            NFD_LOG_TRACE("SHOULD NOT HAPPEN ! meet tracing record. It's not valid, remove it");
            pitTntEntry->removeTracingRecord(it);
            continue;
          }

          NFD_LOG_TRACE("meet tracing record. It's valid");
          //valid check guarantees below are valid share_pt:
          shared_ptr<pit::Entry> tracingPitEntry = TracingRecord.getPitEntry().lock();
          // XXX a single interest by record ??
          shared_ptr<const Interest> tracingInterest = TracingRecord.getInterest().lock();

          if ((TracingRecord.getInFace()).lock().get() == &inFace ||
              ! tracingPitEntry->canForwardTo(inFace)) {
            // XXX How could this happen, since we update
            NFD_LOG_TRACE("SHOULD NOT HAPPEN ! tracing interest, not forwardable to inFace="<< inFace.getId() );
  	        continue;
          }

          NFD_LOG_TRACE("tracing interest is forwardable to inFace=" << inFace.getId() );
          //send the current interest to that incoming face:
          // insert OutRecord
          tracingPitEntry->insertOrUpdateOutRecord(face, *tracingInterest);
          // send Interest
          face->sendInterest(*tracingInterest);
          ++m_counters.getNOutInterests();
          NFD_LOG_INFO("tracing interest=" << tracingInterest->getName()
              << ", pulled by interest="<< interest.getName()
              <<", to face=" << inFace.getId());
        }
      }
    } //end if m_isPull
  }  /* END HOOK : TRACED INTEREST */

  if (isForwardedAsTracing && interest.getTraceForwardingFlag() != 255 &&
      ((interest.getTraceForwardingFlag() >> 1) & 1)) {
    NFD_LOG_INFO("fib lookup is to be skipped");
  } else {
    //NFD_LOG_INFO("will continue processing to the fib");

    // FIB lookup
    shared_ptr<fib::Entry> fibEntry = m_fib.findLongestPrefixMatch(*pitEntry);

    //if this is a traced interest(interest that updates routing)
    if (IS_TRACED(interest)) {
      // don't leave the forwarding operation to strategy layer, instead send it
      // to all nexthops directly to avoid some problems related to
      // loop/supression of traced interest(routing update)
      const fib::NextHopList& nexthops = fibEntry->getNextHops();

      NFD_LOG_INFO("Broadcasting traced interest based on FIB entries" << fibEntry->getPrefix());
      //for(fib::NextHopList::const_iterator it=nexthops.begin(); it!=nexthops.end(); it++)
      if(nexthops.size()!=0) {
        for (auto it : nexthops) {
          shared_ptr<Face> outFace = it.getFace();
	        if(outFace.get()==&inFace) //avoid loops
	          continue;
          //std::cout << "  . outFace " << outFace->getDescription() << std::endl;
          outFace->sendInterest(interest);
          ++m_counters.getNOutInterests();
          pitEntry->insertOrUpdateOutRecord(outFace, interest);

          //logging traced interest transmission at each router
          // LOG: forwarding of Traced interest
          // XXX there is no seq info available, let's put 0 in the mean time. See comment
          // in other places in the code.
          m_onSpecialInterest(interest.getName().toUri(), SEQ_NA, TTL_NA, RETX_NA);
        }
      }
#ifdef WITH_RELIABLE_RETRANSMISSION
      else {
        // THIS IS TO DETECT WE HAVE REACHED THE ANCHOR... it has no FIB entry
	      //automatically send an ack interest back:
        std::ostringstream nstream;
        //NOTE: here it is assumed that prefix has only one name componet like /prefix0 ,
        //we add a sequence number in the ack name to avoid the ack to be pending somewhere
	      //xxx, should be generalized for longer prefix
        nstream<<KITE_TRACE_ACK_PREFIX<<"/"<<m_ack_seq++<<interest.getName().toUri();
        // std::cout <<"***************send back ack interest="<<nstream.str()<<"\n";
        Name ackInterestName(nstream.str());
        shared_ptr<Interest> ackInterest = make_shared<Interest>(ackInterestName);

        ackInterest->setTraceForwardingFlag(2); // interest shall be forwarded using trace only
        ackInterest->setTraceName(interest.getName());
        face->sendInterest(*ackInterest);
        ++m_counters.getNOutInterests();
        pitEntry->insertOrUpdateOutRecord(face, *ackInterest);

        //Ack loggging is dsiabled
        // m_onSpecialInterest(ackInterest->getName().toUri(), SEQ_NA, TTL_NA, RETX_NA);
      }
#endif
    } else {
        // dispatch to strategy
        //NFD_LOG_INFO("Sending interest based on FW strategy and FIB entries");

        //NOTE: the following check is to guarantee that the pit entry created by following trace forwarding will not
        //be removed later by the attempt of FIB forwarding, which makes trace forwarding fail.
        //actually the straggler time of 100ms can prevent us from seeing this problem in most cases.
        if(isForwardedAsTracing) {
          const fib::NextHopList& nexthops = fibEntry->getNextHops();
          if (nexthops.size()!=0 && !(nexthops.size()==1 && nexthops.begin()->getFace().get() == &inFace)) { //if it is neither the case that there's no matching FIB Nor the case that the only output face in FIB is the same as inFace of this interest(loop forwarding)
              this->dispatchToStrategy(pitEntry, bind(&Strategy::afterReceiveInterest, _1,
                  cref(inFace), cref(interest), fibEntry, pitEntry));
          }
        } else {
          this->dispatchToStrategy(pitEntry, bind(&Strategy::afterReceiveInterest, _1,
              cref(inFace), cref(interest), fibEntry, pitEntry));
        }
    }
  }

  if (isForwardedAsTracing) {
    //for(std::vector<shared_ptr<Face> >::iterator it=traceForwardingOutFaces.begin();it!=traceForwardingOutFaces.end();++it)
    NFD_LOG_INFO("insert outrecord for traceforwarding OutFaces");
    for (auto it : traceForwardingOutFaces) {
      NFD_LOG_INFO(" - insert out record");
      pitEntry->insertOrUpdateOutRecord(it, interest);
    }
  }

}


/* Hooks for special interest processing */
void
ForwarderKite::onIncomingInterest(Face& inFace, const Interest& interest)
{
  Interest & interest_ = const_cast<Interest &>(interest);

#ifdef WITH_RELIABLE_RETRANSMISSION
  if( boost::starts_with(interest.getName().toUri(), KITE_TRACE_ACK_PREFIX)) // an ack interest is received
  {
      std::string prefix=interest.getName().getSubName(2, 3).toUri();
//        std::cout<<"recieved ack for "<<prefix<<", at="<<(void*)this<<", from="<<inFace.getId()<<",prefix="<<prefix<<"\n";
      if(!m_pendingUpdates.empty() && m_pendingUpdates.find(prefix)!=m_pendingUpdates.end())
      {
	        scheduler::cancel(m_pendingRetx[prefix]);
//  		std::cout<<"cancel retx for "<<prefix<<", at="<<(void*)this<<", from="<<inFace.getId()<<"\n";
		//then drop the ack
      }
      else
      {
       Forwarder::onIncomingInterest(inFace, interest_);/* Add trace for regular consumer interests */
      }

      return;
  }
#endif

  /* Tag all interests incoming from a local face with KITE flags */
  if (inFace.isLocal() && !boost::starts_with(interest.getName().toUri(), "/localhost")) {
    /* We assume we have a default route in the FIB that we only use to get
     * the traceName, since we prefix all consumer interests with
     * KITE_BASE_TRACENAME
     *
     */
    Name traceName(interest.getName().getSubName(0, 1).toUri());
    NFD_LOG_INFO("Hooked consumer interest with traceName=" << traceName);
    interest_.setTraceName(traceName);
    interest_.setTraceForwardingFlag(KITE_FLAG_FOLLOW_TRACE_AND_FIB);
  }


  //XXX duplicate some codes from forwarder::OnIncomingInterest(), to be fixed later
//here is to avoid content sore lookup for traced interest

   if(IS_TRACED(interest) && interest.getTraceName().empty())// is traced and not tracing interest
   {
     NFD_LOG_DEBUG("onIncomingInterest face=" << inFace.getId() <<
                " interest=" << interest.getName());
     const_cast<Interest&>(interest).setIncomingFaceId(inFace.getId());
     ++m_counters.getNInInterests();

     // /localhost scope control
     bool isViolatingLocalhost = !inFace.isLocal() &&
                              LOCALHOST_NAME.isPrefixOf(interest.getName());
     if (isViolatingLocalhost) {
        NFD_LOG_DEBUG("onIncomingInterest face=" << inFace.getId() <<
                      " interest=" << interest.getName() << " violates /localhost");
         // (drop)
        return;
     }

     // PIT insert
     shared_ptr<pit::Entry> pitEntry = m_pit.insert(interest).first;

     // detect duplicate Nonce
     int dnw = pitEntry->findNonce(interest.getNonce(), inFace);
     bool hasDuplicateNonce = (dnw != pit::DUPLICATE_NONCE_NONE) ||
                           m_deadNonceList.has(interest.getName(), interest.getNonce());
     if (hasDuplicateNonce) {
       // goto Interest loop pipeline
       this->onInterestLoop(inFace, interest_, pitEntry);
       return;
     }

     // cancel unsatisfy & straggler timer
     this->cancelUnsatisfyAndStragglerTimer(pitEntry);

     this->onContentStoreMiss(inFace, pitEntry, interest);
     return;
   }

  /* Add trace for regular consumer interests */
  Forwarder::onIncomingInterest(inFace, interest_);



}

} // namespace nfd 

#endif // KITE