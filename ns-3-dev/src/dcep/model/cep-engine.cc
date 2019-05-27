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
    CEPEngine::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::CEPEngine")
        .SetParent<Object> ()
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
    
    void
    CEPEngine::ForwardProducedCepEvent(Ptr<CepEvent> e)
    {
        GetObject<Placement>()->ForwardProducedCepEvent(e);
    }

    void
    CEPEngine::DoCheckStringConstraints(Ptr<CepEvent> e, std::map<std::string, Ptr<Constraint>> constraints, Ptr<CEPEngine> cep, Ptr<Producer> producer, std::map<std::string, string> values)
    {
        Ptr<Placement> p = GetObject<Placement>();
        Ptr<ExecEnv> ee = GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();
        if (values.begin() == values.end()) {
            if (e->event_class != INTERMEDIATE_EVENT) {
                ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
                Ptr<Node> node = GetObject<Dcep>()->GetNode();
                if (node->GetId() == 2 && e->type == "C") {
                    std::cout << std::endl;
                }
                ee->Proceed(1, ee->currentlyExecutingThread, "handle-cepops-first", &Detector::ProcessCepEvent, GetObject<Detector>(), e);
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("constraints-done")->value = 1;
            } else {
                GetObject<Detector>()->ProcessCepEvent(e);
            }
            return;
        }
        auto k = values.begin()->first;
        values.erase(values.begin());

        if (e->event_class != INTERMEDIATE_EVENT) {
            // Constraint type is 1 (string)
            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("constraints-type")->value = 1;

            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("constraints-done")->value = 0;
            if (constraints[k]) {
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("constraint-processed")->value = 1;
            } else {
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("constraint-processed")->value = 0;
            }
            //e->pkt->m_executionInfo->executedByExecEnv = false;
            ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
            ee->Proceed(1, ee->currentlyExecutingThread, "check-constraints", &CEPEngine::DoCheckStringConstraints, this, e, constraints, cep, producer, values);
        } else {
            DoCheckStringConstraints(e, constraints, cep, producer, values);
        }
    }

    void
    CEPEngine::DoCheckNumberConstraints(Ptr<CepEvent> e, std::map<std::string, Ptr<Constraint>> constraints, Ptr<CEPEngine> cep, Ptr<Producer> producer, std::map<std::string, double> values, std::map<std::string, string> stringValues)
    {
        Ptr<Placement> p = GetObject<Placement>();
        Ptr<ExecEnv> ee = GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();
        if (values.begin() == values.end()) {
            if (e->event_class != INTERMEDIATE_EVENT) {
                //e->pkt->m_executionInfo->executedByExecEnv = false;
                ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
                ee->Proceed(1, ee->currentlyExecutingThread, "check-constraints", &CEPEngine::DoCheckStringConstraints, this, e, constraints, cep, producer, stringValues);
            } else {
                GetObject<Detector>()->ProcessCepEvent(e);
            }
            return;
        }
        auto k = values.begin()->first;
        values.erase(values.begin());

        if (e->event_class != INTERMEDIATE_EVENT) {
            // Constraint type is 0 (number)
            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("constraints-type")->value = 0;

            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("constraints-done")->value = 0;
            if (constraints[k]) {
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("constraint-processed")->value = 1;
            } else {
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("constraint-processed")->value = 0;
            }
            //e->pkt->m_executionInfo->executedByExecEnv = false;
            ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
            ee->Proceed(1, ee->currentlyExecutingThread, "check-constraints", &CEPEngine::DoCheckNumberConstraints, this, e, constraints, cep, producer, values, stringValues);
        } else {
            DoCheckNumberConstraints(e, constraints, cep, producer, values, stringValues);
        }
    }

    void
    CEPEngine::CheckConstraints(Ptr<CepEvent> e)
    {
        auto cep = GetObject<CEPEngine>();

        std::vector<Ptr<CepOperator>> ops;
        cep->GetOpsByInputCepEventType(e->type, ops);
        std::map<std::string, Ptr<Constraint> > constraints;
        for (auto cepop : ops)
        {
            for (auto c : cepop->constraints)
            {
                constraints[c->var_name] = c;
            }
        }
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
        Ptr<ExecEnv> ee = GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();
        NS_LOG_INFO(Simulator::Now() << " Time to process event " << e->m_seq << ": " << (Simulator::Now() - ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->timestamps[0]).GetMicroSeconds());
    }
    
    void
    CEPEngine::ProcessCepEvent(Ptr<CepEvent> e)
    {
        Ptr<ExecEnv> ee = GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();
        auto node = GetObject<Dcep>()->GetNode();

        if (e->event_class != INTERMEDIATE_EVENT) {
            //e->pkt->m_executionInfo->timestamps.emplace_back(Simulator::Now());
            ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->timestamps.emplace_back(Simulator::Now());
            e->timestamp = Simulator::Now();
            //e->pkt->m_executionInfo->executedByExecEnv = false;
            ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
            ee->Proceed(1, ee->currentlyExecutingThread, "check-constraints", &CEPEngine::CheckConstraints, this, e);
            //e->pkt->m_executionInfo->executedByExecEnv = false;
            ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
            ee->Proceed(1, ee->currentlyExecutingThread, "finished-processing", &CEPEngine::FinishedProcessingEvent, this, e);

            ee->ScheduleInterrupt(e->pkt, "HIRQ-1", Seconds(0));
        } else {
            CheckConstraints(e);
        }
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
    CEPEngine::InstantiateQuery(Ptr<Query> q){
        
        if(q->isAtomic)
        {
            /* instantiate atomic event */
            Ptr<CepOperator> cepOp = CreateObject<AtomicOperator>();
            cepOp->Configure(q, this);
            this->ops_queue.push_back(cepOp);
            GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>()->cepQueryQueues["all-cepops"]->push(cepOp);
            GetObject<Dcep>()->ActivateDatasource(q);
        }
        else
        {
            Ptr<CepOperator> cepOp;
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
            this->ops_queue.push_back(cepOp);
            GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>()->cepQueryQueues["all-cepops"]->push(cepOp);
        }

    }

    void
    CEPEngine::StoreQuery(Ptr<Query> q){
        queryPool.push_back(q);
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
        e->pkt = ee->currentlyExecutingThread->m_currentLocation->curPkt;
        auto op = ee->currentlyExecutingThread->m_currentLocation->curCepQuery;

        // The 'e->event_class != INTERMEDIATE_EVENT' part must happen before the evaluation of the operator, and
        // the else part must happen afterward. Therefore, they are detached.
        // Before the evaluation
        if (e->event_class != INTERMEDIATE_EVENT) {
            ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
            ee->Proceed(1, ee->currentlyExecutingThread, "handle-cepops", &Detector::CepOperatorProcessCepEvent, this, e, ops, cep, producer);
            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 0;
            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CreatedComplexEvent")->value = 0;
        }
        if (!op->ExpectingCepEvent(e->type)) {
            if (e->event_class != INTERMEDIATE_EVENT) {
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 1;
            }
            return;
        }

        std::vector<Ptr<CepEvent> > returned;

        op->Evaluate(e, returned, cep->GetQuery(op->queryId), producer, ops, cep);
        // After the evaluation
        if (e->event_class == INTERMEDIATE_EVENT) {
            CepOperatorProcessCepEvent(e, ops, cep, producer);
        }
    }
    
    void
    Detector::ProcessCepEvent(Ptr<CepEvent> e)
    {
        //std::cout << "Received event of type " << e->type << std::endl;
        auto cep = GetObject<CEPEngine>();

        std::vector<Ptr<CepOperator>> ops;
        cep->GetOpsByInputCepEventType(e->type, ops);
        Ptr<Producer> producer = GetObject<Producer>();

        auto node = GetObject<Dcep>()->GetNode();
        auto ee = node->GetObject<ExecEnv>();

        //ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 0;
        CepOperatorProcessCepEvent(e, ops, cep, producer);
    }


    bool
    NumberConstraint::Evaluate(Ptr<CepEvent> e)
    {
        if (e->numberValues.find(var_name) == e->numberValues.end())
            return true;
        switch (type) {
            case EQCONSTRAINT:
                return e->numberValues[var_name] == numberValue;
            case INEQCONSTRAINT:
                return e->numberValues[var_name] != numberValue;
            case LTCONSTRAINT:
                return e->numberValues[var_name] < numberValue;
            case LTECONSTRAINT:
                return e->numberValues[var_name] <= numberValue;
            case GTCONSTRAINT:
                return e->numberValues[var_name] > numberValue;
            case GTECONSTRAINT:
                return e->numberValues[var_name] >= numberValue;
            default:
                NS_FATAL_ERROR("Selected invalid constraint type for number constraint");
        }
    }

    bool
    StringConstraint::Evaluate(Ptr<CepEvent> e)
    {
        switch (type) {
            case EQCONSTRAINT:
                return e->stringValues[var_name] == stringValue;
            case INEQCONSTRAINT:
                return e->stringValues[var_name] != stringValue;
            default:
                NS_FATAL_ERROR("String constraints can only have equality or inequality constraints");
        }
    }


    TypeId
    NumberConstraint::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::NumberConstraint")
                .SetParent<Object> ()
        ;

        return tid;
    }


    TypeId
    StringConstraint::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::StringConstraint")
                .SetParent<Object> ()
        ;

        return tid;
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
    AndOperator::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::AndOperator")
            .SetParent<CepOperator> ()
        ;
        
        return tid;
    }

    TypeId
    ThenOperator::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::ThenOperator")
                .SetParent<CepOperator> ()
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
        ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpType")->value = 0;
        ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CreatedComplexEvent")->value = 0;
        ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 1;
        ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("attributes-left")->value = 0;
        if (!constraintsFulfilled) {
            return false;
        }

        p->GetObject<Forwarder>()->ForwardNewCepEvent(e);
        return true;
    }

    bool
    AndOperator::DoEvaluate(Ptr<CepEvent> newEvent2, std::vector<Ptr<CepEvent> >& returned, std::vector<Ptr<CepEvent>> *events1, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep) {
        Ptr<Node> node = cepEngine->GetObject<Dcep>()->GetNode();
        auto ee = node->GetObject<ExecEnv>();
        if (events1->empty()) {
            // No sequences left
            if (newEvent2->event_class != INTERMEDIATE_EVENT) {
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 1;
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 0;
                //newEvent2->pkt->m_executionInfo->executedByExecEnv = false;
                ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
                ee->Proceed(1, ee->currentlyExecutingThread, "handle-cepops", &Detector::CepOperatorProcessCepEvent,
                            cep->GetObject<Detector>(), newEvent2, ops, cep, p);
            } else {
                cep->GetObject<Detector>()->CepOperatorProcessCepEvent(newEvent2, ops, cep, p);
            }
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
            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 0;
            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 1;
        } else {
            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 0;
            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 0;
        }

        //newEvent2->pkt->m_executionInfo->executedByExecEnv = false;
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

        if (e->event_class != INTERMEDIATE_EVENT)
            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpType")->value = 1;

        if (!events1->empty() && !events2->empty())
        {
            Ptr<Node> node = cepEngine->GetObject<Dcep>()->GetNode();
            auto ee = node->GetObject<ExecEnv>();
            if (e->event_class != INTERMEDIATE_EVENT)
                //e->pkt->m_executionInfo->executedByExecEnv = false;
                ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
            if (e->type == events1->front()->type)
            {
                delete events1;  // Not going to use events1
                if (e->event_class != INTERMEDIATE_EVENT)
                    ee->Proceed(1, ee->currentlyExecutingThread, "handle-and-cepop", &AndOperator::DoEvaluate, this, e, returned, events2, q, p, ops, cep);
                else
                    DoEvaluate(e, returned, events2, q, p, ops, cep);
            }
            else
            {
                delete events2;  // Not going to use events2
                if (e->event_class != INTERMEDIATE_EVENT)
                    ee->Proceed(1, ee->currentlyExecutingThread, "handle-and-cepop", &AndOperator::DoEvaluate, this, e, returned, events1, q, p, ops, cep);
                else
                    DoEvaluate(e, returned, events1, q, p, ops, cep);
            }
            
        } else {
            // No sequences left
            if (e->event_class != INTERMEDIATE_EVENT) {
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 1;
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 1;
            }
        }
        return false;
    }

    bool
    ThenOperator::DoEvaluate(Ptr<CepEvent> newEvent2, std::vector<Ptr<CepEvent> >& returned, std::vector<Ptr<CepEvent>> *events1, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep) {
        Ptr<Node> node = cepEngine->GetObject<Dcep>()->GetNode();
        auto ee = node->GetObject<ExecEnv>();
        if (events1->empty()) {
            // No sequences left
            if (newEvent2->event_class != INTERMEDIATE_EVENT) {
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 1;
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 0;
                //newEvent2->pkt->m_executionInfo->executedByExecEnv = false;
                ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
                ee->Proceed(1, ee->currentlyExecutingThread, "handle-cepops", &Detector::CepOperatorProcessCepEvent, cep->GetObject<Detector>(), newEvent2, ops, cep, p);
            } else {
                cep->GetObject<Detector>()->CepOperatorProcessCepEvent(newEvent2, ops, cep, p);
            }
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
            if (newEvent2->event_class != INTERMEDIATE_EVENT) {
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 1;
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CreatedComplexEvent")->value = 1;
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 1;
            }
        } else if (newEvent2->event_class != INTERMEDIATE_EVENT) {
            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 0;
            //newEvent2->pkt->m_executionInfo->executedByExecEnv = false;
            ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
            ee->Proceed(1, ee->currentlyExecutingThread, "handle-then-cepop", &ThenOperator::DoEvaluate, this, newEvent2, returned, events1, q, p, ops, cep);
        } else {
            DoEvaluate(newEvent2, returned, events1, q, p, ops, cep);
        }
    }

    bool
    ThenOperator::Evaluate(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >& returned, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep)
    {
        static int cnt = 0;
        std::cout << "Times that ThenOperator::Evaluate is called " << ++cnt << std::endl;
        Ptr<ExecEnv> ee = cepEngine->GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();
        if (e->event_class != INTERMEDIATE_EVENT)
            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpType")->value = 2;
        bool constraintsFulfilled = true;
        for (auto c : constraints)
        {
            // All constraints must be fulfilled for constraintsFulfilled to be true
            constraintsFulfilled = c->Evaluate(e) && constraintsFulfilled;
        }

        if (!constraintsFulfilled) {
            if (e->event_class != INTERMEDIATE_EVENT) {
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 1;
            }
            return false;
        }

        auto *events1 = new std::vector<Ptr<CepEvent>>();
        auto *events2 = new std::vector<Ptr<CepEvent>>();
        bufman->put_event(e, this);//wait for event with corresponding sequence number
        bufman->read_events(*events1, *events2);

        if(!events1->empty() && !events2->empty() && e->type == event2)
        {
            delete events2;  // Not going to use events2
            Ptr<Node> node = cepEngine->GetObject<Dcep>()->GetNode();
            if (e->event_class != INTERMEDIATE_EVENT) {
                //e->pkt->m_executionInfo->executedByExecEnv = false;
                ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
                ee->Proceed(1, ee->currentlyExecutingThread, "handle-then-cepop", &ThenOperator::DoEvaluate, this, e, returned, events1, q, p, ops, cep);
            } else {
                DoEvaluate(e, returned, events1, q, p, ops, cep);
            }
        } else {
            // No sequences left
            if (e->event_class != INTERMEDIATE_EVENT) {
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 1;
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 1;
            }
            delete events1;
            delete events2;
        }
        return false;
    }

    bool
    OrOperator::Evaluate(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >& returned, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep)
    {
        static int cnt = 0;
        std::cout << "OrOperator::Evaluate cnt " << cnt++ << std::endl;
        /* everything is a match*/
        returned.push_back(e);
        // Here we insert the incoming event into the sequence
        Ptr<Node> node = cepEngine->GetObject<Dcep>()->GetNode();
        Ptr<ExecEnv> ee = node->GetObject<ExecEnv>();
        if (e->event_class != INTERMEDIATE_EVENT) {
            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpType")->value = 0;
            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 1;
        }
        bool constraintsFulfilled = true;
        for (auto c : q->constraints)
        {
            // All constraints must be fulfilled for constraintsFulfilled to be true
            constraintsFulfilled = c->Evaluate(e) && constraintsFulfilled;
        }
        if (constraintsFulfilled) {
            p->HandleNewCepEvent(q, returned, this);
            if (e->event_class != INTERMEDIATE_EVENT)
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 1;
        } else {
            if (e->event_class != INTERMEDIATE_EVENT)
                ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 0;
        }
        if (e->event_class != INTERMEDIATE_EVENT) {
            //e->pkt->m_executionInfo->executedByExecEnv = false;
            ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
            ee->Proceed(1, ee->currentlyExecutingThread, "handle-cepops", &Detector::CepOperatorProcessCepEvent, cep->GetObject<Detector>(), e,
                        ops, cep, p);
        } else {
            cep->GetObject<Detector>()->CepOperatorProcessCepEvent(e, ops, cep, p);
        }
        return constraintsFulfilled;
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

        /*
        if(events1.empty() && events2.empty())
        {
            events1.push_back(e);
        }
        else if ((events1.empty()) && (!events2.empty()))
        {
            if(events2.front()->type == e->type)
            {
                events2.push_back(e);
            }
            else
            {
                events1.push_back(e);
            }
        }
        else if ((!events1.empty()) && (events2.empty()))
        {
            if(events1.front()->type == e->type)
            {
                events1.push_back(e);
            }
            else
            {
                events2.push_back(e);
            }
        }
        else
        {
            NS_LOG_INFO("unknown type");
        }*/

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

                    if (event->event_class == INTERMEDIATE_EVENT) {
                        event->generatedByOp->Consume(event->prevEvents);
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
    Producer::AddAttributesToNewEvent(Ptr<Query> q, std::vector<Ptr<CepEvent> > &events, Ptr<CepEvent> complex_event, CepOperator *op) {
        Ptr<ExecEnv> ee = GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();
        if (events.empty()) {
            ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("attributes-left")->value = 0;
            std::cout << "Node " << GetObject<Dcep>()->GetNode()->GetId() << ", Thread " << ee->currentlyExecutingThread->name << "-" << ee->currentlyExecutingThread->m_pid << ": AddAttributesToNewEvent 2/2" << std::endl;
            if (complex_event->event_class == INTERMEDIATE_EVENT) {
                complex_event->pkt = Create<Packet>();
                Ptr<CEPEngine> cepEngine = GetObject<CEPEngine>();
                cepEngine->ProcessCepEvent(complex_event);
            } else {
                // Here we consume the previous events
                //complex_event->pkt->m_executionInfo->target = "";
                ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->target = "";
                ee->queues["complex-pkts"]->Enqueue(ee->currentlyExecutingThread->m_currentLocation->m_executionInfo);
                Ptr<Forwarder> forwarder = GetObject<Forwarder>();
                forwarder->ForwardNewCepEvent(complex_event);
                op->Consume(events);
            }
            return;
        }

        ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("attributes-left")->value = 1;
        auto e = events.front();
        for( auto const& [key, val] : e->stringValues )
        {
            complex_event->stringValues[key] = val;
        }
        for( auto const& [key, val] : e->numberValues )
        {
            complex_event->numberValues[key] = val;
        }
        events.erase(events.begin());
        std::cout << "Node " << GetObject<Dcep>()->GetNode()->GetId() << ", Thread " << ee->currentlyExecutingThread->name << "-" << ee->currentlyExecutingThread->m_pid << ": AddAttributesToNewEvent 1/2" << std::endl;
        if (complex_event->event_class == INTERMEDIATE_EVENT) {
            AddAttributesToNewEvent(q, events, complex_event, op);
        } else {
            //complex_event->pkt->m_executionInfo->executedByExecEnv = false;
            ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executedByExecEnv = false;
            ee->Proceed(1, ee->currentlyExecutingThread, "assign-attributes-to-complex-event", &Producer::AddAttributesToNewEvent, this, q, events, complex_event, op);
        }
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
            else if (q->isFinalWithinNode)
            {
                new_event->event_class = INTERMEDIATE_EVENT;
            } else
            {
                new_event->event_class = COMPOSITE_EVENT;
            }
            
            new_event->m_seq = events.back()->m_seq;

            Ptr<ExecEnv> ee = GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();

            if (new_event->event_class == INTERMEDIATE_EVENT) {
                AddAttributesToNewEvent(q, events, new_event, op);
            } else {
                //if (!events.empty()) {
                    //ee->currentlyExecutingThread->m_currentLocation->getLocalStateVariable("attributes-left")->value = 1;
                //}
                AddAttributesToNewEvent(q, events, new_event, op);
                //new_event->pkt->m_executionInfo->executedByExecEnv = false;
                //ee->Proceed(1, new_event->pkt, "assign-attributes-to-complex-event",
                //            &Producer::AddAttributesToNewEvent, this, q, events, new_event, op);
            }
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
    Query::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::Query")
        .SetParent<Object> ()
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
    CepEvent::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::CepEvent")
        .SetParent<Object> ()
        .AddConstructor<CepEvent> ()
        ;
        
        return tid;
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
        message->size = sizeof(SerializedCepEvent);
        message->delay = this->delay;
        message->hopsCount = this->hopsCount;
        message->prevHopsCount = this->prevHopsCount;
        message->m_seq = this->m_seq;
        message->stringValues = this->stringValues;
        message->numberValues = this->numberValues;
        message->timestamp = this->timestamp;
        NS_LOG_INFO(Simulator::Now() << " serialized type " << message->type);
        
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
