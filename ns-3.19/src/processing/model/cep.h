//
// Created by espen on 11.09.18.
//

#ifndef PROCESSING_DELAY_MODELS_CEP_H
#define PROCESSING_DELAY_MODELS_CEP_H

// back-end
#include <boost/msm/back/state_machine.hpp>
//front-end
#include <boost/msm/front/state_machine_def.hpp>

using namespace std;

class CEPEngine;
class CEPOp;
class OrCEPOp;
//class AndCEPOp;
class ThenCEPOp;

class OrCEPOpHelper;

class ThenCEPOpHelper;

class CEPEngine {

};

class CEPOp {

};

class OrCEPOp : CEPOp {
    OrCEPOpHelper *helper;

public:
    OrCEPOp();
};

/*class AndCEPOp : CEPOp {
    vector<and_sm> sequences;
};*/

class ThenCEPOp : CEPOp {
    ThenCEPOpHelper *helper;

public:
    ThenCEPOp();
};

#endif //PROCESSING_DELAY_MODELS_CEP_H
