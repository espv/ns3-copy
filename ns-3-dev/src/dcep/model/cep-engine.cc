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

/*
 * Implementation of a CEP engine including models for:
 *  1. CepEvent and Query models
 *  2. Detector and Producer models to process CEP events
 *  3. A forwarder model to forward composite events produced by the CEP engine
 */

#include "cep-engine.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"
#include "ns3/log.h"
#include "ns3/config.h"
#include "placement.h"
#include "message-types.h"
#include "ns3/abort.h"
#include "placement.h"
#include "dcep.h"
#include "ns3/processing-module.h"
#include "ns3/event-impl.h"

#include <algorithm>


namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(CEPEngine);
NS_LOG_COMPONENT_DEFINE ("Detector");

/**************** CEP CORE *******************
 * *************************************************
 * ***************************************************/
/* ... */
    TypeId
    FogApplicationEngine::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::FogApplicationEngine")
                .SetParent<Object> ()
        ;

        return tid;
    }

    TypeId
    CEPEngine::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::CEPEngine")
        .SetParent<FogApplicationEngine> ()
        .AddConstructor<CEPEngine> ()
        .AddTraceSource ("CepEvent",
                       "Final event.",
                       MakeTraceSourceAccessor (&CEPEngine::nevent))
        
        ;
        
        return tid;
    }
    
    
    CEPEngine::CEPEngine()
    {
        Ptr<Forwarder> forwarder = CreateObject<Forwarder>();
        Ptr<Detector> detector = CreateObject<Detector>();
        Ptr<Producer> producer = CreateObject<Producer>();

        AggregateObject(forwarder);
        AggregateObject(detector);
        AggregateObject(producer);
        
        
    }
    
    void
    CEPEngine::Configure()
    {
        GetObject<Forwarder>()->TraceConnectWithoutContext("new event", 
                MakeCallback(&CEPEngine::ForwardProducedCepEvent, this));
    }

    std::set<Ptr<CepQueryComponent> > CEPEngine::GetCepQueryComponents(int stream_id)
    {
        return streamToQueryComponents[stream_id];
    }
    
    void
    CEPEngine::ForwardProducedCepEvent(Ptr<CepEvent> e)
    {
        GetObject<Placement>()->ForwardProducedCepEvent(e);
    }

    void
    CEPEngine::DoCheckStringConstraints(Ptr<CepEvent> e, std::map<std::string, Ptr<Constraint>> constraints, Ptr<CEPEngine> cep, Ptr<Producer> producer, std::map<std::string, string> values)
    {
        Ptr<Placement> p = GetObject<Placement>();
        auto dcep = GetObject<Dcep>();
        Ptr<ExecEnv> ee = dcep->GetNode()->GetObject<ExecEnv>();
        if (values.begin() == values.end()) {
            dcep->CheckedConstraints(e, ee->currentlyExecutingThread);
            ee->setLocalStateVariable("constraints-done", 1);
            return;
        }
        auto k = values.begin()->first;
        values.erase(values.begin());

        // Constraint type is 1 (string)
        ee->setLocalStateVariable("constraints-type", 1);

        ee->setLocalStateVariable("constraints-done", 0);
        if (constraints[k]) {
            ee->setLocalStateVariable("constraint-processed", 1);
        } else {
            ee->setLocalStateVariable("constraint-processed", 0);
        }
        ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
        ee->Proceed(1, ee->currentlyExecutingThread, "check-constraints", &CEPEngine::DoCheckStringConstraints, this, e, constraints, cep, producer, values);
    }

    void
    CEPEngine::DoCheckNumberConstraints(Ptr<CepEvent> e, std::map<std::string, Ptr<Constraint>> constraints, Ptr<CEPEngine> cep, Ptr<Producer> producer, std::map<std::string, double> values, std::map<std::string, string> stringValues)
    {
        Ptr<Placement> p = GetObject<Placement>();
        Ptr<ExecEnv> ee = GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();
        if (values.begin() == values.end()) {
            ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
            ee->Proceed(1, ee->currentlyExecutingThread, "check-constraints", &CEPEngine::DoCheckStringConstraints, this, e, constraints, cep, producer, stringValues);
            return;
        }
        auto k = values.begin()->first;
        values.erase(values.begin());

        // Constraint type is 0 (number)
        ee->setLocalStateVariable("constraints-type", 0);
        ee->setLocalStateVariable("constraints-done", 0);
        if (constraints[k]) {
            ee->setLocalStateVariable("constraint-processed", 1);
        } else {
            ee->setLocalStateVariable("constraint-processed", 0);
        }
        ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
        ee->Proceed(1, ee->currentlyExecutingThread, "check-constraints", &CEPEngine::DoCheckNumberConstraints, this, e, constraints, cep, producer, values, stringValues);
    }

    void
    CEPEngine::CheckConstraints(Ptr<CepEvent> e)
    {
        auto cep = GetObject<CEPEngine>();

        std::vector<Ptr<CepOperator>> ops;
        cep->GetOpsByInputCepEventType(e->type, ops);
        std::map<std::string, Ptr<Constraint> > constraints;
        /*for (auto cepop : ops)
        {
            for (auto c : cepop->constraints)
            {
                constraints[c->var_name] = c;
            }
        }*/
        Ptr<Producer> producer = GetObject<Producer>();

        auto node = GetObject<Dcep>()->GetNode();
        auto ee = node->GetObject<ExecEnv>();

        std::map<std::string, double> numberValues;
        for (auto const& x : e->numberValues)
        {
            numberValues[x.first] = x.second;
        }
        std::map<std::string, std::string> stringValues;
        for (auto const& x : e->stringValues)
        {
            stringValues[x.first] = x.second;
        }
        DoCheckNumberConstraints(e, constraints, cep, producer, numberValues, stringValues);
    }

    void CEPEngine::FinishedProcessingEvent(Ptr<CepEvent> e)
    {
        //NS_LOG_INFO(Simulator::Now() << " Time to process event " << e->m_seq << ": " << (Simulator::Now() - e->pkt->m_executionInfo->timestamps[0]).GetMicroSeconds());
        auto dcep = GetObject<Dcep>();
        Ptr<ExecEnv> ee = dcep->GetNode()->GetObject<ExecEnv>();
        //NS_LOG_INFO(Simulator::Now() << " Time to process event " << e->m_seq << ": " << (Simulator::Now() - ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->timestamps[0]).GetMicroSeconds() << " µs");
        ee->currentlyExecutingThread->m_currentLocation->m_executionInfo = Create<ExecutionInfo>();
        dcep->FinishedProcessingCepEvent(e, ee->currentlyExecutingThread);
    }

    void
    CEPEngine::PacketThreadRecvPacket(Ptr<CepEvent> e)
    {
        auto dcep = GetObject<Dcep>();
        Ptr<ExecEnv> ee = dcep->GetNode()->GetObject<ExecEnv>();
        dcep->RxCepEvent(e, ee->currentlyExecutingThread);
    }

    void
    CEPEngine::ProcessCepEvent(Ptr<CepEvent> e)
    {
        auto dcep = GetObject<Dcep>();
        Ptr<ExecEnv> ee = dcep->GetNode()->GetObject<ExecEnv>();
        dcep->RxCepEvent(e, ee->currentlyExecutingThread);
        auto node = GetObject<Dcep>()->GetNode();

        // I think it's an error to just set the curCepEvent for the entire thread to be e, since a function that
        // was interrupted by HIRQ-1 might be using another curCepEvent. This leads to a race condition.
        ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->curCepEvent = e;
        ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->timestamps.emplace_back(Simulator::Now());
        e->timestamp = Simulator::Now();
        ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
        ee->Proceed(1, ee->currentlyExecutingThread, "check-constraints", &CEPEngine::CheckConstraints, this, e);
        ee->ScheduleInterrupt(e->pkt, "HIRQ-1", Seconds(0));
    }
    
    
    void
    CEPEngine::GetOpsByInputCepEventType(std::string eventType, std::vector<Ptr<CepOperator>>& ops)
    {
        for(auto op : ops_queue)
        {
            //Ptr<CepEventPattern> ep = q->GetObject<CepEventPattern>();
            //Ptr<CepEventPattern> ep = q->ep;
            if(op->ExpectingCepEvent(eventType))
            {
                //NS_LOG_INFO("found query expecting event type " << eventType);
                ops.push_back(op);
            }
        }
    }
    
    Ptr<Query> 
    CEPEngine::GetQuery(uint32_t id)
    {
        for(auto q : queryPool) {

            if(q->id == id)
            {
                return q;
            }
        }
        
        return NULL;
    }

    Ptr<CepOperator> CEPEngine::GetOperator(uint32_t queryId)
    {
        for (auto op : ops_queue) {
            if (op->queryId == queryId)
            {
                return op;
            }
        }
    }
    
    void
    CEPEngine::RecvQuery(Ptr<Query> q)
    {
        InstantiateQuery(q);
        StoreQuery(q);
    }

    void
    CEPEngine::RecvQueryComponent(Ptr<CepQueryComponent> queryComponent)
    {
        InstantiateQueryComponent(queryComponent);
        StoreQueryComponent(queryComponent);
    }
    
    void
    CEPEngine::InstantiateQuery(Ptr<Query> q){
        Ptr<CepOperator> cepOp;
        if(q->isAtomic)
        {
            /* instantiate atomic event */
            cepOp = CreateObject<AtomicOperator>();
            cepOp->Configure(q, this);
            GetObject<Dcep>()->ActivateDatasource(q);
        }
        else
        {
            if(q->op == "and")
            {
                cepOp = CreateObject<AndOperator>();
            }
            else if(q->op == "or")
            {
                cepOp = CreateObject<OrOperator>();
            }
            else if (q->op == "then")
            {
                cepOp = CreateObject<ThenOperator>();
            }
            else
            {
                NS_ABORT_MSG ("UNKNOWN OPERATOR");
            }
            cepOp->Configure(q, this);
        }

        if (q->toBeProcessed) {
            GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>()->cepQueryQueues["all-cepops"]->push(cepOp);
            this->ops_queue.push_back(cepOp);
        }
    }

    void
    CEPEngine::InstantiateQueryComponent(Ptr<CepQueryComponent> queryComponent)
    {
      GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>()->cepQueryComponentQueues["all-queries"]->push(queryComponent);
    }

    void
    CEPEngine::ClearQueries() {
        auto dcep = GetObject<Dcep>();
        dcep->ClearQueries();
        auto processing_ops_queue = dcep->GetNode()->GetObject<ExecEnv>()->cepQueryQueues["all-cepops"];
        while(!processing_ops_queue->empty())
            processing_ops_queue->pop();
        this->ops_queue.clear();
    }

    void
    CEPEngine::ClearQueryComponents() {
        auto dcep = GetObject<Dcep>();
        dcep->ClearQueries();
        auto processing_ops_queue2 = dcep->GetNode()->GetObject<ExecEnv>()->cepQueryComponentQueues["all-queries"];
        while(!processing_ops_queue2->empty())
            processing_ops_queue2->pop();
        queryComponentPool.clear();
        streamToQueryComponents.clear();
        this->ops_queue.clear();
    }

    void
    CEPEngine::StoreQuery(Ptr<Query> q){
        queryPool.push_back(q);
    }

    void
    CEPEngine::StoreQueryComponent(Ptr<CepQueryComponent> queryComponent)
    {
        queryComponentPool.push_back(queryComponent);
        /* For each event stream that's in the queryComponent locally, place the queryComponent in the vector in
         * streamToQueryComponents that corresponds to the stream */
        auto cepop = queryComponent->GetFirstOperator();
        while (cepop != nullptr) {
            if (typeid(*cepop) == typeid(JoinOperator)) {
                Ptr<JoinOperator> joinOp = dynamic_cast<JoinOperator* >(&(*cepop));
                for (auto atom : joinOp->GetAtomicOperators()) {
                    streamToQueryComponents[atom->stream_id].insert(queryComponent);
                }
            } else if (typeid(*cepop) == typeid(AtomicOperator)) {
                Ptr<AtomicOperator> atom = dynamic_cast<AtomicOperator* >(&(*cepop));
                streamToQueryComponents[atom->stream_id].insert(queryComponent);
            } else if (typeid(*cepop) == typeid(ThenOperator)) {

            } else {
                // Unknown Type
                NS_ABORT_MSG("Unknown operator");
            }
            cepop = cepop->nextOperator;
        }
    }


    /**************** DETECTOR ****************
     ***********************************************
     **********************************************/
    
    TypeId
    Detector::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::Detector")
        .SetParent<Object> ()
        .AddConstructor<Detector> ()
        ;
        
        return tid;
    }


    void
    Detector::CepOperatorProcessCepEvent(Ptr<CepEvent> e, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep, Ptr<Producer> producer)
    {
        Ptr<ExecEnv> ee = GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();
        cep = GetObject<CEPEngine>();
        producer = GetObject<Producer>();
        ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->curCepEvent = e;
        e->pkt = ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->packet;
        auto op = ee->currentlyExecutingThread->m_currentLocation->curCepQuery;

        ee->setLocalStateVariable("CepOpDoneYet", 0);
        ee->setLocalStateVariable("CreatedComplexEvent", 0);
        if (!op->ExpectingCepEvent(e->type)) {
            ee->setLocalStateVariable("CepOpDoneYet", 1);
            return;
        }

        std::vector<Ptr<CepEvent> > returned;

        auto evs = ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executionVariables.find("DCEP-Sim");
        if (evs == ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executionVariables.end()) {
            ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executionVariables["DCEP-Sim"] = new DcepSimExecutionVariables();
            evs = ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executionVariables.find("DCEP-Sim");
        }
        auto devs = (DcepSimExecutionVariables *)evs->second;
        devs->cepOperatorProcessCepEvent_cep = cep;
        devs->cepOperatorProcessCepEvent_ops = ops;
        devs->cepOperatorProcessCepEvent_producer = producer;

        op->Evaluate(e, returned, cep->GetQuery(op->queryId), producer, ops, cep);
    }


    void
    Detector::CepQueryComponentProcessCepEvent(Ptr<CepEvent> e, Ptr<CEPEngine> cep)
    {
        Ptr<ExecEnv> ee = GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();
        cep = GetObject<CEPEngine>();
        Ptr<Producer> producer = GetObject<Producer>();
        ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->curCepEvent = e;
        e->pkt = ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->packet;
        auto op = ee->currentlyExecutingThread->m_currentLocation->curCepQuery;

        ee->setLocalStateVariable("CepOpDoneYet", 0);
        ee->setLocalStateVariable("CreatedComplexEvent", 0);
        /*if (!op->ExpectingCepEvent(e->type)) {
            ee->setLocalStateVariable("CepOpDoneYet", 1);
            return;
        }*/

        auto evs = ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executionVariables.find("DCEP-Sim");
        if (evs == ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executionVariables.end()) {
            ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executionVariables["DCEP-Sim"] = new DcepSimExecutionVariables();
            evs = ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executionVariables.find("DCEP-Sim");
        }
        auto devs = (DcepSimExecutionVariables *)evs->second;
        devs->cepOperatorProcessCepEvent_cep = cep;
        //devs->cepOperatorProcessCepEvent_ops = ops;
        devs->cepOperatorProcessCepEvent_cepQueryComponents = cep->GetCepQueryComponents(e->GetStreamId());
        devs->cepOperatorProcessCepEvent_producer = producer;

        auto cqc = ee->currentlyExecutingThread->m_currentLocation->curCepQueryComponent;
        std::vector<Ptr<CepEvent> > returned;
        if (cqc->GetThenOperators().empty()) {
            // A single AtomicOperator or JoinOperator
            auto cepop = cqc->GetFirstOperator();
            cepop->Evaluate2(e, returned, cep);
            if (!returned.empty()) {
                cep->GetObject<Producer>()->HandleNewCepEvent2(cqc, returned);
            }
        } else {
            for (auto thenop : cqc->GetThenOperators()) {
                thenop->Evaluate2(e, returned, cep);
                if (thenop->nextOperator->nextOperator == nullptr && !returned.empty()) {
                    cep->GetObject<Producer>()->HandleNewCepEvent2(cqc, returned);
                }
            }
        }
    }

    
    void
    Detector::ProcessCepEvent(Ptr<CepEvent> e)
    {
        auto cep = GetObject<CEPEngine>();

        std::vector<Ptr<CepOperator>> ops;
        cep->GetOpsByInputCepEventType(e->type, ops);
        Ptr<Producer> producer = GetObject<Producer>();

        auto node = GetObject<Dcep>()->GetNode();
        auto ee = node->GetObject<ExecEnv>();

        CepOperatorProcessCepEvent(e, ops, cep, producer);
    }

    bool
    Constraint::Evaluate(Ptr<CepEvent> e)
    {
        switch (this->type) {
            case NUMBEREQCONSTRAINT:
                return e->numberValues[var_name] == numberValue;
            case NUMBERINEQCONSTRAINT:
                return e->numberValues[var_name] != numberValue;
            case NUMBERLTCONSTRAINT:
                return e->numberValues[var_name] < numberValue;
            case NUMBERLTECONSTRAINT:
                return e->numberValues[var_name] <= numberValue;
            case NUMBERGTCONSTRAINT:
                return e->numberValues[var_name] > numberValue;
            case NUMBERGTECONSTRAINT:
                return e->numberValues[var_name] >= numberValue;
            case AVGNUMBEREQCONSTRAINT:
                return e->window->GetAvgNumberValue(var_name) == numberValue;
                //return e->numberValues[var_name] == numberValue;
            case AVGNUMBERINEQCONSTRAINT:
                return e->window->GetAvgNumberValue(var_name) != numberValue;
                //return e->numberValues[var_name] != numberValue;
            case AVGNUMBERLTCONSTRAINT:
                return e->window->GetAvgNumberValue(var_name) < numberValue;
                //return e->numberValues[var_name] < numberValue;
            case AVGNUMBERLTECONSTRAINT:
                return e->window->GetAvgNumberValue(var_name) <= numberValue;
                //return e->numberValues[var_name] <= numberValue;
            case AVGNUMBERGTCONSTRAINT:
                return e->window->GetAvgNumberValue(var_name) > numberValue;
                //return e->numberValues[var_name] > numberValue;
            case AVGNUMBERGTECONSTRAINT:
                return e->window->GetAvgNumberValue(var_name) >= numberValue;
                //return e->numberValues[var_name] >= numberValue;
            case STRINGEQCONSTRAINT:
                return e->stringValues[var_name] == stringValue;
            case STRINGINEQCONSTRAINT:
                return e->stringValues[var_name] != stringValue;
            default:
                NS_FATAL_ERROR("Selected invalid constraint type for number constraint");
        }
    }

    bool
    Constraint::Evaluate(Ptr<CepEvent> e1, Ptr<CepEvent> e2)
    {
        switch (type) {
            case JOINNUMBEREQCONSTRAINT:
                return e1->numberValues[var_name] == e2->numberValues[var_name];
            case JOINNUMBERINEQCONSTRAINT:
                return e1->numberValues[var_name] != e2->numberValues[var_name];
            case JOINNUMBERLTCONSTRAINT:
                return e1->numberValues[var_name] < e2->numberValues[var_name];
            case JOINNUMBERLTECONSTRAINT:
                return e1->numberValues[var_name] <= e2->numberValues[var_name];
            case JOINNUMBERGTCONSTRAINT:
                return e1->numberValues[var_name] > e2->numberValues[var_name];
            case JOINNUMBERGTECONSTRAINT:
                return e1->numberValues[var_name] >= e2->numberValues[var_name];
            case JOINSTRINGEQCONSTRAINT:
                return e1->stringValues[var_name] == e2->stringValues[var_name];
            case JOINSTRINGINEQCONSTRAINT:
                return e1->stringValues[var_name] != e2->stringValues[var_name];
            default:
                NS_FATAL_ERROR("Selected invalid constraint type for number constraint");
        }
    }

    void
    Constraint::SetType(json constraint)
    {
        type = INVALIDCONSTRAINT;
        std::string type_str = constraint["type"];
        std::string value_type = constraint["value-type"];
        bool is_atomic = constraint.contains("value");
        bool is_join = constraint.contains("reference1") && constraint.contains("reference2");
        bool is_aggregation = constraint.contains("aggregation");
        NS_ASSERT_MSG(value_type == "number" || value_type == "string",
                'The "value-type" field must either be "number" or "string"');
        NS_ASSERT_MSG(is_atomic || is_join,
                'Either a "value" or a "reference" field must be defined for the constraint');
        if (is_atomic) {
            if (value_type == "number") {
                if (is_aggregation) {
                    if (constraint["aggregation"] == "average") {
                        if (type_str == "EQCONSTRAINT") {
                            type = AVGNUMBEREQCONSTRAINT;
                        } else if (type_str == "INEQCONSTRAINT") {
                            type = AVGNUMBERINEQCONSTRAINT;
                        } else if (type_str == "LTCONSTRAINT") {
                            type = AVGNUMBERLTCONSTRAINT;
                        } else if (type_str == "LTECONSTRAINT") {
                            type = AVGNUMBERLTECONSTRAINT;
                        } else if (type_str == "GTCONSTRAINT") {
                            type = AVGNUMBERGTCONSTRAINT;
                        } else if (type_str == "GTECONSTRAINT") {
                            type = AVGNUMBERGTECONSTRAINT;
                        }
                    }
                } else {
                    if (type_str == "EQCONSTRAINT") {
                        type = NUMBEREQCONSTRAINT;
                    } else if (type_str == "INEQCONSTRAINT") {
                        type = NUMBERINEQCONSTRAINT;
                    } else if (type_str == "LTCONSTRAINT") {
                        type = NUMBERLTCONSTRAINT;
                    } else if (type_str == "LTECONSTRAINT") {
                        type = NUMBERLTECONSTRAINT;
                    } else if (type_str == "GTCONSTRAINT") {
                        type = NUMBERGTCONSTRAINT;
                    } else if (type_str == "GTECONSTRAINT") {
                        type = NUMBERGTECONSTRAINT;
                    }
                }
            } else if (value_type == "string") {
                if (type_str == "EQCONSTRAINT") {
                    type = STRINGEQCONSTRAINT;
                } else if (type_str == "INEQCONSTRAINT") {
                    type = STRINGINEQCONSTRAINT;
                }
            }
        } else if (is_join) {  // JoinConstraint
            if (value_type == "int") {
                if (type_str == "EQCONSTRAINT") {
                    type = JOINNUMBEREQCONSTRAINT;
                } else if (type_str == "INEQCONSTRAINT") {
                    type = JOINNUMBERINEQCONSTRAINT;
                } else if (type_str == "LTCONSTRAINT") {
                    type = JOINNUMBERLTCONSTRAINT;
                } else if (type_str == "LTECONSTRAINT") {
                    type = JOINNUMBERLTECONSTRAINT;
                } else if (type_str == "GTCONSTRAINT") {
                    type = JOINNUMBERGTCONSTRAINT;
                } else if (type_str == "GTECONSTRAINT") {
                    type = JOINNUMBERGTECONSTRAINT;
                }
            } else if (value_type == "string") {
                if (type_str == "EQCONSTRAINT") {
                    type = JOINSTRINGEQCONSTRAINT;
                } else if (type_str == "INEQCONSTRAINT") {
                    type = JOINSTRINGINEQCONSTRAINT;
                }
            }
        }

        if (type == INVALIDCONSTRAINT) {
            // Failed to parse json for the constraint type
            NS_ABORT_MSG("Invalid constraint type selected.\n\"type\": " << type_str << "\n\"value-type\": " <<
                         value_type << "\nIs atomic constraint: " << (is_atomic ? "yes" : "no") <<
                         "\nIs join constraint: " << (is_join ? "yes" : "no"));
        }
    }

    TypeId
    FogApplicationComponent::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::FogApplicationComponent")
                .SetParent<Object> ()
        ;

        return tid;
    }

    TypeId
    CepQueryComponent::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::CepQueryComponent")
                .SetParent<FogApplicationComponent> ()
                .AddConstructor<CepQueryComponent> ()
        ;

        return tid;
    }

    CepQueryComponent::CepQueryComponent()
    {
        static int cepQueryComponentCounter = 0;
        id = cepQueryComponentCounter++;
    }

    int
    CepQueryComponent::GetId()
    {
        return id;
    }

    void
    CepQueryComponent::InsertThenOperator(Ptr<ThenOperator> cepop)
    {
        thenOperators.emplace_back(cepop);
    }

    void
    CepQueryComponent::SetFirstOperator(Ptr<CepOperator> cepop)
    {
        firstOperator = cepop;
    }

    void
    CepQueryComponent::SetLastOperator(Ptr<CepOperator> cepop)
    {
        lastOperator = cepop;
    }

    Ptr<CepOperator>
    CepQueryComponent::GetFirstOperator()
    {
        return firstOperator;
    }

    Ptr<CepOperator>
    CepQueryComponent::GetLastOperator()
    {
        return lastOperator;
    }

    std::vector<Ptr<ThenOperator> >
    CepQueryComponent::GetThenOperators()
    {
        return thenOperators;
    }

    std::string CepQueryComponent::GetEventType()
    {
        return eventType;
    }

    void CepQueryComponent::SetEventType(std::string et)
    {
        eventType = et;
    }

    TypeId
    CepOperator::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::CepOperator")
            .SetParent<Object> ()
        ;
        
        return tid;
    }

    TypeId
    AtomicOperator::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::AtomicOperator")
                .SetParent<CepOperator> ()
        ;

        return tid;
    }

    TypeId
    JoinOperator::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::JoinOperator")
                .SetParent<CepOperator> ()
        ;

        return tid;
    }
    
    TypeId
    AndOperator::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::AndOperator")
            .SetParent<JoinOperator> ()
        ;
        
        return tid;
    }

    TypeId
    ThenOperator::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::ThenOperator")
                .SetParent<JoinOperator> ()
        ;

        return tid;
    }
    
    TypeId
    OrOperator::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::OrOperator")
            .SetParent<CepOperator> ()
        ;
        
        return tid;
    }

    void
    AtomicOperator::Configure(Ptr<Query> q, Ptr<CEPEngine> cep)
    {
        this->queryId = q->id;
        this->event1 = q->inevent1;
        this->event2 = q->inevent2;
        this->constraints = q->constraints;

        cepEngine = cep;
    }

    void
    JoinOperator::Configure(Ptr<Query> q, Ptr<CEPEngine> cep)
    {
        this->queryId = q->id;
        this->event1 = q->inevent1;
        this->event2 = q->inevent2;
        this->constraints = q->constraints;

        cepEngine = cep;
    }
    
    void
    AndOperator::Configure(Ptr<Query> q, Ptr<CEPEngine> cep)
    {
        this->queryId = q->id;
        this->event1 = q->inevent1;
        this->event2 = q->inevent2;
        this->constraints = q->constraints;
        if (q->prevQuery != nullptr)
            this->prevOperator = cep->GetOperator(q->prevQuery->id);
            
        Ptr<BufferManager> bufman = CreateObject<BufferManager>();
        
        bufman->consumption_policy = SELECTED_CONSUMPTION; //default
        bufman->selection_policy = SINGLE_SELECTION; //default
        bufman->Configure(this);
        this->bufman = bufman;
        cepEngine = cep;
    }

    void
    ThenOperator::Configure(Ptr<Query> q, Ptr<CEPEngine> cep)
    {
        this->queryId = q->id;
        this->event1 = q->inevent1;
        this->event2 = q->inevent2;
        this->constraints = q->constraints;
        if (q->prevQuery != nullptr)
            this->prevOperator = cep->GetOperator(q->prevQuery->id);

        Ptr<BufferManager> bufman = CreateObject<BufferManager>();

        bufman->consumption_policy = SELECTED_CONSUMPTION; //default
        bufman->selection_policy = SINGLE_SELECTION; //default
        bufman->Configure(this);
        this->bufman = bufman;
        cepEngine = cep;
    }
    
    void
    OrOperator::Configure(Ptr<Query> q, Ptr<CEPEngine> cep)
    {
        this->queryId = q->id;
        this->event1 = q->inevent1;
        this->event2 = q->inevent2;
        this->constraints = q->constraints;
        if (q->prevQuery != nullptr)
            this->prevOperator = cep->GetOperator(q->prevQuery->id);

        Ptr<BufferManager> bufman = CreateObject<BufferManager>();
        
        bufman->consumption_policy = SELECTED_CONSUMPTION; //default
        bufman->selection_policy = SINGLE_SELECTION; //default
        bufman->Configure(this);
        this->bufman = bufman;
        cepEngine = cep;
    }

    std::vector<Ptr<AtomicOperator> > JoinOperator::GetAtomicOperators()
    {
        return atomicOperators;
    }

    void
    CepOperator::InsertCepEventIntoWindows(Ptr<CepEvent> e)
    {
        for (auto window : stream_windows[e->GetStreamId()]) {
            e->window = window;
            Ptr<CepEvent> copy = CreateObject<CepEvent>(e);
            window->InsertEvent(copy);
        }
    }

    void
    CepOperator::RemoveCepEventFromWindows(Ptr<CepEvent> e)
    {
        for (auto window : stream_windows[e->GetStreamId()]) {
            window->RemoveEvent(e);
        }
    }

    bool
    AtomicOperator::Evaluate (Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >& returned, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep)
    {
        bool constraintsFulfilled = true;
        for (auto c : constraints)
        {
            // All constraints must be fulfilled for constraintsFulfilled to be true
            constraintsFulfilled = c->Evaluate(e) && constraintsFulfilled;
        }

        Ptr<Node> node = cepEngine->GetObject<Dcep>()->GetNode();
        auto ee = node->GetObject<ExecEnv>();
        ee->setLocalStateVariable("CepOpType", 0);
        ee->setLocalStateVariable("CreatedComplexEvent", 0);
        ee->setLocalStateVariable("CepOpDoneYet", 1);
        ee->setLocalStateVariable("attributes-left", 0);
        if (!constraintsFulfilled) {
            return false;
        }

        p->GetObject<Forwarder>()->ForwardNewCepEvent(e);
        return true;
    }

    std::vector<Ptr<CepEvent> >
    AtomicOperator::Evaluate2 (Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >& returned, Ptr<CEPEngine> cep)
    {
        if (stream_windows[e->GetStreamId()].empty()) {
            return std::vector<Ptr<CepEvent> > ();
        }
        this->InsertCepEventIntoWindows(e);
        bool constraintsFulfilled = true;
        for (auto c : constraints)
        {
            // All constraints must be fulfilled for constraintsFulfilled to be true
            constraintsFulfilled = c->Evaluate(e) && constraintsFulfilled;
        }

        Ptr<Node> node = cep->GetObject<Dcep>()->GetNode();
        auto ee = node->GetObject<ExecEnv> ();
        ee->setLocalStateVariable("CepOpType", 0);
        ee->setLocalStateVariable("CreatedComplexEvent", 0);
        ee->setLocalStateVariable("CepOpDoneYet", 1);
        ee->setLocalStateVariable("attributes-left", 0);
        if (!constraintsFulfilled) {
            this->RemoveCepEventFromWindows(e);
            return std::vector<Ptr<CepEvent> >();
        }

        returned.emplace_back(e);
        //cep->GetObject<Forwarder>()->ForwardNewCepEvent(e);
        return std::vector<Ptr<CepEvent> > ();
    }

    void
    AtomicOperator::GetPartialResults(std::vector<Ptr<CepEvent> > &ret)
    {
        for (auto q : stream_windows) {
            for (auto p : q.second) {
                for (auto e : p->GetCepEvents()) {
                    ret.emplace_back(e.second);
                }
            }
        }
    }

    // TODO: finish implementing this method
    bool
    JoinOperator::Evaluate (Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >& returned, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep)
    {
        bool constraintsFulfilled = true;
        for (auto c : constraints)
        {
            // All constraints must be fulfilled for constraintsFulfilled to be true
            constraintsFulfilled = c->Evaluate(e) && constraintsFulfilled;
        }

        Ptr<Node> node = cep->GetObject<Dcep>()->GetNode();
        auto ee = node->GetObject<ExecEnv>();
        ee->setLocalStateVariable("CepOpType", 4);
        ee->setLocalStateVariable("CreatedComplexEvent", 0);
        ee->setLocalStateVariable("CepOpDoneYet", 1);
        ee->setLocalStateVariable("attributes-left", 0);
        if (!constraintsFulfilled) {
            return false;
        }

        p->GetObject<Forwarder>()->ForwardNewCepEvent(e);
        return true;
    }

    std::vector<Ptr<CepEvent> >
    JoinOperator::Evaluate2 (Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >& returned, Ptr<CEPEngine> cep)
    {
        for (auto atomicOperator : atomicOperators) {
            atomicOperator->InsertCepEventIntoWindows(e);
        }
        bool constraintsFulfilled = true;

        for (auto atomic : atomicOperators) {
            if (atomic->stream_id == e->GetStreamId()) {
                for (auto c : atomic->constraints) {
                    // All constraints must be fulfilled for constraintsFulfilled to be true
                    constraintsFulfilled = c->Evaluate(e) && constraintsFulfilled;
                }
                // Only one AtomicOperator for each stream
                break;
            }
        }

        if (!constraintsFulfilled) {
            for (auto atomicOperator : atomicOperators) {
                atomicOperator->RemoveCepEventFromWindows(e);
            }
            return std::vector<Ptr<CepEvent> >();
        }

        // Atomic constraints are fulfilled; now we insert the event into windows

        // For each tuple, we need to check if all constraints are fulfilled,
        // and we potentially have to compare the tuple with every other tuple in all windows.
        // Now we check if the event joins with other events. If all constraints are fulfilled
        // TODO: Finish this; it's not trivial because we have to potentially join many tuples
        // The algorithm to check this out
        std::vector<std::vector<Ptr<CepEvent> > > cartesian;
        std::vector<std::vector<Ptr<CepEvent> > > toBeAddedToCartesian;
        for (auto atomicOperator : atomicOperators) {
            for (auto & stream_window : atomicOperator->stream_windows) {
                for (auto w : stream_window.second) {
                    for (auto p : w->GetCepEvents()) {
                        auto event = p.second;
                        std::vector<Ptr<CepEvent>> new_events;
                        new_events.emplace_back(event);
                        cartesian.emplace_back(new_events);
                        for (std::vector<Ptr<CepEvent> > event_vector : cartesian) {
                            bool already_in_vector = false;
                            for (auto e2 : event_vector) {
                                if (e2->GetStreamId() == event->GetStreamId()) {
                                    already_in_vector = true;
                                }
                            }
                            if (!already_in_vector) {
                                std::vector<Ptr<CepEvent> > add_event;
                                add_event.insert(add_event.end(), event_vector.begin(), event_vector.end());
                                add_event.emplace_back(event);
                                toBeAddedToCartesian.emplace_back(add_event);
                            }
                        }
                        cartesian.insert(cartesian.end(), toBeAddedToCartesian.begin(), toBeAddedToCartesian.end());
                        toBeAddedToCartesian.clear();
                    }
                }
            }
        }

        for (auto event_vector : cartesian) {
            bool containsNewEvent = false;
            for (auto event : event_vector) {
                if (event->m_seq == e->m_seq && event->GetStreamId() == e->GetStreamId()) {
                    containsNewEvent = true;
                }
            }
            if (!containsNewEvent)
                continue;
            bool constraints_fulfilled = true;
            for (auto c : constraints) {
                Ptr<CepEvent> first_event = nullptr;
                Ptr<CepEvent> second_event = nullptr;
                for (auto event : event_vector) {
                    if (c->event_stream1 == event->GetStreamId()) {
                        first_event = event;
                    } else if (c->event_stream2 == event->GetStreamId()) {
                        second_event = event;
                    }
                }

                if (first_event != nullptr && second_event != nullptr) {
                    constraints_fulfilled = c->Evaluate(first_event, second_event) && constraints_fulfilled;
                } else {
                    constraints_fulfilled = false;
                }
            }

            //static int cnt = 0;
            if (constraints_fulfilled) {
                // Yield output
                Ptr<CepEvent> complex_event = CreateObject<CepEvent>(event_vector);
                returned.emplace_back(complex_event);
            }
        }

        return std::vector<Ptr<CepEvent> > ();
    }

    void
    JoinOperator::InsertAtomicOperator(Ptr<AtomicOperator> a)
    {
        atomicOperators.emplace_back(a);
    }

    void
    JoinOperator::GetPartialResults(std::vector<Ptr<CepEvent> > &ret)
    {

    }

    bool
    AndOperator::DoEvaluate(Ptr<CepEvent> newEvent2, std::vector<Ptr<CepEvent> >& returned, std::vector<Ptr<CepEvent>> *events1, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep) {
        Ptr<Node> node = cepEngine->GetObject<Dcep>()->GetNode();
        auto ee = node->GetObject<ExecEnv>();
        if (events1->empty()) {
            // No sequences left
            ee->setLocalStateVariable("CepOpDoneYet", 1);
            ee->setLocalStateVariable("InsertedSequence", 0);
            delete events1;
            return false;
        }

        Ptr<CepEvent> curEvent1 = *events1->begin();
        events1->erase(events1->begin());
        // Create a complex event from each atomic event number 1.

        // If curEvent1 came before or after newEvent2 less than the window size, we trigger the rule
        if(Abs(curEvent1->timestamp - newEvent2->timestamp) < q->window) {
            // Here we insert the incoming event into the sequence
            // Split loop into recursion.
            // Return a recursive call to some function

            returned.push_back(curEvent1);
            returned.push_back(newEvent2);
            Consume(returned);

            p->HandleNewCepEvent(q, returned, this);
            ee->setLocalStateVariable("CepOpDoneYet", 0);
            ee->setLocalStateVariable("InsertedSequence", 1);
        } else {
            ee->setLocalStateVariable("CepOpDoneYet", 0);
            ee->setLocalStateVariable("InsertedSequence", 0);
        }

        ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
        ee->Proceed(1, ee->currentlyExecutingThread, "handle-and-cepop", &AndOperator::DoEvaluate, this, newEvent2, returned, events1, q, p, ops, cep);
    }
    
    bool
    AndOperator::Evaluate(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >& returned, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep)
    {
        auto events1 = new std::vector<Ptr<CepEvent>>();
        auto events2 = new std::vector<Ptr<CepEvent>>();
        bufman->put_event(e, this);  //wait for event with corresponding sequence number
        bufman->read_events(*events1, *events2);

        auto ee = cepEngine->GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();

        ee->setLocalStateVariable("CepOpType", 1);

        if (!events1->empty() && !events2->empty())
        {
            Ptr<Node> node = cepEngine->GetObject<Dcep>()->GetNode();
            auto ee = node->GetObject<ExecEnv>();
            ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
            if (e->type == events1->front()->type)
            {
                delete events1;  // Not going to use events1
                ee->Proceed(1, ee->currentlyExecutingThread, "handle-and-cepop", &AndOperator::DoEvaluate, this, e, returned, events2, q, p, ops, cep);
            }
            else
            {
                delete events2;  // Not going to use events2
                ee->Proceed(1, ee->currentlyExecutingThread, "handle-and-cepop", &AndOperator::DoEvaluate, this, e, returned, events1, q, p, ops, cep);
            }
            
        } else {
            // No sequences left
            ee->setLocalStateVariable("CepOpDoneYet", 1);
            ee->setLocalStateVariable("InsertedSequence", 1);
        }
        return false;
    }

    std::vector<Ptr<CepEvent> >
    AndOperator::Evaluate2 (Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >& returned, Ptr<CEPEngine> cep)
    {
      return std::vector<Ptr<CepEvent> > ();
    }

    void
    AndOperator::GetPartialResults(std::vector<Ptr<CepEvent> > &ret)
    {

    }

    bool
    ThenOperator::DoEvaluate(Ptr<CepEvent> newEvent2, std::vector<Ptr<CepEvent> >& returned, std::vector<Ptr<CepEvent>> *events1, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep) {
        Ptr<Node> node = cepEngine->GetObject<Dcep>()->GetNode();
        auto ee = node->GetObject<ExecEnv>();
        if (events1->empty()) {
            // No sequences left
            ee->setLocalStateVariable("CepOpDoneYet", 1);
            ee->setLocalStateVariable("InsertedSequence", 0);
            delete events1;
            return false;
        }

        Ptr<CepEvent> curEvent1 = *events1->begin();
        events1->erase(events1->begin());
        // Create a complex event from each atomic event number 1.

        if(curEvent1->timestamp + q->window > newEvent2->timestamp) {
            // Here we insert the incoming event into the sequence
            // Split loop into recursion.
            // Return a recursive call to some function

            returned.push_back(curEvent1);
            returned.push_back(newEvent2);

            p->HandleNewCepEvent(q, returned, this);
            ee->setLocalStateVariable("InsertedSequence", 1);
            ee->setLocalStateVariable("CreatedComplexEvent", 1);
            ee->setLocalStateVariable("CepOpDoneYet", 1);
        } else {
            ee->setLocalStateVariable("InsertedSequence", 0);
            ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
            ee->Proceed(1, ee->currentlyExecutingThread, "handle-then-cepop", &ThenOperator::DoEvaluate, this, newEvent2, returned, events1, q, p, ops, cep);
        }
    }

    bool
    ThenOperator::Evaluate(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >& returned, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep)
    {
        auto dcep = cepEngine->GetObject<Dcep>();
        Ptr<ExecEnv> ee = dcep->GetNode()->GetObject<ExecEnv>();
        ee->setLocalStateVariable("CepOpType", 2);
        bool constraintsFulfilled = true;
        for (auto c : constraints)
        {
            // All constraints must be fulfilled for constraintsFulfilled to be true
            constraintsFulfilled = c->Evaluate(e) && constraintsFulfilled;
        }

        if (!constraintsFulfilled) {
            ee->setLocalStateVariable("CepOpDoneYet", 1);
            return false;
        }

        dcep->PassedConstraints(e, q, ee->currentlyExecutingThread);

        auto *events1 = new std::vector<Ptr<CepEvent>>();
        auto *events2 = new std::vector<Ptr<CepEvent>>();
        bufman->put_event(e, this);//wait for event with corresponding sequence number
        bufman->read_events(*events1, *events2);

        if(!events1->empty() && !events2->empty() && e->type == event2)
        {
            delete events2;  // Not going to use events2
            Ptr<Node> node = cepEngine->GetObject<Dcep>()->GetNode();
            ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
            ee->Proceed(1, ee->currentlyExecutingThread, "handle-then-cepop", &ThenOperator::DoEvaluate, this, e, returned, events1, q, p, ops, cep);
        } else {
            // No sequences left
            ee->setLocalStateVariable("CepOpDoneYet", 1);
            ee->setLocalStateVariable("InsertedSequence", 1);
            delete events1;
            delete events2;
        }
        return false;
    }

    void Consume2(Ptr<CepEvent> e, Ptr<CepOperator> cepop)
    {
        Ptr<Window> w = cepop->stream_windows[e->GetStreamId()].at(0);
        w->RemoveEvent(e);
        if (cepop->prevOperator != nullptr && cepop->prevOperator->prevOperator != nullptr) {
            for (auto event : e->internalEvents) {
                Consume2(event, cepop->prevOperator->prevOperator);
            }
        }
    }

    /*
     * Algorithm: Check if there's output in window such that the "followed by" expression is expected,
     * and check if the newly received event fits that operator.
     * Evaluate the previous operator and place the contents in the current window if there was output.
     **/
    std::vector<Ptr<CepEvent> >
    ThenOperator::Evaluate2(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >& returned, Ptr<CEPEngine> cep)
    {
        std::vector<Ptr<CepEvent> > output;
        // Check if there's already a compound event in the window, and that the next operator expects this event.
        prevOperator->GetPartialResults(output);
        if (!output.empty()) {
            // We copy e because we will change its state and don't want that to be included when evaluating prevOperator
            Ptr<CepEvent> copy = CreateObject<CepEvent>(e);
            // We insert the partial results into the received event e such that it contains the previous events
            copy->internalEvents.insert(e->internalEvents.end(), output.begin(), output.end());
            // copy's internalEvents consists of the events that made it possible to insert copy in the nextOperator
            // That means these events belong to this query. If the consumption policy is to use events only a single
            // time, we will remove these events recursively once they trigger output for the whole query.
            // We can evaluate the next operator
            std::vector<Ptr<CepEvent> > output2;
            nextOperator->Evaluate2(copy, output2, cep);
            // Only consume if we're the last ThenOperator
            if (!output2.empty() && nextOperator->nextOperator == nullptr) {
                // We have output, and should consume events
                //returned.insert(returned.end(), output.begin(), output.end());
                //returned.insert(returned.end(), output2.begin(), output2.end());
                for (auto event : output2) {
                    if (event != copy) {
                        copy->internalEvents.emplace_back(event);
                    }
                }
                returned.emplace_back(copy);
                // Create complex event
                Consume2(copy, this->nextOperator);
            }
        }

        if (returned.empty()) {
            // Execute the prevOperator with a copy of the newly received event
            std::vector<Ptr<CepEvent> > output3 = prevOperator->Evaluate2(CreateObject<CepEvent>(e), output, cep);
        }
        return returned;
    }

    void
    ThenOperator::GetPartialResults(std::vector<Ptr<CepEvent> > &ret)
    {

    }

    bool
    OrOperator::Evaluate(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >& returned, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep)
    {
        /* everything is a match*/
        returned.push_back(e);
        // Here we insert the incoming event into the sequence
        Ptr<Node> node = cepEngine->GetObject<Dcep>()->GetNode();
        Ptr<ExecEnv> ee = node->GetObject<ExecEnv>();
        ee->setLocalStateVariable("CepOpType", 3);
        ee->setLocalStateVariable("CepOpDoneYet", 1);
        bool constraintsFulfilled = true;
        for (auto c : q->constraints)
        {
            // All constraints must be fulfilled for constraintsFulfilled to be true
            constraintsFulfilled = c->Evaluate(e) && constraintsFulfilled;
        }
        if (constraintsFulfilled) {
            p->HandleNewCepEvent(q, returned, this);
            ee->setLocalStateVariable("InsertedSequence", 1);
        } else {
            ee->setLocalStateVariable("InsertedSequence", 0);
        }
        return constraintsFulfilled;
    }

    std::vector<Ptr<CepEvent> >
    OrOperator::Evaluate2(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >& returned, Ptr<CEPEngine> cep)
    {
      return std::vector<Ptr<CepEvent> >();
    }

    void
    OrOperator::GetPartialResults(std::vector<Ptr<CepEvent> > &ret)
    {

    }
    
    bool
    CepOperator::ExpectingCepEvent(std::string eType)
    {
       return event1 == eType || event2 == eType;
    }

    void
    CepOperator::Consume(std::vector<Ptr<CepEvent> > &events)
    {
        bufman->consume(events);
    }

     
    /*********** BUFFER MANAGEMENT******************
     *****************************************************
     ************************************************************ */
    
    TypeId
    BufferManager::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::WindowManager")
        .SetParent<Object> ()
        .AddConstructor<CEPEngine> ()
        ;
        
        return tid;
    }
    
    void
    BufferManager::Configure(Ptr<CepOperator> op)
    {
        /*
         * setup the buffers with their corresponding event types
         */
        
    }
    
    void
    BufferManager::read_events(std::vector<Ptr<CepEvent> >& event1,
            std::vector<Ptr<CepEvent> >& event2)
    {
        //apply selection policy
        switch(selection_policy)
        {
            case SINGLE_SELECTION:
                event1 = events1;
                event2 = events2;
                break;
                
            default:
                NS_LOG_INFO(Simulator::Now() << " Not applying any selection policy");
        }
        
    }

    void
    BufferManager::put_event(Ptr<CepEvent> e, CepOperator *op)
    {
        if (e->type == op->event1)
        {
            events1.push_back(e);
        } else if (e->type == op->event2)
        {
            events2.push_back(e);
        } else
        {
            NS_LOG_INFO(Simulator::Now() << " BufferManager::put_event: Unknown event type");
        }
    }
    
    void
    BufferManager::consume(std::vector<Ptr<CepEvent> > &events)
    {
        switch(consumption_policy)
        {
            case SELECTED_CONSUMPTION:
                NS_LOG_INFO(Simulator::Now() << " Applying consumption policy " << SELECTED_CONSUMPTION);
                // Consuming event 1
                for (auto event : events) {
                    for (auto it     = events1.begin(); it != events1.end(); it++) {
                        auto e = *it;
                        if (e->m_seq == event->m_seq) {
                            events1.erase(it);
                            break;
                        }
                    }

                    // Consuming event 2
                    for (auto it = events2.begin(); it != events2.end(); it++) {
                        auto e = *it;
                        if (e->m_seq == event->m_seq) {
                            events2.erase(it);
                            break;
                        }
                    }
                }
                break;
                
            default:
                NS_LOG_INFO(Simulator::Now() << " Not applying any consumption policy");
                
        }
        
    }
      
    /***************************PRODUCER **************
     * ***************************************************
     * *************************************************************/
    
    TypeId
    Producer::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::Producer")
        .SetParent<Object> ()
        .AddConstructor<Producer> ()
        ;
        
        return tid;
    }

    void
    Producer::AddAttributesToNewEvent(Ptr<Query> q, std::vector<Ptr<CepEvent> > &events, Ptr<CepEvent> complex_event, CepOperator *op, int index) {
        Ptr<ExecEnv> ee = GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();
        if (index >= events.size()) {
            ee->setLocalStateVariable("attributes-left", 0);
            // Here we consume the previous events
            ee->queues["complex-pkts"]->Enqueue(ee->currentlyExecutingThread->m_currentLocation->m_executionInfo);
            Ptr<Forwarder> forwarder = GetObject<Forwarder>();
            forwarder->ForwardNewCepEvent(complex_event);
            op->Consume(events);
            return;
        }

        ee->setLocalStateVariable("attributes-left", 1);
        auto e = events[index];
        for( auto const& [key, val] : e->stringValues )
        {
            complex_event->stringValues[key] = val;
        }
        for( auto const& [key, val] : e->numberValues )
        {
            complex_event->numberValues[key] = val;
        }
        ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
        ee->Proceed(1, ee->currentlyExecutingThread, "assign-attributes-to-complex-event", &Producer::AddAttributesToNewEvent, this, q, events, complex_event, op, index+1);
    }
    
    void
    Producer::HandleNewCepEvent(Ptr<Query> q, std::vector<Ptr<CepEvent> > &events, CepOperator *op){
        if(q->actionType == NOTIFICATION)
        {
            Ptr<CepEvent> new_event = CreateObject<CepEvent>();
            new_event->timestamp = Simulator::Now();
            uint64_t delay = 0;
            uint32_t hops = 0;
            new_event->timestamp = Seconds(0);
            for(auto e : events)
            {
                delay = std::max(delay, e->delay);
                hops = hops + e->hopsCount;
                new_event->prevEvents.push_back(e);
                new_event->pkt = e->pkt;  // Last event is the most recently received event, with the relevant packet
                new_event->timestamp = std::max(e->timestamp, new_event->timestamp);
            }
            
            new_event->type = q->eventType;
            
            new_event->delay = delay; 
            new_event->hopsCount = hops;

            new_event->generatedByOp = op;
            
            
            if(q->isFinal)
            {
                new_event->event_class = FINAL_EVENT;
            }
            else
            {
                new_event->event_class = COMPOSITE_EVENT;
            }
            
            new_event->m_seq = events.back()->m_seq;

            Ptr<ExecEnv> ee = GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();

            AddAttributesToNewEvent(q, events, new_event, op, 0);
        }
    }

    void
    Producer::HandleNewCepEvent2(Ptr<CepQueryComponent> cqc, std::vector<Ptr<CepEvent> > &events)
    {
        static int cnt = 0;
        if (++cnt % 1000 == 0)
            std::cout << "Producer::HandleNewCepEvent2 " << cnt << std::endl;
        Ptr<CepEvent> new_event = CreateObject<CepEvent>();
        new_event->timestamp = Simulator::Now();
        uint64_t delay = 0;
        uint32_t hops = 0;
        new_event->timestamp = Seconds(0);
        for(auto e : events)
        {
            delay = std::max(delay, e->delay);
            hops = hops + e->hopsCount;
            new_event->prevEvents.push_back(e);
            new_event->pkt = e->pkt;  // Last event is the most recently received event, with the relevant packet
            new_event->timestamp = std::max(e->timestamp, new_event->timestamp);
        }
        new_event->type = cqc->GetEventType();
        new_event->delay = delay;
        new_event->hopsCount = hops;
        new_event->m_seq = events.back()->m_seq;
        Ptr<ExecEnv> ee = GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();
        for (auto event : events) {
            for( auto const& [key, val] : event->stringValues )
            {
                new_event->stringValues[key] = val;
            }
            for( auto const& [key, val] : event->numberValues )
            {
                new_event->numberValues[key] = val;
            }

            GetObject<Forwarder>()->ForwardNewCepEvent(new_event);
        }
    }

    
    
    /*************************** FORWARDER **************
     * ***************************************************
     * *************************************************************/
    
    TypeId
    Forwarder::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::Forwarder")
        .SetParent<Object> ()
        .AddConstructor<Forwarder> ()
        .AddTraceSource ("new event",
                       "a new event is produced by the CEP engine.",
                       MakeTraceSourceAccessor (&Forwarder::new_event))
        ;
        
        return tid;
    }
    
    Forwarder::Forwarder()
    {}
    
    void
    Forwarder::Configure()
    {}
    
    void
    Forwarder::ForwardNewCepEvent(Ptr<CepEvent> event)
    {
        new_event(event);
    }

    TypeId
    FogService::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::FogService")
                .SetParent<Object> ()
        ;

        return tid;
    }

    //Ptr<FogApplicationComponent> FogService::buildComponentDAG() {return nullptr;}
    
    TypeId
    Query::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::Query")
        .SetParent<FogService> ()
        .AddConstructor<Query> ()
        ;
        
        return tid;
    }
    
    
    /*********** EVENT PATTERN IMPLEMENTATION ******************
     *****************************************************
     ************************************************************ */
    
    TypeId
    CepEventPattern::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::CepEventPattern")
        .SetParent<Object> ()
        .AddConstructor<CepEventPattern> ()
        ;
        return tid;
    }
    
    
    /************** EVENT **************
     * ***********************************************
     * *************************************************/
    TypeId
    FogEvent::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::FogEvent")
                .SetParent<Object> ()
        ;

        return tid;
    }

    TypeId
    CepEvent::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::CepEvent")
        .SetParent<FogEvent> ()
        .AddConstructor<CepEvent> ()
        ;
        
        return tid;
    }

    CepEvent::CepEvent(std::vector<Ptr<CepEvent> > events)
    {
        internalEvents = events;
    }
    
    CepEvent::CepEvent(Ptr<CepEvent> e)
    {
        type = e->type;
        event_class = e->event_class;
        delay = e->delay;
        hopsCount = e->hopsCount;
        m_seq = e->m_seq;
        stringValues = e->stringValues;
        numberValues = e->numberValues;
    }
    
    CepEvent::CepEvent()
    {}
    
    void 
    CepEvent::operator=(Ptr<CepEvent> e)
    {
        this->type = e->type;
        
    }
    uint32_t
    CepEvent::getSize()
    { 
        return type.size()+sizeof(uint64_t)+sizeof(uint32_t);
    }
    
    SerializedCepEvent*
    CepEvent::serialize()
    {
        
        auto message = new SerializedCepEvent();
        message->type = this->type;
        message->event_class = this->event_class;
        message->event_base = this->event_base;
        message->size = sizeof(SerializedCepEvent);
        message->delay = this->delay;
        message->hopsCount = this->hopsCount;
        message->prevHopsCount = this->prevHopsCount;
        message->m_seq = this->m_seq;
        message->stringValues = this->stringValues;
        message->numberValues = this->numberValues;
        message->timestamp = this->timestamp;
        
        return message;
       
    }
    
    void
    CepEvent::deserialize(uint8_t *buffer, uint32_t size)
    {
        auto message = new SerializedCepEvent;
        
        memcpy(message, buffer, size);
        this->type = message->type;
        this->delay = message->delay;
        this->m_seq = message->m_seq;
        this->hopsCount = message->hopsCount;
        this->prevHopsCount = message->prevHopsCount;
        this->event_class = message->event_class;
        this->event_base = message->event_base;
        this->stringValues = message->stringValues;
        this->numberValues = message->numberValues;
        this->timestamp = message->timestamp;
    }
    
    void
    CepEvent::CopyCepEvent(Ptr<CepEvent> e)
    {
        e->event_class = event_class;
        e->hopsCount = hopsCount;
        e->m_seq = m_seq;
        e->prevHopsCount = prevHopsCount;
        e->delay = delay;
    }

    int
    CepEvent::GetStreamId()
    {
        return std::atoi(this->type.c_str());
    }


    /************** WINDOW ***************************/
    TypeId
    Window::GetTypeId(void)
    {
      static TypeId tid = TypeId("ns3::Window")
              .SetParent<Object> ()
      ;

      return tid;
    }

    TypeId
    TimeWindow::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::TimeWindow")
              .SetParent<Window> ()
        ;

        return tid;
    }

    TypeId
    SlidingTimeWindow::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::SlidingTimeWindow")
                .SetParent<TimeWindow> ()
        ;

        return tid;
    }

    TypeId
    TumblingTimeWindow::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::TumblingTimeWindow")
                .SetParent<TimeWindow> ()
        ;

        return tid;
    }

    TypeId
    TupleWindow::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::TupleWindow")
                .SetParent<Window> ()
        ;

        return tid;
    }

    TypeId
    SlidingTupleWindow::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::SlidingTupleWindow")
                .SetParent<TupleWindow> ()
        ;

        return tid;
    }

    TypeId
    TumblingTupleWindow::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::TumblingTupleWindow")
                .SetParent<TupleWindow> ()
        ;

        return tid;
    }

    TumblingTupleWindow::TumblingTupleWindow(int numberTuples)
    {
        this->size = numberTuples;
    }

    SlidingTupleWindow::SlidingTupleWindow(int numberTuples)
    {
        this->size = numberTuples;
    }

    TumblingTimeWindow::TumblingTimeWindow(Time t)
    {
        this->size = t;
        lastTumble = Simulator::Now();
    }

    SlidingTimeWindow::SlidingTimeWindow(Time t)
    {
        this->size = t;
    }

    void
    SlidingTimeWindow::InsertEvent(Ptr<CepEvent> e)
    {
        this->UpdateWindow();
        Time now = Simulator::Now();
        buffer.emplace_back(std::pair<Time, Ptr<CepEvent> >(now, e));
    }

    void
    Window::RemoveEvent(Ptr<CepEvent> e)
    {
        for (auto it=buffer.begin(); it!=buffer.end();) {
            auto p = *it;
            if (p.second == e) {
                buffer.erase(it);
            } else {
                ++it;
            }
        }
    }

    int
    Window::GetAvgNumberValue(std::string var_name)
    {
        int avg = 0;
        for (auto p : buffer) {
            auto e = p.second;
            avg += e->numberValues[var_name];
        }
        avg /= buffer.size();
        return avg;
    }

    void
    SlidingTupleWindow::InsertEvent(Ptr<CepEvent> e)
    {
        buffer.emplace_back(std::pair<Time, Ptr<CepEvent> >(Simulator::Now(), e));
        if (buffer.size() > this->size) {
            // Erase first element in buffer
            buffer.erase(buffer.begin(), buffer.begin()+1);
        }
    }

    void
    TumblingTimeWindow::InsertEvent(Ptr<CepEvent> e)
    {
        this->UpdateWindow();
        Time now = Simulator::Now();
        buffer.emplace_back(std::pair<Time, Ptr<CepEvent> >(now, e));
    }

    void
    TumblingTupleWindow::InsertEvent(Ptr<CepEvent> e)
    {
        // We empty the whole buffer excluding the element we received
        if (buffer.size() == this->size) {
            buffer.clear();
        }
        buffer.emplace_back(std::pair<Time, Ptr<CepEvent> >(Simulator::Now(), e));
    }

    std::vector <std::pair<Time, Ptr<CepEvent> > >
    Window::GetCepEvents()
    {
        return buffer;
    }

    std::vector <std::pair<Time, Ptr<CepEvent> > >
    SlidingTimeWindow::GetCepEvents()
    {
        this->UpdateWindow();
        return buffer;
    }

    std::vector <std::pair<Time, Ptr<CepEvent> > >
    TumblingTimeWindow::GetCepEvents()
    {
        this->UpdateWindow();
        return buffer;
    }

    void SlidingTimeWindow::UpdateWindow()
    {
        Time now = Simulator::Now();
        // Sliding window logic: If the item has been in the buffer for more than the size of the time window, it's removed from it
        for(auto it = buffer.begin(); it != buffer.end(); ++it) {
            std::pair<Time, Ptr<CepEvent> > p = *it;
            Time t = p.first;
            if (now - t > this->size) {
                buffer.erase(it);
            }
        }
    }

    void TumblingTimeWindow::UpdateWindow()
    {
        Time now = Simulator::Now();
        // Update last time the window tumbled
        lastTumble = lastTumble + this->size*((now-lastTumble)/size);
        for(auto it = buffer.begin(); it != buffer.end(); ++it) {
            std::pair<Time, Ptr<CepEvent> > p = *it;
            Time t = p.first;
            if (this->lastTumble > t) {
                buffer.erase(it);
            }
        }
    }
    
    
    /************** QUERY **************
     * ***********************************************
     * *************************************************/
    Query::Query()
    {
    }
    Query::Query(Ptr<Query> q)
    {
        this->actionType = q->actionType;
        this->eventType = q->eventType;
        this->id = q->id;
        this->isAtomic = q->isAtomic;
        this->isFinal = q->isFinal;
        this->output_dest = q->output_dest;
        this->assigned = q->assigned;
        this->currentHost = q->currentHost;
    }


    uint32_t 
    Query::getSerializedSize()
    {
        uint32_t size = eventType.size()+inevent1.size()+inevent2.size()+
                (sizeof(uint32_t)*3)+sizeof(bool);
        return size;
    }
    
    SerializedQuery*
    Query::serialize()
    {
        SerializedQuery * message = new SerializedQuery();
        
        
        output_dest.Serialize(message->output_dest);
        inputStream1_address.Serialize(message->inputStream1_address);
        inputStream2_address.Serialize(message->inputStream2_address);
        currentHost.Serialize(message->currentHost);
        message->actionType = this->actionType;
        message->eventType = this->eventType;
        message->q_id = this->id;
        message->isFinal = this->isFinal;
        message->isAtomic = this->isAtomic;
        message->inevent1 = this->inevent1;
        message->inevent2 = this->inevent2;
        message->op = this->op;
        message->assigned = this->assigned;
        
        message->parent_output = this->parent_output;
        message->size = sizeof(SerializedQuery);
        auto constraints_begin = this->constraints.begin();
        auto constraints_end = this->constraints.end();
        auto message_constraints_begin = message->constraints.begin();
        message->constraints = this->constraints;
        return message;
    }
    
    void
    Query::deserialize(uint8_t *buffer, uint32_t size)
    {
        //NS_LOG_INFO ("1");
        SerializedQuery *message = new SerializedQuery();
        memcpy(message, buffer, size);
        NS_LOG_INFO("DESERIALIZED MESSAGE " << message->eventType);
        this->actionType = message->actionType;
        //NS_LOG_INFO ("1");
        this->id = message->q_id;
        //NS_LOG_INFO ("1");
        this->isFinal = message->isFinal;
        //NS_LOG_INFO ("1");
        this->isAtomic = message->isAtomic;
        //NS_LOG_INFO ("1");
        this->eventType = message->eventType;
        //NS_LOG_INFO ("1");
        
        this->output_dest = Ipv4Address::Deserialize(message->output_dest);
        this->inputStream1_address = Ipv4Address::Deserialize(message->inputStream1_address);
        this->inputStream2_address = Ipv4Address::Deserialize(message->inputStream2_address);
        this->currentHost = Ipv4Address::Deserialize(message->currentHost);
        //NS_LOG_INFO ("1");
        this->inevent1 = message->inevent1;
        this->inevent2 = message->inevent2;
        this->parent_output = message->parent_output;
        //NS_LOG_INFO ("1");
        this->op = message->op;
        this->assigned = message->assigned;
        this->constraints = message->constraints;
        //NS_LOG_INFO ("1");
    }
    
}
