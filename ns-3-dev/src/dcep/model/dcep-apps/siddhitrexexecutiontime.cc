#include <cstdlib>

#include "ns3/dcep.h"
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/dcep-app-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/cep-engine.h"
#include "ns3/stats-module.h"
#include "ns3/data-collector.h"
#include "ns3/time-data-calculators.h"
#include "ns3/common.h"
#include "ns3/log.h"
#include "ns3/communication.h"
#include "ns3/application.h"

#include "siddhitrexexecutiontime.h"


NS_OBJECT_ENSURE_REGISTERED(SiddhiTRexExecutionTimeDcep);
NS_LOG_COMPONENT_DEFINE ("SiddhiTRexExecutionTimeDcep");


TypeId
SiddhiTRexExecutionTimeDcep::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::SiddhiTRexExecutionTimeDcep")
            .SetParent<Dcep> ()
            .SetGroupName("Applications")
            .AddConstructor<SiddhiTRexExecutionTimeDcep> ()
    ;

    return tid;
}

void
SiddhiTRexExecutionTimeDcep::StartApplication (void)
{


    NS_LOG_FUNCTION (this);



    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
    uint32_t mrand = x->GetInteger (1,30);

    auto sink = CreateObject<SiddhiTRexExecutionTimeSink>();
    auto datasource = CreateObject<SiddhiTRexExecutionTimeDataSource>();

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

static Ptr<Query> q1, q2;
static int q1orq2 = 0;

void
SiddhiTRexExecutionTimeDcep::ScheduleEventsFromTrace(Ptr<Query> q)
{
    if (!trace_fn.empty()) {
        auto ds = GetObject<DataSource> ();
        std::string line;
        std::ifstream trace_file;
        trace_file.open (trace_fn);
        Time next_time;

        while (!trace_file.eof()) {
            getline(trace_file, line);
            if (line.empty())
                continue;
            next_time = MicroSeconds(atoi(line.c_str()));

            Ptr<Query> q;
            if (q1orq2 == 1) {
                q = q1;
                q1orq2 = 2;
            } else {
                q = q2;
                q1orq2 = 1;
            }
            Simulator::Schedule (next_time, &DataSource::GenerateAtomicCepEvents, ds, q);
        }
    }
}

void
SiddhiTRexExecutionTimeDcep::ActivateDatasource(Ptr<Query> q)
{
    if (isGenerator()) {
        auto ds = GetObject<DataSource> ();
        if (!ds->IsActive()) {
            ds->Activate();
            UintegerValue nqueries;
            GetAttribute("number_of_queries", nqueries);
            static int cnt = 0;
            int generate_events_in = cnt++;
            ScheduleEventsFromTrace(q);
            if (trace_fn.empty())
                Simulator::Schedule(Seconds(generate_events_in), &DataSource::GenerateAtomicCepEvents, ds, q);
        }
    }
}

/*
 * #######################################################################
 * ####################### SINK ##########################################
 */

TypeId
SiddhiTRexExecutionTimeSink::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::SiddhiTRexExecutionTimeSink")
            .SetParent<Sink>()
    ;
    return tid;
}

void
SiddhiTRexExecutionTimeSink::BuildTRexQueries(Ptr<Dcep> dcep)
{
    UintegerValue nqueries;
    dcep->GetAttribute("number_of_queries", nqueries);
    uint32_t numCepQueries = nqueries.Get();
    std::list<std::string> eventTypes {"BC"}; //, "DE", "FG", "HI", "JK", "LM", "NO", "PQ", "RS", "TU"};
    int complex_event_cnt = 0;
    uint32_t query_counter = 1;
    double in_seconds = 0;
    // The complex queries are dispatched before the atomic queries
    for (auto eventType : eventTypes) {
        auto event1 = eventType.substr(0, 1);
        auto event2 = eventType.substr(1, 1);
        auto parent_output = event1 + "then" + event2;
        for (int temp = 0; temp <= 0; temp++) {
            for (unsigned int j = 0; j < numCepQueries; j++) {
                Ptr<Query> q3 = CreateObject<Query>();  // q3 = complex event
                q3->toBeProcessed = true;
                q3->actionType = NOTIFICATION;
                q3->id = query_counter++;
                q3->isFinal = true;
                q3->isAtomic = false;
                q3->eventType = std::to_string(complex_event_cnt++);
                q3->output_dest = Ipv4Address("10.0.0.3");
                q3->inevent1 = event2;
                q3->inevent2 = event1;
                q3->window = Seconds(150000000000000000);
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
                in_seconds += 1;
                //dcep->DispatchQuery(q3);
            }
        }
    }

    for (auto eventType : eventTypes) {
        auto event1 = eventType.substr(0, 1);
        auto event2 = eventType.substr(1, 1);
        auto parent_output = event1 + "then" + event2;
        for (int temp = 0; temp <= 0; temp++) {
            q2 = CreateObject<Query> ();
            q2->toBeProcessed = dcep->isDistributedExecution();
            q2->actionType = NOTIFICATION;
            q2->id = query_counter++;
            q2->isFinal = false;
            q2->isAtomic = true;
            q2->eventType = event2;
            if (dcep->isDistributedExecution())
                q2->output_dest = Ipv4Address("10.0.0.2");
            else
                q2->output_dest = Ipv4Address("10.0.0.3");
            q2->inevent1 = event2;
            q2->inevent2 = "";
            q2->op = "true";
            q2->assigned = false;
            q2->currentHost.Set("0.0.0.0");
            q2->parent_output = parent_output;
            q2->window = Seconds(150000000000000000);
            q2->isFinalWithinNode = true;
            Ptr<NumberConstraint> c2 = CreateObject<NumberConstraint> ();
            c2->var_name = "percentage";
            c2->numberValue = 25;
            c2->type = LTCONSTRAINT;
            q2->constraints.emplace_back(c2);
            Simulator::Schedule(Seconds(in_seconds), &Dcep::DispatchQuery, dcep, q2);
            in_seconds += 1;

            q1 = CreateObject<Query> ();
            q1->toBeProcessed = dcep->isDistributedExecution();
            q1->actionType = NOTIFICATION;
            q1->id = query_counter++;
            q1->isFinal = false;
            q1->isAtomic = true;
            q1->eventType = event1;
            if (dcep->isDistributedExecution())
                q1->output_dest = Ipv4Address("10.0.0.1");
            else
                q1->output_dest = Ipv4Address("10.0.0.3");
            q1->inevent1 = event1;
            q1->inevent2 = "";
            q1->op = "true";
            q1->assigned = false;
            q1->currentHost.Set("0.0.0.0");
            q1->parent_output = parent_output;
            q1->window = Seconds(150000000000000000);
            q1->isFinalWithinNode = true;  // Meaning that the output is a complex event
            Ptr<NumberConstraint> c1 = CreateObject<NumberConstraint> ();
            c1->var_name = "value";
            c1->numberValue = 45;
            c1->type = GTCONSTRAINT;
            q1->constraints.emplace_back(c1);
            Simulator::Schedule(Seconds(in_seconds), &Dcep::DispatchQuery, dcep, q1);
            in_seconds += 1;
        }
    }
}


/*
 * ########################################################
 * ####################### DATASOURCE #########################
 */


TypeId
SiddhiTRexExecutionTimeDataSource::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::SiddhiTRexExecutionTimeDataSource")
            .SetParent<DataSource> ()
            .AddConstructor<SiddhiTRexExecutionTimeDataSource> ()
    ;
    return tid;
}


void
SiddhiTRexExecutionTimeDataSource::GenerateAtomicCepEvents(Ptr<Query> q) {
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
        NS_LOG_INFO(Simulator::Now() << " CepEvent number " << e->m_seq << " timestamp: " << e->timestamp);
        //dcep->DispatchAtomicCepEvent(e);
        GetObject<CEPEngine>()->ProcessCepEvent(e);

        // Will only schedule events using interval if we don't read from trace. One or the other.
        if(trace_fn.empty() && counter < numCepEvents)
        {
            Simulator::Schedule (MilliSeconds (cepEventsInterval), &DataSource::GenerateAtomicCepEvents, this, q);
        }
    }

}