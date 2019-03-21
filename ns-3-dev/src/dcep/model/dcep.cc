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

#include "dcep.h"
#include "ns3/node.h"
#include "ns3/uinteger.h"
#include "ns3/object.h"
#include "ns3/log.h"
#include "ns3/random-variable-stream.h"
#include "placement.h"
#include "communication.h"
#include "cep-engine.h"
#include "common.h"
#include "ns3/simulator.h"
#include "ns3/config.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/processing-module.h"
#include "ns3/object-base.h"
#include "dcep-state.h"

#include <ctime>
#include <chrono>
#include <iostream>
#include <fstream>
#include <random>

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(Dcep);
NS_LOG_COMPONENT_DEFINE ("Dcep");


    TypeId
    Dcep::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::Dcep")
        .SetParent<Application> ()
        .SetGroupName("Applications")
        .AddConstructor<Dcep> ()
        .AddAttribute ("placement_policy", "The placement mechanism policy being used",
                       StringValue("centralized"),
                       MakeStringAccessor (&Dcep::placementPolicy),
                       MakeStringChecker())
        .AddAttribute ("IsGenerator",
                       "This attribute is used to configure the current node as a datasource",
                       BooleanValue (false),
                       MakeBooleanAccessor (&Dcep::datasource_node),
                       MakeBooleanChecker ())
        .AddAttribute ("IsSink",
                       "This attribute is used to configure the current node as a Sink",
                       BooleanValue (false),
                       MakeBooleanAccessor (&Dcep::sink_node),
                       MakeBooleanChecker ())
        .AddAttribute ("SinkAddress",
                       "The destination Address of the outbound packets",
                       Ipv4AddressValue (),
                       MakeIpv4AddressAccessor (&Dcep::m_sinkAddress),
                       MakeIpv4AddressChecker ())
        .AddAttribute ("event_code",
                       "The descriptor for the type of event generated by a datasource",
                       UintegerValue (0),
                       MakeUintegerAccessor (&Dcep::event_code),
                       MakeUintegerChecker<uint16_t> ())
        .AddAttribute ("number_of_events", "The number_of_events to be generated by a datasource",
                       UintegerValue (100),
                       MakeUintegerAccessor (&Dcep::events_load),
                       MakeUintegerChecker<uint32_t> ())
        .AddAttribute ("event_interval", "The interval in between each event that is generated by a datasource",
                       UintegerValue (1000000),
                       MakeUintegerAccessor (&Dcep::event_interval),
                       MakeUintegerChecker<uint64_t> ())
        .AddAttribute ("routing_protocol", "The routing_protocol being used",
                       StringValue("olsr"),
                       MakeStringAccessor (&Dcep::routing_protocol),
                       MakeStringChecker())
        .AddTraceSource ("RxFinalCepEvent",
                         "a new final event has been detected.",
                         MakeTraceSourceAccessor (&Dcep::RxFinalCepEvent))
        .AddTraceSource ("RxFinalCepEventHops",
                         "",
                         MakeTraceSourceAccessor (&Dcep::RxFinalCepEventHops))
        .AddTraceSource ("RxFinalCepEventDelay",
                         "",
                         MakeTraceSourceAccessor (&Dcep::RxFinalCepEventDelay))
        
        ;
        
        return tid;
    }
    
     
    Dcep::Dcep()
    {
        this->m_cepPort = 100;
    }
    
    uint16_t
    Dcep::getCepEventCode()
    {
        return this->event_code;
    }
    
    void
    Dcep::StartApplication (void)
    {
        
        
        NS_LOG_FUNCTION (this);
        
        
        
        Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
        uint32_t mrand = x->GetInteger (1,30);
        mrand = mrand;
        std::cout << "generated number: " << mrand << std::endl;

        Ptr<Sink> sink = CreateObject<Sink>();
        Ptr<DataSource> datasource = CreateObject<DataSource>();

        AggregateObject (sink);
        AggregateObject (datasource);
        
        Ptr<Placement> c_placement = CreateObject<Placement> ();
        
        AggregateObject (c_placement);
       
        Ptr<CEPEngine> c_cepengine = CreateObject<CEPEngine> ();
        AggregateObject (c_cepengine);
        
        Ptr<Communication> c_communication = CreateObject<Communication> ();
        AggregateObject (c_communication);
        
        c_placement->configure();
        c_cepengine->Configure();
        datasource->Configure();
        
        c_communication->setNode(GetNode());
        c_communication->setPort(m_cepPort);
        c_communication->SetAttribute("SinkAddress", Ipv4AddressValue (m_sinkAddress));
        c_communication->Configure();
        
        NS_LOG_INFO(Simulator::Now() << " STARTED DCEP APPLICATION AT NODE " << c_communication->GetLocalAddress());
        
        if(sink_node)
        {
            Simulator::Schedule (Seconds (20.0), &Sink::BuildAndSendQuery, sink);
        }
          
    }
    
    uint32_t
    Dcep::getNumCepEvents()
    {
        return this->events_load;
    }
    
    bool
    Dcep::isGenerator()
    {
        return this->datasource_node;
    }
    
    void
    Dcep::StopApplication (void)
    {
        NS_LOG_FUNCTION (this);
    }
    
    void
    Dcep::SendPacket(Ptr<Packet> p, Ipv4Address addr)
    {
        Ptr<Communication> comm = GetObject<Communication>();
        NS_LOG_INFO (Simulator::Now() << " DCEP: Sending packet from " << comm->GetLocalAddress() << " to destination " << addr);
        comm->ScheduleSend(p, addr);
    }
    
    void
    Dcep::DispatchQuery(Ptr<Query> q)
    {
        NS_LOG_FUNCTION(this);
        NS_LOG_INFO(Simulator::Now() << " DCEP: received query to dispatch");

        GetObject<Placement>()->RecvQuery(q);
    }
    
    void
    Dcep::DispatchAtomicCepEvent(Ptr<CepEvent> e)
    {
        NS_LOG_INFO (Simulator::Now() << " DCEP: Dispatching atomic event");
        GetObject<Placement>()->ForwardProducedCepEvent(e);
    }
    
    void
    Dcep::ActivateDatasource(Ptr<Query> q)
    {
        auto ds = GetObject<DataSource> ();
        //Simulator::Schedule(Seconds(10), &DataSource::GenerateAtomicCepEvents, GetObject<DataSource>());
        if (!ds->IsActive()) {
            ds->Activate();
            ds->GenerateAtomicCepEvents(q);
        }
    }
    
    void 
    Dcep::SendFinalCepEventToSink(Ptr<CepEvent> event)
    {
        this->RxFinalCepEvent(1);
        this->RxFinalCepEventDelay(event->delay);
        this->RxFinalCepEventHops(event->hopsCount);

        GetObject<Sink>()->receiveFinalCepEvent(event);
    }
    
    void
    Dcep::rcvRemoteMsg(uint8_t* data, uint32_t size, uint16_t msg_type, uint64_t delay)
    {

        Ptr<Placement> p = GetObject<Placement>();

        switch(msg_type)
        {
            case EVENT: /*handle event*/
            {
                Ptr<CepEvent> event = CreateObject<CepEvent>();

                event->deserialize(data, size);
                /* setting link delay from source to this node*/
                event->delay = delay;

                static int cnt = 0;
                NS_LOG_INFO (Simulator::Now() << " DCEP: RECEIVED EVENT MESSAGE OF TYPE " << event->type << ", SEQ NO " << event->m_seq << ", RCVD EVENT NUMBER " << ++cnt);

                Ptr<Packet> pkt = Create<Packet>(data, size);
                event->pkt = pkt;

                p->RcvCepEvent(event);

                break;
            }

            case QUERY: /* handle query*/
            {
                NS_LOG_INFO (Simulator::Now() << " DCEP: RECEIVED QUERY MESSAGE");
                Ptr<Query> q = CreateObject<Query>();
                q->deserialize(data, size);
                p->RecvQuery(q);
                // Since the send-packet SEM is invoked for each packet to be sent, query or event, maybe we should
                // invoke a HIRQ here as well, but with different state condition variable. (packet type EVENT or QUERY)
                break;
            }
                
            default:
                NS_LOG_INFO(Simulator::Now() << " dcep: unrecognized remote message");
                
        }
    }
    
    
    /*
     * #######################################################################
     * ####################### SINK ##########################################
     */
    
    TypeId
    Sink::GetTypeId (void)
    {
      static TypeId tid = TypeId ("ns3::Sink")
        .SetParent<Object> ()
        .AddConstructor<Sink> ()
        .AddTraceSource("new-query",
              "A newly generated query",
              MakeTraceSourceAccessor (&Sink::nquery)
              )

      ;
      return tid;
    }

    Sink::Sink ()
    {
      NS_LOG_FUNCTION (this);
    }

    Sink::~Sink ()
    {
      NS_LOG_FUNCTION (this);
    }

    void
    Sink::receiveFinalCepEvent(Ptr<CepEvent> e)
    {
        NS_LOG_INFO(Simulator::Now() << " COMPLEX EVENT RECEIVED " << ++number_received << " EVENTS, NOTIFIED HOPSCOUNT " << e->hopsCount << " e->timestamp " << e->timestamp << " DELAY " << Simulator::Now() - e->timestamp);
        NS_LOG_INFO(Simulator::Now() << " $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ COMPLEX EVENT $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$, temperature: " << e->numberValues["value"]);
    }

    void
    Sink::BuildTRexQueries(Ptr<Dcep> dcep)
    {
        std::list<std::string> eventTypes {"BC"}; //, "DE", "FG", "HI", "JK", "LM", "NO", "PQ", "RS", "TU"};
        int complex_event_cnt = 0;
        uint32_t query_counter = 1;
        double in_seconds = 0;
        for (auto eventType : eventTypes)
        {
            auto event1 = eventType.substr(0, 1);
            auto event2 = eventType.substr(1, 1);
            auto parent_output = event1 + "then" + event2;
            for (int temp = 0; temp <= 0; temp++)
            {
                Ptr<Query> q1 = CreateObject<Query> ();
                q1->actionType = NOTIFICATION;
                q1->id = query_counter++;
                q1->isFinal = false;
                q1->isAtomic = true;
                q1->eventType = event1;
                q1->output_dest = Ipv4Address("10.0.0.1");
                q1->inevent1 = event1;
                q1->inevent2 = "";
                q1->op = "true";
                q1->assigned = false;
                q1->currentHost.Set("0.0.0.0");
                q1->parent_output = parent_output;
                q1->window = Seconds(15);
                q1->isFinalWithinNode = true;  // Meaning that the output is a complex event
                Ptr<NumberConstraint> c1 = CreateObject<NumberConstraint> ();
                c1->var_name = "value";
                c1->numberValue = 45;
                c1->type = GTCONSTRAINT;
                q1->constraints.emplace_back(c1);
                Simulator::Schedule(Seconds(in_seconds), &Dcep::DispatchQuery, dcep, q1);
                in_seconds += 0.01;

                Ptr<Query> q2 = CreateObject<Query> ();
                q2->actionType = NOTIFICATION;
                q2->id = query_counter++;
                q2->isFinal = false;
                q2->isAtomic = true;
                q2->eventType = event2;
                q2->output_dest = Ipv4Address("10.0.0.2");
                q2->inevent1 = event2;
                q2->inevent2 = "";
                q2->op = "true";
                q2->assigned = false;
                q2->currentHost.Set("0.0.0.0");
                q2->parent_output = parent_output;
                q2->window = Seconds(15);
                q2->isFinalWithinNode = true;
                Ptr<NumberConstraint> c2 = CreateObject<NumberConstraint> ();
                c2->var_name = "percentage";
                c2->numberValue = 25;
                c2->type = LTCONSTRAINT;
                q2->constraints.emplace_back(c2);
                Simulator::Schedule(Seconds(in_seconds), &Dcep::DispatchQuery, dcep, q2);
                in_seconds += 0.01;

                for (int j = 0; j <= 0; j++) {
                    Ptr<Query> q3 = CreateObject<Query>();  // q3 = complex event
                    q3->actionType = NOTIFICATION;
                    q3->id = query_counter++;
                    q3->isFinal = true;
                    q3->isAtomic = false;
                    q3->eventType = std::to_string(complex_event_cnt++);
                    q3->output_dest = Ipv4Address("10.0.0.3");
                    q3->inevent1 = event1;
                    q3->inevent2 = event2;
                    q3->window = Seconds(15);
                    q3->isFinalWithinNode = true;

                    Ptr<StringConstraint> c3 = CreateObject<StringConstraint>();
                    c3->var_name = "area";
                    c3->stringValue = "office";
                    c3->type = EQCONSTRAINT;
                    q3->constraints.emplace_back(c3);
                    q3->op = "then";
                    q3->assigned = false;
                    q3->currentHost.Set("0.0.0.0");
                    q3->parent_output = parent_output;
                    Simulator::Schedule(Seconds(in_seconds), &Dcep::DispatchQuery, dcep, q3);
                    //in_seconds += 0.01;
                    //dcep->DispatchQuery(q3);
                }

                /*for (int j = 0; j <= 0; j++) {
                    Ptr<Query> q3 = CreateObject<Query>();  // q3 = complex event
                    q3->actionType = NOTIFICATION;
                    q3->id = query_counter++;
                    q3->isFinal = true;
                    q3->isAtomic = true;
                    q3->eventType = std::to_string(complex_event_cnt++);
                    q3->output_dest = Ipv4Address("10.0.0.3");
                    q3->inevent1 = event1;
                    q3->inevent2 = "";
                    q3->window = Seconds(15);
                    q3->isFinalWithinNode = true;

                    Ptr<StringConstraint> c3 = CreateObject<StringConstraint>();
                    q3->op = "true";
                    q3->assigned = false;
                    q3->currentHost.Set("0.0.0.0");
                    q3->parent_output = parent_output;
                    Simulator::Schedule(Seconds(in_seconds), &Dcep::DispatchQuery, dcep, q3);
                    in_seconds += 0.01;
                }*/
            }
        }
    }

    void
    Sink::BuildAndSendQuery(){

       uint32_t query_counter = 1;

        /**
         * create and configure the query
         * 
         */
        Ptr<Dcep> dcep = GetObject<Dcep> ();
        BuildTRexQueries(dcep);
        /*
        Ptr<Query> q1 = CreateObject<Query> ();
       
        q1->actionType = NOTIFICATION;

        q1->id = query_counter++;

        q1->isFinal = false;
        q1->isAtomic = true;
        q1->eventType = "B";
        q1->output_dest = Ipv4Address::GetAny();
        q1->inevent1 = "B";
        q1->inevent2 = "";
        q1->op = "true";
        q1->assigned = false;
        q1->currentHost.Set("0.0.0.0");
        q1->parent_output = "BorC";
        NS_LOG_INFO ("Setup query " << q1->eventType);
        dcep->DispatchQuery(q1);

        Ptr<Query> q2 = CreateObject<Query> ();
        q2->actionType = NOTIFICATION;

        q2->id = query_counter++;

        q2->isFinal = false;
        q2->isAtomic = true;
        q2->eventType = "C";
        q2->output_dest = Ipv4Address::GetAny();
        q2->inevent1 = "C";
        q2->inevent2 = "";
        q2->op = "true";
        q2->assigned = false;
        q2->currentHost.Set("0.0.0.0");
        q2->parent_output = "BorC";
        NS_LOG_INFO ("Setup query " << q2->eventType);
        dcep->DispatchQuery(q2);

        Ptr<Query> q3 = CreateObject<Query> ();
        q3->actionType = NOTIFICATION;

        q3->id = query_counter++;

        q3->isFinal = true;
        q3->isAtomic = false;
        q3->eventType = "C";  // Used to be "AthenB", but that is unnecessary
        q3->output_dest = Ipv4Address::GetAny();
        q3->inevent1 = "A";
        q3->inevent2 = "B";
        q3->op = "then";
        q3->assigned = false;
        q3->currentHost.Set("0.0.0.0");
        q3->parent_output = "AthenB";
        NS_LOG_INFO ("Setup query " << q3->eventType);
        //dcep->DispatchQuery(q3);

        Ptr<Query> q4 = CreateObject<Query> ();
        q4->actionType = NOTIFICATION;

        q4->id = query_counter++;

        q4->isFinal = true;
        q4->isAtomic = false;
        q4->eventType = "D";
        q4->output_dest = Ipv4Address::GetAny();
        q4->inevent1 = "A";
        q4->inevent2 = "B";
        q4->op = "and";
        q4->assigned = false;
        q4->currentHost.Set("0.0.0.0");
        q4->parent_output = "AandB";
        NS_LOG_INFO ("Setup query " << q4->eventType);
        //dcep->DispatchQuery(q4);

        Ptr<Query> q5 = CreateObject<Query> ();
        q5->actionType = NOTIFICATION;

        q5->id = query_counter++;

        q5->isFinal = true;
        q5->isAtomic = false;
        q5->eventType = "E";
        q5->output_dest = Ipv4Address::GetAny();
        q5->inevent1 = "A";
        q5->inevent2 = "B";
        q5->op = "or";
        q5->assigned = false;
        q5->currentHost.Set("0.0.0.0");
        q5->parent_output = "AorB";
        NS_LOG_INFO ("Setup query " << q5->eventType);
        dcep->DispatchQuery(q5);*/
        

    }
    
    
    
    /*
     * ########################################################
     * ####################### DATASOURCE #########################
     */
    
    
    
    TypeId
    DataSource::GetTypeId (void)
    {
      static TypeId tid = TypeId ("ns3::Datasource")
        .SetParent<Object> ()
        .AddConstructor<DataSource> ()
        .AddTraceSource ("CepEvent",
                       "New CEP event from data source.",
                       MakeTraceSourceAccessor (&DataSource::nevent))
      
      ;
      return tid;
    }

    DataSource::DataSource ()
    : counter(0) 
    {
      NS_LOG_FUNCTION (this);
      
    }
    
    void
    DataSource::Configure()
    {
      Ptr<Dcep> dcep = GetObject<Dcep>();
      UintegerValue ecode, nevents, interval;
      
      dcep->GetAttribute("event_code", ecode);
      dcep->GetAttribute("number_of_events", nevents);
      dcep->GetAttribute("event_interval", interval);
      eventCode = ecode.Get();
      numCepEvents = nevents.Get();
      cepEventsInterval = interval.Get();
    }

    DataSource::~DataSource ()
    {
      NS_LOG_FUNCTION (this);
    }

    bool DataSource::IsActive()
    {
        return active;
    }

    void DataSource::Activate()
    {
        active = true;
    }

    void
    DataSource::GenerateAtomicCepEvents(Ptr<Query> q){

        std::string eventType = q->eventType;
        Ptr<Dcep> dcep = GetObject<Dcep>();
        Ptr<Node> node = dcep->GetNode();

        Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();

        NS_LOG_INFO (Simulator::Now() << " Starting to generate events of type " << eventCode );

        uint32_t random_number = x->GetInteger (1,99999);
        if (eventType == "B") {
            m_eventType = eventType;
            m_eventNumberValues["value"] = 46;
            m_eventStringValues["area"] = "office";
        } else if (eventType == "C") {
            m_eventType = eventType;
            m_eventNumberValues["percentage"] = 21;
            m_eventStringValues["area"] = "office";
        }

        if(m_eventType != " ")
        {
            counter++;
            Ptr<CepEvent> e = CreateObject<CepEvent>();
            NS_LOG_INFO(Simulator::Now() << " creating event of type " << m_eventType << " cnt " << counter);
            e->type = m_eventType;
            e->event_class = ATOMIC_EVENT;
            e->delay = 0; //initializing delay
            e->m_seq = counter;
            e->hopsCount = 0;
            e->prevHopsCount = 0;
            e->numberValues = m_eventNumberValues;
            e->stringValues = m_eventStringValues;
            e->timestamp = Simulator::Now();
            e->pkt = Create<Packet>();  // Dummy packet for processing delay
            NS_LOG_INFO(Simulator::Now() << " CepEvent number  " << e->m_seq << " timestamp: " << e->timestamp);
            //dcep->DispatchAtomicCepEvent(e);
            GetObject<CEPEngine>()->ProcessCepEvent(e);

            if(counter < numCepEvents)
            {
                Simulator::Schedule (NanoSeconds (cepEventsInterval), &DataSource::GenerateAtomicCepEvents, this, q);
            }
        }
            
    }
    
}
