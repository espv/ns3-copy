//
// Created by espen on 11.09.18.
//

#ifndef PROCESSING_DELAY_MODELS_CEP_H
#define PROCESSING_DELAY_MODELS_CEP_H


using namespace std;

// back-end
#include <boost/msm/back/state_machine.hpp>
//front-end
#include <boost/msm/front/state_machine_def.hpp>

#include "ns3/object.h"

namespace ns3 {
    class ProcessCEPEngine;
    class CEPOp;
    class OrCEPOp;
    //class AndCEPOp;
    class ThenCEPOp;

    class CEPOpHelper : public Object {
    public:
        int GetNumberSequences(void) {return 0;}
        static TypeId GetTypeId (void);
    };

    class OrCEPOpHelper : public CEPOpHelper {
    public:
        vector<string> event_sequences;
        static TypeId GetTypeId (void);
    };

    class ThenCEPOpHelper : public CEPOpHelper {
    public:
        vector<string> event_sequences;
        static TypeId GetTypeId (void);
    };

    class CEPOp : public Object {
    public:
        Ptr<CEPOpHelper> helper;
        virtual void InsertEvent(string event) {

        }
    };

    class OrCEPOp : public CEPOp {
    public:
        Ptr<OrCEPOpHelper> helper;
        static TypeId GetTypeId (void);
        OrCEPOp();
    };

    /*class AndCEPOp : CEPOp {
        vector<and_sm> sequences;
    };*/

    class ThenCEPOp : public CEPOp {
    public:
        Ptr<ThenCEPOpHelper> helper;
        static TypeId GetTypeId (void);
        ThenCEPOp();
    };

    class ProcessCEPEngine : public Object {
    public:
        vector< Ptr<CEPOp> > operators;

        static TypeId GetTypeId (void);

        ProcessCEPEngine();

        void InsertEvent(string event);

        void AddOperator(string type, vector<string> event_sequences);
    };

    class CEPSequence : public Object {
    public:
        static TypeId GetTypeId (void);
    };
}
#endif //PROCESSING_DELAY_MODELS_CEP_H
