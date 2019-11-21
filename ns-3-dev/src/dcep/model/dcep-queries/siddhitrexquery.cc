//
// Created by espen on 16.11.2019.
//

#include <nlohmann/json.hpp>
#include <ns3/common.h>
#include "siddhitrexquery.h"

using json = nlohmann::json;
namespace ns3 {

    TypeId
    SiddhiTRexQuery::GetTypeId(void)
    {
      static TypeId tid = TypeId("ns3::SiddhiTRexQuery")
              .SetParent<Query> ()
      ;

      return tid;
    }

    Ptr<SiddhiTRexQuery> SiddhiTRexQuery::buildQuery(SiddhiTRexQueryId query_id, json query_to_add) {
      Ptr<SiddhiTRexQuery> q = CreateObject<SiddhiTRexQuery>();
      q->json_query = query_to_add;
      switch (query_id) {
        case SIDDHITREXDETECTFIRETEMPGT80HUMIDITYLT10: {

          break;
        }
        case SIDDHITREXDETECTFIRETEMPGT45HUMIDITYLT25: {
          q->toBeProcessed = true;
          q->actionType = NOTIFICATION;
          q->query_base_id = query_id;
          static int query_cnt = 0;
          static int complex_event_cnt = 0;
          q->id = query_cnt++;
          q->isFinal = true;
          q->isAtomic = false;
          q->eventType = std::to_string(complex_event_cnt++);
          q->output_dest = Ipv4Address("10.0.0.3");
          std::string event1 = std::to_string((int)query_to_add["inevents"][0]);
          std::string event2 = std::to_string((int)query_to_add["inevents"][1]);
          q->inevent1 = event1;
          q->inevent2 = event2;
          q->window = Seconds(150000000000000000);
          q->isFinalWithinNode = true;

          Ptr<Constraint> c = CreateObject<Constraint>();
          c->var_name = "value";
          c->numberValue = 45;
          c->type = NUMBERGTCONSTRAINT;

          q->constraints.emplace_back(c);
          q->op = "then";
          q->assigned = false;
          q->currentHost.Set("0.0.0.0");
          q->parent_output = event1 + "then" + event2;
          break;
        }
        default: {
          NS_ABORT_MSG("SiddhiTRexQuery::buildQuery received invalid query_id " << query_id);
          break;
        }
      }
      return q;
    }

}