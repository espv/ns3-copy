//
// Created by espen on 16.11.2019.
//

#ifndef NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_SIDDHITREXQUERY_H
#define NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_SIDDHITREXQUERY_H

#include "ns3/cep-engine.h"

namespace ns3 {

    enum SiddhiTRexQueryId {
        SIDDHITREXDETECTFIRETEMPGT80HUMIDITYLT10,
        SIDDHITREXDETECTFIRETEMPGT45HUMIDITYLT25
    };

    class SiddhiTRexQuery : public Query {
    public:
        static TypeId GetTypeId ();
        static Ptr<SiddhiTRexQuery> buildQuery(SiddhiTRexQueryId query_id);
    };

}


#endif //NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_SIDDHITREXQUERY_H
