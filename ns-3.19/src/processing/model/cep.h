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

class CEPOp {
public:
    virtual void InsertEvent(string event) {

    }
};

class OrCEPOp : public CEPOp {
    OrCEPOpHelper *helper;

public:
    OrCEPOp();
};

/*class AndCEPOp : CEPOp {
    vector<and_sm> sequences;
};*/

class ThenCEPOp : public CEPOp {
    ThenCEPOpHelper *helper;

public:
    ThenCEPOp();
};

class CEPEngine {
    vector<CEPOp> operators;

public:
    void InsertEvent(string event) {
        //for (std::vector<T>::iterator it = v.begin(); it != v.end(); ++it) {
        for (vector<CEPOp>::iterator it = operators.begin(); it != operators.end(); ++it) {
            CEPOp *op = (CEPOp*)&(*it);
            op->InsertEvent(event);
        }
    }

    void AddOperator(string type) {
        if (type == "OR") {
            OrCEPOp or_op;
            operators.push_back(or_op);
        } else if (type == "THEN") {
            ThenCEPOp then_op;
            operators.push_back(then_op);
        }
    }
};

#endif //PROCESSING_DELAY_MODELS_CEP_H
