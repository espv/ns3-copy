#include <cstdlib>
#include <iostream>
#include <iomanip>

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

#include "siddhitrexthroughput.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;


NS_OBJECT_ENSURE_REGISTERED(SiddhiTRexThroughputDcep);
NS_LOG_COMPONENT_DEFINE ("SiddhiTRexThroughputDcep");


TypeId
SiddhiTRexThroughputDcep::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::SiddhiTRexThroughputDcep")
            .SetParent<Dcep> ()
            .SetGroupName("Applications")
            .AddConstructor<SiddhiTRexThroughputDcep> ()
    ;

    return tid;
}

void
SiddhiTRexThroughputDcep::StartApplication (void)
{


    NS_LOG_FUNCTION (this);



    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
    uint32_t mrand = x->GetInteger (1,30);
    mrand = mrand;

    auto sink = CreateObject<SiddhiTRexThroughputSink>();
    auto datasource = CreateObject<SiddhiTRexThroughputDataSource>();

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
      //Simulator::Schedule (Seconds (20.0), &Sink::BuildAndSendQuery, sink);
      ActivateDatasource(CreateObject<Query>());
    }

}

std::vector<std::string> split(const std::string& s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}


void
SiddhiTRexThroughputDcep::ScheduleEventsFromTrace(Ptr<Query> q)
{
    if (!experiment_metadata_fn.empty() && !trace_fn.empty()) {
        std::ifstream file(experiment_metadata_fn);
        json j;
        file >> j;
        std::cout << std::setw(4) << j["tracepoints"] << std::endl;

        auto ds = GetObject<DataSource> ();
        std::map<int, json> metadata;
        for (auto tracepoint : j["tracepoints"]) {
            if (tracepoint["category"]["isSimulationEvent"]) {
                metadata[(int)tracepoint["id"]] = tracepoint;
            }
        }

        std::string line;
        std::ifstream trace_file;
        trace_file.open (trace_fn);
        Time next_time;

        while (!trace_file.eof()) {
            getline(trace_file, line);
            if (line.empty())
                continue;

            auto splitLine = split(line, '\t');
            int argument = std::stoi(splitLine[3]);
            int tracepointID = std::stoi(splitLine[0]);

            json tracepoint = metadata[tracepointID];
            // Check if the tracepoint ID is relevant for simulation
            if (tracepoint == nullptr)
                continue;

            static long long first_time = 0;
            if (first_time == 0)
                first_time = (NanoSeconds(stol(splitLine[4])) - Seconds(100)).GetNanoSeconds();
            next_time = NanoSeconds(stol(splitLine[4])-first_time);

            std::string tracepointName = tracepoint["name"];

            // We now create handlers for all the simulation events.
            // Each tracepoint will get tracepoint ID as key and a callback
            // function as value, and the trace file will schedule a call
            // to this function at the time that it actually happened in the
            // real-world execution.
            if (tracepointName == "receiveEvent") {
                // Schedule a CEP event to be produced
                int eventID = argument;
                json event_to_add;
                auto cepevents = j["cepevents"];
                for (auto cepevent : cepevents) {
                    if (cepevent["id"] == eventID) {
                        // Found event
                        event_to_add = cepevent;
                        break;
                    }
                }

                Ptr<CepEvent> e = CreateObject<CepEvent>();
                e->type = std::to_string((int)event_to_add["stream-id"]);
                e->event_class = ATOMIC_EVENT;
                e->delay = 0; // initializing delay
                static int cnt;
                e->m_seq = cnt;
                e->hopsCount = 0;
                e->prevHopsCount = 0;
                std::map<std::string, double> numberValues;
                std::map<std::string, std::string> stringValues;
                for (auto attribute : event_to_add["attributes"]) {
                    if (attribute["value"].type() == json::value_t::number_unsigned) {
                        e->numberValues[attribute["name"]] = attribute["value"];
                    } else if (attribute["value"].type() == json::value_t::string) {
                        e->stringValues[attribute["name"]] = attribute["value"];
                    } else {
                        NS_ABORT_MSG("CEP event has unsupported attribute type");
                    }
                }
                e->timestamp = Simulator::Now();
                e->pkt = Create<Packet>();  // Dummy packet for processing delay
                NS_LOG_INFO(Simulator::Now() << " CepEvent number " << e->m_seq << " timestamp: " << e->timestamp);
                // TODO: Find out how to create this event here and send it to GenerateAtomicCepEvents
                Simulator::Schedule (next_time, &CEPEngine::ProcessCepEvent, GetObject<CEPEngine>(), e);
                //Simulator::Schedule (next_time, &DataSource::GenerateAtomicCepEvents, ds, q);
            } else if (tracepointName == "addQuery") {
                // Schedule a complex query to be produced and placed
                int queryID = argument;
                json query_to_add;
                auto cepqueries = j["cepqueries"];
                for (auto cepquery : cepqueries) {
                    if (cepquery["id"] == queryID) {
                        // Found query
                        query_to_add = cepquery;
                        break;
                    }
                }

                Ptr<Query> q = CreateObject<Query>();  // q3 = complex event
                q->toBeProcessed = true;
                q->actionType = NOTIFICATION;
                static int query_cnt = 0;
                static int complex_event_cnt = 0;
                q->id = query_cnt++;
                q->isFinal = true;
                q->isAtomic = false;
                q->eventType = std::to_string(complex_event_cnt++);
                q->output_dest = Ipv4Address("10.0.0.3");
                std::string event1 = query_to_add["inevents"][0];
                std::string event2 = query_to_add["inevents"][1];
                q->inevent1 = event1;
                q->inevent2 = event2;
                q->window = Seconds(150000000000000000);
                q->isFinalWithinNode = true;

                Ptr<NumberConstraint> c = CreateObject<NumberConstraint>();
                c->var_name = "value";
                c->numberValue = 45;
                c->type = GTCONSTRAINT;

                q->constraints.emplace_back(c);
                q->op = "then";
                q->assigned = false;
                q->currentHost.Set("0.0.0.0");
                auto parent_output = event1 + "then" + event2;
                q->parent_output = parent_output;
                Simulator::Schedule(next_time, &Dcep::DispatchQuery, this, q);
                // TODO: Find out how to create this query here and deploy it
            } else if (tracepointName == "clearQueries") {
                // Schedule all queries to be removed
            } else {
                NS_ABORT_MSG("Unrecognized simulation event in metadata");
            }
        }
    }
}


void
SiddhiTRexThroughputDcep::ActivateDatasource(Ptr<Query> q)
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
            if (!experiment_metadata_fn.empty() && !trace_fn.empty())
                Simulator::Schedule(Seconds(generate_events_in), &DataSource::GenerateAtomicCepEvents, ds, q);
        }
    }
}

/*
 * #######################################################################
 * ####################### SINK ##########################################
 */

TypeId
SiddhiTRexThroughputSink::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::SiddhiTRexThroughputSink")
            .SetParent<Sink>()
    ;
    return tid;
}


static Ptr<Query> q1, q2;
static int q1orq2 = 0;


void
SiddhiTRexThroughputSink::BuildTRexQueries(Ptr<Dcep> dcep)
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
SiddhiTRexThroughputDataSource::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::SiddhiTRexThroughputDataSource")
            .SetParent<DataSource> ()
            .AddConstructor<SiddhiTRexThroughputDataSource> ()
    ;
    return tid;
}


void
SiddhiTRexThroughputDataSource::GenerateAtomicCepEvents(Ptr<Query> q) {
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