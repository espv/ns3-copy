//
// Created by espen on 11.09.18.
//

#include <iostream>
// back-end
#include <boost/msm/back/state_machine.hpp>
//front-end
#include <boost/msm/front/state_machine_def.hpp>

#include "cep.h"

namespace msm = boost::msm;
namespace mpl = boost::mpl;

class ThenCEPOpHelper
{
    // Espen's THEN events
    struct first_event {};
    struct second_event {};


    // front-end: define the FSM structure
    struct then_ : public msm::front::state_machine_def<then_>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&)
        {
            std::cout << "entering: Then" << std::endl;
        }
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& )
        {
            std::cout << "leaving: Then" << std::endl;
        }

        // The list of FSM states
        struct Empty : public msm::front::state<>
        {
            // every (optional) entry/exit methods get the event passed.
            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {std::cout << "entering: Empty" << std::endl;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {std::cout << "leaving: Empty" << std::endl;}
        };
        struct ReceivedFirstEvent : public msm::front::state<>
        {
            template <class Event,class FSM>
            void on_entry(Event const& ,FSM&) {std::cout << "entering: ReceivedFirstEvent" << std::endl;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {std::cout << "leaving: ReceivedFirstEvent" << std::endl;}
        };

        // sm_ptr still supported but deprecated as functors are a much better way to do the same thing
        struct ReceivedSecondEvent : public msm::front::state<msm::front::default_base_state,msm::front::sm_ptr>
        {
            template <class Event,class FSM>
            void on_entry(Event const& ,FSM&) {std::cout << "entering: ReceivedSecondEvent" << std::endl;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {std::cout << "leaving: ReceivedSecondEvent" << std::endl;}
            void set_sm_ptr(then_* th)
            {
                m_then=th;
            }
            then_ *m_then;
        };

        // the initial state of the player SM. Must be defined
        typedef Empty initial_state;

        // transition actions
        void receive_first_event(first_event const&)       { std::cout << "then::receive_first_event\n"; }
        // Guard condition on first transition
        bool first_event_guard(first_event const&)       { return true; }
        void receive_second_event(second_event const&)    { std::cout << "then::receive_second_event\n"; }
        // Guard condition on second transition
        bool second_event_guard(second_event const&)    { return true; }

        typedef then_ t; // makes transition table cleaner

        // Transition table for player
        struct transition_table : mpl::vector<
                 //    Start     Event         Next      Action				 Guard
                 //  +---------+-------------+---------+---------------------+----------------------+
                 row < Empty, first_event, ReceivedFirstEvent, &t::receive_first_event, &t::first_event_guard >,
                 //  +---------+-------------+---------+---------------------+----------------------+
                 row < ReceivedFirstEvent, second_event, ReceivedSecondEvent, &t::receive_second_event, &t::second_event_guard >
        //  +---------+-------------+---------+---------------------+----------------------+
        > {};
        // Replaces the default no-transition response.
        template <class FSM,class Event>
        void no_transition(Event const& e, FSM&,int state)
        {
            std::cout << "no transition from state " << state
                      << " on event " << typeid(e).name() << std::endl;
        }
    };
    // Pick a back-end
    // Each ThenCEPOp object should have a vector of the state machine below
    typedef msm::back::state_machine<then_> then_sm;

    //
    // Testing utilities.
    //
    /*static char const* const state_names[] = { "Empty", "ReceivedFirstEvent", "ReceivedSecondEvent" };
    void pstate(then_sm const& t)
    {
        std::cout << " -> " << state_names[t.current_state()[0]] << std::endl;
    }

    void test()
    {
        then_sm t;
        // needed to start the highest-level SM. This will call on_entry and mark the start of the SM
        t.start();
        pstate(t);
        // go to Open, call on_exit on Empty, then action, then on_entry on Open
        t.process_event(first_event()); pstate(t);
        t.process_event(first_event()); pstate(t);
        std::cout << "stop fsm" << std::endl;
        t.stop();
    }*/

    vector<then_sm> sequences;

    void InsertEvent() {
        //for (std::vector<T>::iterator it = v.begin(); it != v.end(); ++it) {
        for (std::vector<then_sm>::iterator it = sequences.begin(); it != sequences.end(); ++it) {

        }
    }

public:
    ThenCEPOpHelper() {
        then_sm first_sm;
        first_sm.start();
        sequences.push_back(first_sm);
    }
};

class OrCEPOpHelper
{
    // Espen's OR events
    struct first_event {};
    struct second_event {};


    // front-end: define the FSM structure
    struct or_ : public msm::front::state_machine_def<or_>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&)
        {
            std::cout << "entering: Then" << std::endl;
        }
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& )
        {
            std::cout << "leaving: Then" << std::endl;
        }

        // The list of FSM states
        struct Empty : public msm::front::state<>
        {
            // every (optional) entry/exit methods get the event passed.
            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {std::cout << "entering: Empty" << std::endl;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {std::cout << "leaving: Empty" << std::endl;}
        };
        struct ReceivedEvent : public msm::front::state<>
        {
            template <class Event,class FSM>
            void on_entry(Event const& ,FSM&) {std::cout << "entering: ReceivedEvent" << std::endl;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {std::cout << "leaving: ReceivedEvent" << std::endl;}
        };

        // the initial state of the player SM. Must be defined
        typedef Empty initial_state;

        // transition actions
        void receive_first_event(first_event const&)       { std::cout << "or_sm::receive_first_event\n"; }
        // Guard condition on first transition
        bool first_event_guard(first_event const&)       { return true; }
        void receive_second_event(second_event const&)    { std::cout << "or_sm::receive_second_event\n"; }
        // Guard condition on second transition
        bool second_event_guard(second_event const&)    { return true; }

        typedef or_ o; // makes transition table cleaner

        // Transition table for player
        struct transition_table : mpl::vector<
                //    Start     Event         Next      Action				 Guard
                //  +---------+-------------+---------+---------------------+----------------------+
                row < Empty, first_event, ReceivedEvent, &o::receive_first_event, &o::first_event_guard >,
                //  +---------+-------------+---------+---------------------+----------------------+
                row < Empty, second_event, ReceivedEvent, &o::receive_second_event, &o::second_event_guard >
        //  +---------+-------------+---------+---------------------+----------------------+
        > {};
        // Replaces the default no-transition response.
        template <class FSM,class Event>
        void no_transition(Event const& e, FSM&,int state)
        {
            std::cout << "no transition from state " << state
                      << " on event " << typeid(e).name() << std::endl;
        }
    };
    // Pick a back-end
    // Each ThenCEPOp object should have a vector of the state machine below
    typedef msm::back::state_machine<or_> or_sm;

    //
    // Testing utilities.
    //
    /*static char const* const state_names[] = { "Empty", "ReceivedEvent" };
    void pstate(or_sm const& t)
    {
        std::cout << " -> " << state_names[t.current_state()[0]] << std::endl;
    }

    void test()
    {
        or_sm o;
        // needed to start the highest-level SM. This will call on_entry and mark the start of the SM
        o.start();
        pstate(t);
        // go to Open, call on_exit on Empty, then action, then on_entry on Open
        o.process_event(first_event()); pstate(o);
        o.process_event(first_event()); pstate(o);
        std::cout << "stop fsm" << std::endl;
        o.stop();
    }*/

    vector<or_sm> sequences;

    void InsertEvent() {
        //for (std::vector<T>::iterator it = v.begin(); it != v.end(); ++it) {
        for (std::vector<or_sm>::iterator it = sequences.begin(); it != sequences.end(); ++it) {

        }
    }

public:
    OrCEPOpHelper() {
        or_sm first_sm;
        first_sm.start();
        sequences.push_back(first_sm);
    }
};

OrCEPOp::OrCEPOp() {
    helper = new OrCEPOpHelper();

};

ThenCEPOp::ThenCEPOp() {
    helper = new ThenCEPOpHelper();
};
