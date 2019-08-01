//
// Created by espen on 30.07.2019.
//

#ifndef NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_SOFTWARE_EXECUTION_MODEL_H
#define NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_SOFTWARE_EXECUTION_MODEL_H

#include <iostream>
#include "execenv.h"
#include "ns3/ptr.h"

using namespace ns3;

namespace ns3 {

class ExecEnv;

class SoftwareExecutionModel : public Object {
public:
    /*
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);

    std::string deviceFile;

    virtual void FsmTriggerCallback(Ptr<ExecEnv> ee, std::string fsm);
};

}

#endif //NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_SOFTWARE_EXECUTION_MODEL_H
