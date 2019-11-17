//
// Created by espen on 16.11.2019.
//

#ifndef NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_SIDDHITREXQUERY_H
#define NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_SIDDHITREXQUERY_H

#include "ns3/cep-engine.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace ns3 {

    enum SiddhiTRexQueryId {
        SIDDHITREXDETECTFIRETEMPGT80HUMIDITYLT10,
        SIDDHITREXDETECTFIRETEMPGT45HUMIDITYLT25
    };

    class SiddhiTRexQuery : public Query {
    public:
        static TypeId GetTypeId ();
        static Ptr<SiddhiTRexQuery> buildQuery(SiddhiTRexQueryId query_id, json query_to_add);
    };

}


#endif //NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_SIDDHITREXQUERY_H
