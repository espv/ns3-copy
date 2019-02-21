
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

#include "dcep-state.h"
#include "ns3/log.h"
#include "cep-engine.h"
#include "ns3/abort.h"
#include "communication.h"
#include "placement.h"
namespace ns3 
{
    
    NS_OBJECT_ENSURE_REGISTERED(DcepState);
    NS_LOG_COMPONENT_DEFINE("dcepstate");


    TypeId
    CepEventRoutingTableEntry::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::CepEventRoutingTableEntry")
                .SetParent<Object> ()
                .AddConstructor<CepEventRoutingTableEntry> ()
                ;

        return tid;
    }
    
    CepEventRoutingTableEntry::CepEventRoutingTableEntry()
    {
        freezeAck_counter = 0;
        monitoring = false;
    }
    
            /* ... */
    TypeId
    DcepState::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::DcepState")
                .SetParent<Object> ()
                .AddConstructor<DcepState> ()
        ;
        return tid;
    }
    
    DcepState::DcepState()
    {}
    
    void
    DcepState::Configure ()
    {
        Ptr<Placement> pcomp = GetObject<Placement>();
        Ptr<PlacementPolicy> ppolicy = GetObject<PlacementPolicy>();
        
        
        ppolicy->TraceConnectWithoutContext("new local placement",
                    MakeCallback(&DcepState::HandlerLocalPlacement, this));
        
    }
    
    
    void
    DcepState::HandlerLocalPlacement(std::string eType)
    {
        OperatorState ostate = GetState(eType);
        std::cout << "HandlerLocalPlacement" << std::endl;
        if (ostate == ACTIVE)
            return;
        if(ostate == UNDEFINED)
        {
            SetState (eType, ACTIVE);
        }
        else
        {
            NS_ABORT_MSG("UNRECOGNIZED STATE TRANSITION PREVIOUS STATE WAS " << ostate );
        }
    }
    
    Ptr<CepEventRoutingTableEntry>
    DcepState::lookUpCepEventRoutingTable(std::string eventType) 
    {

        Ptr<CepEventRoutingTableEntry> er;
        std::vector<Ptr<CepEventRoutingTableEntry> >::iterator it;
        for (it = eventRoutingTable.begin(); it != eventRoutingTable.end(); ++it) 
        {

            er = (Ptr<CepEventRoutingTableEntry>) * it;

            if (er->source_query->eventType == eventType) {
                // NS_LOG_INFO("Found entry with event type " << er->source_query->eventType);
                return er;
            }

        }

        er = CreateObject<CepEventRoutingTableEntry>();
        er->next_hop = Ipv4Address::GetAny();

        return er;
    }
    
    void
    DcepState::CreateCepEventRoutingTableEntry (Ptr<Query> q)
    {
        Ptr<CepEventRoutingTableEntry> ee = CreateObject<CepEventRoutingTableEntry>();
        ee->source_query = q;
        ee->state = ACTIVE;
        if(ee->source_query->isFinal)
        {
            ee->source_query->output_dest = GetObject<Communication>()->GetSinkAddress();
            //ee->dataSources.push_back("10.0.0.1");
            //ee->dataSources.push_back("10.0.0.3");
        }
        
        /*if ((q->eventType == "AorB") || (q->eventType == "AandB"))
        {
            ee->dataSources.push_back("10.0.0.2");
            ee->dataSources.push_back("10.0.0.3");
        }*/
        this->eventRoutingTable.push_back(ee);
    }
    
    
    bool
    DcepState::IsExpected(Ptr<CepEvent> e)
    {
        for (uint32_t i = 0; i < eventRoutingTable.size(); i++)
        {
            if ((eventRoutingTable[i]->source_query->inevent1 == e->type ||
                    (eventRoutingTable[i]->source_query->inevent2 == e->type)) && !eventRoutingTable[i]->source_query->isAtomic)
            {
                // Commented out by Espen because I have multiple queries that lead to the complex event A
                // Multiple complex events of same type is not supported with this type of state handling
                //if (eventRoutingTable[i]->state == ACTIVE)
                //{
                    return true;
                //}
                
            }
        }
        return false;
    }
    
    
    Ipv4Address
    DcepState::GetOuputDest(std::string eType)
    {
        Ptr<CepEventRoutingTableEntry> erte = this->lookUpCepEventRoutingTable(eType);
        return erte->source_query->output_dest;
    }
    
    bool 
    DcepState::IsActive(std::string eType)
    {
        Ptr<CepEventRoutingTableEntry> erte = this->lookUpCepEventRoutingTable(eType);
        return erte->state;
    }
    
    
    Ptr<Query>
    DcepState::GetQuery(std::string eType)
    {
        Ptr<CepEventRoutingTableEntry> erte = this->lookUpCepEventRoutingTable(eType);
        return erte->source_query;
    }
    
    void
    DcepState::SetNextHop(std::string eType, Ipv4Address adr)
    {
        
        for(uint32_t i = 0; i != eventRoutingTable.size(); i++)
        {
            if(eventRoutingTable[i]->source_query->eventType == eType)
            {
                eventRoutingTable[i]->next_hop = adr;
                NS_LOG_INFO ("NEXT HOP " << eventRoutingTable[i]->next_hop);
            }
        }
        
    }
    
    Ipv4Address
    DcepState::GetNextHop(std::string eType)
    {
      Ptr<CepEventRoutingTableEntry> erte = this->lookUpCepEventRoutingTable(eType);
      return erte->next_hop;
    }
    
    
    void
    DcepState::SetOutDest(std::string eType, Ipv4Address adr)
    {
        for(uint32_t i = 0; i != eventRoutingTable.size(); i++)
        {
            if(eventRoutingTable[i]->source_query->eventType == eType)
            {
                eventRoutingTable[i]->source_query->output_dest = adr;
            }
        }
    }
    
    void
    DcepState::SetCurrentProcessor(std::string eType, Ipv4Address adr)
    {
        for(uint32_t i = 0; i != eventRoutingTable.size(); i++)
        {
            if(eventRoutingTable[i]->source_query->eventType == eType)
            {
                eventRoutingTable[i]->current_processor = adr;
            }
        }
    }
    
    Ipv4Address
    DcepState::GetCurrentProcessor(std::string eType)
    {
        Ptr<CepEventRoutingTableEntry> erte = this->lookUpCepEventRoutingTable(eType);
        return erte->current_processor;
    }
    
    void
    DcepState::SetState(std::string eType, OperatorState state)
    {
        for(uint32_t i = 0; i != eventRoutingTable.size(); i++)
        {
            if(eventRoutingTable[i]->source_query->eventType == eType)
            {
                eventRoutingTable[i]->state = state;
            }
        }
    }
    
    
    OperatorState 
    DcepState::GetState(std::string eType)
    {
        Ptr<CepEventRoutingTableEntry> erte = this->lookUpCepEventRoutingTable(eType);
        return erte->state;
    }
}
