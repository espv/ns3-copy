/*
 * Copyright (C) 2018, Fabrice S. Bigirimana
 * Copyright (c) 2018, University of Oslo
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * 
 */


#include <string.h>
#include "placement.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"
#include "ns3/log.h"
#include "ns3/object.h"
#include "dcep.h"
#include "ns3/ipv4.h"
#include "ns3/string.h"
#include "ns3/object.h"
#include "communication.h"
#include "cep-engine.h"
#include "common.h"
#include "message-types.h"
#include "dcep-header.h"
#include "ns3/abort.h"
#include "resource-manager.h"
#include "ns3/ipv4-address.h"
#include "dcep-state.h"
#include "ns3/processing-module.h"

namespace ns3 {

    NS_OBJECT_ENSURE_REGISTERED(Placement);
    NS_OBJECT_ENSURE_REGISTERED(PlacementPolicy);
    NS_OBJECT_ENSURE_REGISTERED(CentralizedPlacementPolicy);
    NS_LOG_COMPONENT_DEFINE("Placement");

        /* ... */
    TypeId
    Placement::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::Placement")
                .SetParent<Object> ()
                .AddConstructor<Placement> ()
                .AddTraceSource("activate datasource",
                "when an atomic query for this data source is received.",
                MakeTraceSourceAccessor(&Placement::activateDatasource))
                .AddTraceSource("Remote event received",
                "A local event must be forwarded to the local cep engine",
                MakeTraceSourceAccessor(&Placement::remoteCepEventReceived))
                .AddTraceSource("new event produced",
                "A new event is produced by the local CEPEngine",
                MakeTraceSourceAccessor(&Placement::m_newCepEventProduced))
                
                ;
        return tid;
    }

    void
    Placement::configure() {

        if (this->GetAggregateIterator().HasNext()) {
            
            Ptr<Dcep> dcep = GetObject<Dcep>();
            
            StringValue s1;
            dcep->GetAttribute("placement_policy", s1);
            Ptr<PlacementPolicy> p_policy;
            
            
            if (s1.Get() == "centralized")// the default
            {
                NS_LOG_INFO(Simulator::Now() << " Centralized placement mechanism");

                p_policy = CreateObject<CentralizedPlacementPolicy>();

            }
            else
            {
                NS_ABORT_MSG ("UNKNOWN PLACEMENT POLICY");
            }
            
            AggregateObject(p_policy);
            p_policy->configure();
            
            
            Ptr<ResourceManager> rm = CreateObject<ResourceManager>();
            AggregateObject(rm);
            rm->Configure();
    
            
            /* Aggregate dcep state object*/
            Ptr<DcepState> dstate = CreateObject<DcepState>();
            AggregateObject(dstate);
            dstate->Configure();

            dstate->SetNextHop("0", Ipv4Address("10.0.0.4"));
            
        }

    }
    

    /**
     ************************************************
     * ********************* EVENT FORWARDING ************************************
     * ***************************************************************************
     */
    void
    Placement::RcvCepEvent(Ptr<CepEvent> e)
    {
        remoteCepEventReceived (e);
        
        if (e->event_class == FINAL_EVENT) {
            Ptr<ExecEnv> ee = GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();
            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("AllCepOpsDoneYet")->value = 1;
            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 1;
            SendCepEventToSink(e);
        }
        else
        {
            if(GetObject<DcepState>()->IsExpected(e))
            {
                SendCepEventToCepEngine(e);
            }
            else
            {
                NS_ABORT_MSG("PLACEMENT: UNEXPECTED EVENT");
            }
        }
    }

    
    void
    Placement::ForwardProducedCepEvent(Ptr<CepEvent> e)
    {
        Ptr<Node> node = GetObject<Dcep> ()->GetNode();
        Ptr<DcepState> dstate = GetObject<DcepState>();
        Ipv4Address dest = dstate->GetOuputDest(e->type);
        
        m_newCepEventProduced (e);

        if (dest.IsEqual(GetObject<Communication>()->GetLocalAddress()))
        {
            //if (dstate->IsActive(e->type))
            //{
                SendCepEventToCepEngine(e);
            //}
            //else
            //{
                //NS_ABORT_MSG ("PLACEMENT MECHANISM: CORRESPONDING OPERATOR NOT ACTIVE");
            //}
        }
        else
        {
            if(!dest.IsAny())
            {
                /*if (e->event_class == INTERMEDIATE_EVENT) {
                    SendCepEvent (e, dest);
                } else {
                    Ptr<ExecEnv> ee = GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();
                    ee->Proceed(1, e->pkt, "send-packet", &Placement::SendCepEvent, this, e, dest);
                }*/
                SendCepEvent(e, dest);
            }
            else
            {
                NS_ABORT_MSG ("PLACEMENT MECHANISM: NO DESTINATION PROVIDED FOR CURRENT EVENT");
            }
        }
         
    }
    
    
    void
    Placement::SendCepEventToCepEngine (Ptr<CepEvent> e)
    {
        GetObject<CEPEngine>()->ProcessCepEvent(e);
    }


    Ipv4Address
    Placement::SinkAddressForEvent(Ptr<CepEvent> e)
    {
        return Ipv4Address("10.0.0.4");
    }

    
    void
    Placement::SendCepEventToSink(Ptr<CepEvent> e)
    {
        //GetObject<Dcep>()->SendFinalCepEventToSink(e);
        auto dest = SinkAddressForEvent(e);
        SendCepEvent(e, dest);
    }


    void
    Placement::SendCepEvent(Ptr<CepEvent> e, Ipv4Address dest)
    {
        olsr::RoutingTableEntry entry;
        
        entry.destAddr = dest;
        if (GetObject<ResourceManager>()->getRoute(entry) == -1) {
            GetObject<Dcep>()->SendFinalCepEventToSink(e);
            return;
        }
        if(entry.distance > 0)
        {
            //set here and when nely produced
            e->hopsCount = entry.distance + e->hopsCount;
            
            SerializedCepEvent *message = e->serialize();
            auto buffer = new uint8_t[message->size];
            memcpy(buffer, message, message->size);
            DcepHeader dcepHeader;
            dcepHeader.SetContentType(EVENT);
            dcepHeader.setContentSize(message->size);

            Ptr<Packet> p = Create<Packet> (buffer, message->size);

            p->AddHeader (dcepHeader);
            
            GetObject<Dcep>()->SendPacket(p, dest);
        }
        else
        {
            eventsList.push_back(e);
            Simulator::Schedule(MilliSeconds(100.0), &Placement::SendCepEvent, this, e, dest);
        }
    }
    
    
    /*************************************************
     * *********************** OPERATOR NETWORK CONSTRUCTION  *********************
     * ****************************************************************************
     */
    
    
    void
    Placement::RecvQuery(Ptr<Query> q) 
    {
        q_queue.push_back(q);
        Simulator::Schedule(Seconds(0.0), &PlacementPolicy::DoPlacement, GetObject<PlacementPolicy>());
    }
    
    
    uint32_t 
    Placement::RemoveQuery(Ptr<Query> q) 
    {
        std::vector<Ptr<Query> >::iterator it;
        for (it = q_queue.begin(); it != q_queue.end(); it++)
        {
            Ptr<Query> qry = *it;
            if (qry->id == q->id)
            {
                q_queue.erase(it);
                break;
            }
                
        }
        return q_queue.size();
    }


    void 
    Placement::ForwardRemoteQuery(std::string eType)
    {
        NS_LOG_INFO (Simulator::Now() << " PLACEMENT: SENDING QUERY TO REMOTE NODE");
        
        Ptr<DcepState> dstate = GetObject<DcepState>();
        SerializedQuery *message= dstate->GetQuery(eType)->serialize();
        NS_LOG_INFO (Simulator::Now() << " QUERY BEING SENT " << message->eventType);
        uint8_t *buffer = new uint8_t[message->size];
        memcpy(buffer, message, message->size);

        uint16_t msgType = QUERY;
        DcepHeader dcepHeader;
        dcepHeader.SetContentType(msgType);
        dcepHeader.setContentSize(message->size);

        Ptr<Packet> p = Create<Packet> (buffer, message->size);
        
        p->AddHeader (dcepHeader);
        static float seconds_to_send = 0;
        Simulator::Schedule(Seconds(seconds_to_send), &Dcep::SendPacket, GetObject<Dcep> (), p, dstate->GetNextHop(eType));
        // seconds_to_send is incremented to delay the transmission of queries.
        // Otherwise, some buffer will be overflowed and queries will be dropped.
        seconds_to_send += 0.01;
        //GetObject<Dcep>()->SendPacket(p, dstate->GetNextHop(eType));

    }
    
    void 
    Placement::SendQueryToCepEngine(Ptr<Query> q)
    {
        GetObject<CEPEngine>()->RecvQuery(q);
    }

    void Placement::ForwardQuery(Ptr<Query> q)
    {
        auto eType = q->eventType;

        NS_LOG_INFO(Simulator::Now() << " Forwarding partial query to its destination");
        Ptr<DcepState> dstate = GetObject<DcepState>();
        
        if (dstate->GetNextHop(eType).IsAny())
        {
            NS_ABORT_MSG ("ASKED TO FORWARD A QUERY WITH NO DESTINATION ADDRESS!");
        }
        else if (dstate->GetNextHop(eType).IsEqual(GetObject<Communication>()->GetLocalAddress()))
        {
            
            if(dstate->GetQuery(eType)->isAtomic)
            {
                GetObject<CEPEngine>()->RecvQuery(q);
            }
            else/* Send to local CEP engine*/
                SendQueryToCepEngine (dstate->GetQuery(eType));
        }
        else
        {
            /* Send to remote destination */
            ForwardRemoteQuery(eType);
        }
 
    }
    
    /*
     * ********************** PLACEMENT POLICIES ***********************
     * *****************************************************************
     * *****************************************************************
     */


    TypeId PlacementPolicy::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::PlacementPolicy")
        .SetParent<Object>()
        .AddTraceSource("New host found",
                "A new host has been found",
                MakeTraceSourceAccessor(&PlacementPolicy::newHostFound))
        .AddTraceSource("new local placement",
                "a query placed on a remote node.",
                MakeTraceSourceAccessor(&PlacementPolicy::newLocalPlacement))
        ;
        return tid;
    }

    TypeId CentralizedPlacementPolicy::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::CentralizedPlacementPolicy")
                .SetParent<PlacementPolicy> ()
                .AddConstructor<CentralizedPlacementPolicy> ()

                ;
        return tid;
    }

    void
    CentralizedPlacementPolicy::configure() {
        
    }

    void
    CentralizedPlacementPolicy::DoPlacement() 
    {
        NS_LOG_INFO (Simulator::Now() << " Doing centralized placement");
        Ptr<Placement> p = GetObject<Placement>();

        std::vector<Ptr < Query>>::iterator it;
        std::vector<Ptr < Query>> qs = p->q_queue;

        for (it = qs.begin(); it != qs.end(); ++it) {

            Ptr<Query> q = *it;
            if (!PlaceQuery(q)) 
            {
                Simulator::Schedule(Seconds(3.0), &CentralizedPlacementPolicy::DoPlacement, this);
            } else {
                p->RemoveQuery(q);
            }
        }

    }

    bool
    CentralizedPlacementPolicy::doAdaptation(std::string eType) 
    {
        return false;
    }

    bool
    CentralizedPlacementPolicy::PlaceQuery(Ptr<Query> q) {
        
        Ptr<Placement> p = GetObject<Placement>();
        Ptr<DcepState> dstate = GetObject<DcepState>();
        Ptr<Node> node = GetObject<Dcep> ()->GetNode();
        dstate->CreateCepEventRoutingTableEntry(q);
        Ptr<Communication> cm = GetObject<Communication>();
        bool placed = false;

        if (!q->isAtomic) 
        {
            dstate->SetNextHop(q->eventType, cm->GetLocalAddress());
            placed = true;
        }
        else if (q->isAtomic) 
        {
            /*if(q->eventType == "A")
            {
                dstate->SetNextHop(q->eventType, Ipv4Address("10.0.0.2"));
                placed = true;
                
            }
            else*/
            if(q->eventType == "B")
            {
                dstate->SetNextHop(q->eventType, Ipv4Address("10.0.0.1"));
                placed = true;
                
            }
            else if (q->eventType == "C")
            {
                dstate->SetNextHop(q->eventType, Ipv4Address("10.0.0.2"));
                placed = true;
                
            }
            /*else if (q->eventType == "D")
            {
                dstate->SetNextHop(q->eventType, Ipv4Address("10.0.0.5"));
                placed = true;
                
            }
            else if (q->eventType == "E") 
            {
                dstate->SetNextHop(q->eventType, Ipv4Address("10.0.0.1"));
                placed = true;
            }
            else if (q->eventType == "F") 
            {
                dstate->SetNextHop(q->eventType, Ipv4Address("10.0.0.7"));
                placed = true;
            }
            else if (q->eventType == "G") 
            {
                dstate->SetNextHop(q->eventType, Ipv4Address("10.0.0.8"));
                placed = true;
            }
            else if (q->eventType == "H") 
            {
                dstate->SetNextHop(q->eventType, Ipv4Address("10.0.0.9"));
                placed = true;
            }*/

            //dstate->SetNextHop(q->eventType, "10.0.0.1");
            //placed = true;

        }

        if (placed) 
        {
            NS_LOG_INFO (Simulator::Now() << " QUERY WILL BE PLACED");
            newLocalPlacement(q->eventType);

            p->ForwardQuery(q);
        }

        return placed;
    }
    

    

}
