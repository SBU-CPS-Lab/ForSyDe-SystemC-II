/**********************************************************************
    * dde_process_constructors.hpp -- Process constructors in the DDE *
    *                                MOC                              *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Providing basic process constructors for modeling      *
    *          distributed discrete-event systems in ForSyDe-SystemC  *
    *                                                                 *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef DDE_PROCESS_CONSTRUCTORS_HPP
#define DDE_PROCESS_CONSTRUCTORS_HPP

/*! \file dde_process_constructors.hpp
 * \brief Implements the basic process constructors in the DDE MoC
 *
 *  This file includes the basic process constructors used for modeling
 * in the distributed discrete-event model of computation.
 */

#include <systemc>
#include <functional>
#include <algorithm>
#include <tuple>
#include <deque>
#include <utility>
#include <boost/numeric/ublas/matrix.hpp>

// Streams a std::vector under FORSYDE_INTROSPECTION (e.g. "ss << offsets"
// below) via prettyprint.hpp's generic container operator<<, which this
// file otherwise relies on forsyde.hpp having included first.
#include "prettyprint.hpp"

#include "tt_event.hpp"
#include "dde_process.hpp"

namespace ForSyDe
{

namespace DDE
{

using namespace sc_core;
using namespace boost::numeric::ublas;

//! Process constructor for a combinational process with one input and one output
/*! This class is used to build combinational processes with one input
 * and one output. The class is parameterized for input and output
 * data-types.
 */
template <typename T0, typename T1>
class comb : public dde_process,
             public ForSyDe::detail::bindable<comb<T0,T1>>
{
public:
    DDE_in<T1>  iport1;       ///< port for the input channel
    DDE_out<T0> oport1;        ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<comb<T0,T1>>::operator();
    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(abst_ext<T0>&, const T1&)> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented function to it and writes the
     * results using the output port
     */
    comb(sc_module_name _name,      ///< process name
         functype _func             ///< function to be passed
         ) : dde_process(_name), iport1("iport1"), oport1("oport1"),
             _func(_func)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_func",func_name+std::string("_func")));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DDE::comb";}

private:
    // Inputs and output variables
    abst_ext<T0>* oval;
    ttn_event<T1>* iev1;

    //! The function passed to the process constructor
    functype _func;

    //Implementing the abstract semantics
    void init()
    {
        oval = new abst_ext<T0>;
        iev1 = new ttn_event<T1>;
    }

    void prep()
    {
        *iev1 = iport1.read();
    }

    void exec()
    {
        if (is_present(get_value(*iev1)))
            _func(*oval, unsafe_from_abst_ext(get_value(*iev1)));
        else
            *oval = abst_ext<T0>();
    }

    void prod()
    {
        auto oev = ttn_event<T0>(*oval, get_time(*iev1));
        write_multiport(oport1, oev);
        // synchronization with kernel time
        wait_until(get_time(oev), name());
    }

    void clean()
    {
        delete iev1;
        delete oval;
    }

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! Process constructor for a combinational process with two inputs and one output
/*! similar to comb with two inputs
 */
template <typename T0, typename T1, typename T2>
class comb2 : public dde_process,
              public ForSyDe::detail::bindable<comb2<T0,T1,T2>>
{
public:
    DDE_in<T1> iport1;        ///< port for the input channel 1
    DDE_in<T2> iport2;        ///< port for the input channel 2
    DDE_out<T0> oport1;        ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<comb2<T0,T1,T2>>::operator();
    auto in_ports()  {return std::tie(iport1,iport2);}
    auto out_ports() {return std::tie(oport1);}

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(abst_ext<T0>&, const abst_ext<T1>&, const abst_ext<T2>&)> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input ports,
     * applies the user-imlpemented function to them and writes the
     * results using the output port
     */
    comb2(sc_module_name _name,      ///< process name
          functype _func             ///< function to be passed
          ) : dde_process(_name), iport1("iport1"), iport2("iport2"), oport1("oport1"),
              _func(_func)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_func",func_name+std::string("_func")));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DDE::comb2";}
private:
    // Inputs and output variables
    abst_ext<T0>* oval;
    ttn_event<T1> *next_iev1;
    ttn_event<T2> *next_iev2;
    abst_ext<T1> *cur_ival1;
    abst_ext<T2> *cur_ival2;

    // the current time (local time)
    sc_time tl;

    // clocks of the input ports (channel times)
    sc_time in1T, in2T;

    //! The function passed to the process constructor
    functype _func;

    //Implementing the abstract semantics
    void init()
    {
        oval = new abst_ext<T0>;
        next_iev1 = new ttn_event<T1>;
        next_iev2 = new ttn_event<T2>;
        cur_ival1 = new abst_ext<T1>;
        cur_ival2 = new abst_ext<T2>;
        in1T = in2T = tl = SC_ZERO_TIME;
    }

    void prep()
    {
        if (in1T == tl)
        {
            *next_iev1 = iport1.read();
            in1T = get_time(*next_iev1);
        }
        if (in2T == tl)
        {
            *next_iev2 = iport2.read();
            in2T = get_time(*next_iev2);
        }

        // update channel clocks and the local clock
        tl = std::min(in1T, in2T);

        // update current values
        if (get_time(*next_iev1) == tl)
            *cur_ival1 = get_value(*next_iev1);
        else
            *cur_ival1 = abst_ext<T1>();
        if (get_time(*next_iev2) == tl)
            *cur_ival2 = get_value(*next_iev2);
        else
            *cur_ival2 = abst_ext<T2>();
    }

    void exec()
    {
        if (is_absent(*cur_ival1) && is_absent(*cur_ival2))
            *oval = abst_ext<T0>();
        else
            _func(*oval, *cur_ival1, *cur_ival2);
    }

    void prod()
    {
        write_multiport(oport1, ttn_event<T0>(*oval,tl));
        wait_until(tl, name());
    }

    void clean()
    {
        delete oval;
        delete next_iev1;
        delete next_iev2;
        delete cur_ival1;
        delete cur_ival2;
    }

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! Process constructor for a delay element
/*! This class is used to build a delay element. Given an initial null-
 * extended value and a delay time, it inserts this value as the first
 * event in time zero in the output and delays the rest of the events by
 * the delay time.
 *
 * It is mandatory to include at least one delay element in all feedback
 * loops since combinational loops are forbidden in ForSyDe.
 */
template <class T>
class delay : public dde_process,
              public ForSyDe::detail::bindable<delay<T>>
{
public:
    DDE_in<T>  iport1;       ///< port for the input channel
    DDE_out<T> oport1;        ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<delay<T>>::operator();
    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which inserts the initial element, reads
     * data from its input port, and writes the results using the output
     * port.
     */
    delay(sc_module_name _name,     ///< process name
          abst_ext<T> init_val,     ///< initial value
          sc_time delay_time        ///< delay time
          ) : dde_process(_name), iport1("iport1"), oport1("oport1"),
              init_val(init_val), delay_time(delay_time)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << init_val;
        arg_vec.push_back(std::make_tuple("init_val", ss.str()));
        ss.str("");
        ss << delay_time;
        arg_vec.push_back(std::make_tuple("delay_time", ss.str()));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DDE::delay";}

private:
    // Initial value and the delay time
    abst_ext<T> init_val;
    sc_time delay_time;
    // Tag of the event consumed this firing, before delay_time is added
    sc_time in_time;

    // Inputs and output variables
    ttn_event<T>* ev;

    //Implementing the abstract semantics
    void init()
    {
        ev = new ttn_event<T>;
        auto oev = ttn_event<T>(init_val, SC_ZERO_TIME);
        write_multiport(oport1, oev);
        wait(SC_ZERO_TIME);
    }

    void prep()
    {
        *ev = iport1.read();
    }

    void exec()
    {
        in_time = get_time(*ev);
        set_time(*ev, get_time(*ev)+delay_time);
    }

    void prod()
    {
        write_multiport(oport1, *ev);
        // A DDE process advances its local clock to the latest time for
        // which it has complete input information -- for one input, the
        // tag it has just consumed.
        // This used to advance to get_time(*ev), which exec() has just
        // moved forward by delay_time -- the tag of the event being
        // emitted rather than the one consumed. A delay knows about its
        // input up to in_time and no further.
        wait_until(in_time, name());
    }

    void clean()
    {
        delete ev;
    }
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! Process constructor for a Mealy machine
/*! This class is used to build a finite state machine of type Mealy.
 * Given an initial state, a next-state function, and an output decoding
 * function it creates a Mealy process.
 */
template <typename IT, typename ST, typename OT>
class mealy : public dde_process,
              public ForSyDe::detail::bindable<mealy<IT,ST,OT>>
{
public:
    DDE_in<IT>  iport1;        ///< port for the input channel
    DDE_out<OT> oport1;        ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<mealy<IT,ST,OT>>::operator();
    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}

    //! Type of the next-state function to be passed to the process constructor
    typedef std::function<void(ST&, const ST&, const ttn_event<IT>&)> ns_functype;

    //! Type of the output-decoding function to be passed to the process constructor
    typedef std::function<void(abst_ext<OT>&, const ST&, const ttn_event<IT>&)> od_functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented functions to the input and current
     * state and writes the results using the output port
     */
    mealy(sc_module_name _name,  ///< process name
           const ns_functype& _ns_func, ///< The next_state function
           const od_functype& _od_func, ///< The output-decoding function
           const ST& init_st,           ///< Initial state
           const sc_time& delay_time    ///< The constant delay for output
          ) : dde_process(_name), _ns_func(_ns_func), _od_func(_od_func),
              init_st(init_st), delay_time(delay_time)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_ns_func",func_name+std::string("_ns_func")));
        arg_vec.push_back(std::make_tuple("_od_func",func_name+std::string("_od_func")));
        std::stringstream ss;
        ss << init_st;
        arg_vec.push_back(std::make_tuple("init_st",ss.str()));
        ss.str("");
        ss << delay_time;
        arg_vec.push_back(std::make_tuple("delay_time",ss.str()));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "DDE::mealy";}

private:
    //! The functions passed to the process constructor
    ns_functype _ns_func;
    od_functype _od_func;

    // Initial value
    ST init_st;

    sc_time delay_time;

    // Input, output, current state, and next state variables
    ttn_event<IT>* itok;
    ST* stval;
    ST* nsval;
    abst_ext<OT>* oval;

    //Implementing the abstract semantics
    void init()
    {
        itok = new ttn_event<IT>;
        stval = new ST;
        *stval = init_st;
        nsval = new ST;
        oval = new abst_ext<OT>;
    }

    void prep()
    {
        *itok = iport1.read();
    }

    void exec()
    {
        _ns_func(*nsval, *stval, *itok);
        _od_func(*oval, *stval, *itok);
        *stval = *nsval;
    }

    void prod()
    {
        write_multiport(oport1, ttn_event<OT>(*oval,get_time(*itok)+delay_time));
        // A DDE process advances its local clock to the latest time for
        // which it has complete input information -- for one input, the
        // tag it has just consumed.
        // The tag it emits is later than that, by delay_time; what the
        // process knows about is still only up to the input.
        wait_until(get_time(*itok), name());
    }

    void clean()
    {
        delete itok;
        delete stval;
        delete nsval;
        delete oval;
    }
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! Process constructor for a Mealy machine with two inputs
/*! This class is used to build a finite state machine of type Mealy
 * with two inputs. Given an initial state, a next-state function, and
 * an output decoding function it creates a Mealy process.
 */
template <typename IT1, typename IT2, typename ST, typename OT>
class mealy2 : public dde_process,
               public ForSyDe::detail::bindable<mealy2<IT1,IT2,ST,OT>>
{
public:
    DDE_in<IT1>  iport1;        ///< port for the input channel
    DDE_in<IT2>  iport2;        ///< port for the input channel
    DDE_out<OT> oport1;        ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<mealy2<IT1,IT2,ST,OT>>::operator();
    auto in_ports()  {return std::tie(iport1,iport2);}
    auto out_ports() {return std::tie(oport1);}

    //! Type of the next-state function to be passed to the process constructor
    typedef std::function<void(ST&, const ST&, const ttn_event<IT1>&, const ttn_event<IT2>&)> ns_functype;

    //! Type of the output-decoding function to be passed to the process constructor
    typedef std::function<void(abst_ext<OT>&, const ST&, const ttn_event<IT1>&, const ttn_event<IT2>&)> od_functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented functions to the input and current
     * state and writes the results using the output port
     */
    mealy2(sc_module_name _name, ///< process name
           const ns_functype& _ns_func, ///< The next_state function
           const od_functype& _od_func, ///< The output-decoding function
           const ST& init_st,           ///< Initial state
           const sc_time& delay_time    ///< The constant delay for output
          ) : dde_process(_name), _ns_func(_ns_func), _od_func(_od_func),
              init_st(init_st), delay_time(delay_time)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_ns_func",func_name+std::string("_ns_func")));
        arg_vec.push_back(std::make_tuple("_od_func",func_name+std::string("_od_func")));
        std::stringstream ss;
        ss << init_st;
        arg_vec.push_back(std::make_tuple("init_st",ss.str()));
        ss.str("");
        ss << delay_time;
        arg_vec.push_back(std::make_tuple("delay_time",ss.str()));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "DDE::mealy2";}

private:
    //! The functions passed to the process constructor
    ns_functype _ns_func;
    od_functype _od_func;

    // Initial value
    ST init_st;

    sc_time delay_time;

    // Input, output, current state, and next state variables
    ttn_event<IT1> *next_iev1;
    ttn_event<IT2> *next_iev2;
    abst_ext<IT1> *cur_ival1;
    abst_ext<IT2> *cur_ival2;
    ST* stval;
    ST* nsval;
    abst_ext<OT>* oval;

    // the current time (local time)
    sc_time tl;

    // clocks of the input ports (channel times)
    sc_time in1T, in2T;

    //Implementing the abstract semantics
    void init()
    {
        next_iev1 = new ttn_event<IT1>;
        next_iev2 = new ttn_event<IT2>;
        cur_ival1 = new abst_ext<IT1>;
        cur_ival2 = new abst_ext<IT2>;
        stval = new ST;
        *stval = init_st;
        nsval = new ST;
        oval = new abst_ext<OT>;
        in1T = in2T = tl = SC_ZERO_TIME;
    }

    void prep()
    {
        if (in1T == tl)
        {
            *next_iev1 = iport1.read();
            in1T = get_time(*next_iev1);
        }
        if (in2T == tl)
        {
            *next_iev2 = iport2.read();
            in2T = get_time(*next_iev2);
        }

        // update channel clocks and the local clock
        tl = std::min(in1T, in2T);

        // update current values
        if (get_time(*next_iev1) == tl)
            *cur_ival1 = get_value(*next_iev1);
        else
            *cur_ival1 = abst_ext<IT1>();
        if (get_time(*next_iev2) == tl)
            *cur_ival2 = get_value(*next_iev2);
        else
            *cur_ival2 = abst_ext<IT2>();
    }

    void exec()
    {
        if (is_absent(*cur_ival1) && is_absent(*cur_ival2))
            *oval = abst_ext<OT>();
        else
        {
            _ns_func(*nsval, *stval, ttn_event<IT1>(*cur_ival1,tl), ttn_event<IT2>(*cur_ival2,tl));
            _od_func(*oval, *stval, ttn_event<IT1>(*cur_ival1,tl), ttn_event<IT2>(*cur_ival2,tl));
            *stval = *nsval;
        }
    }

    void prod()
    {
        write_multiport(oport1, ttn_event<OT>(*oval,tl+delay_time));
        wait_until(tl, name());
    }

    void clean()
    {
        delete next_iev1;
        delete next_iev2;
        delete cur_ival1;
        delete cur_ival2;
        delete stval;
        delete nsval;
        delete oval;
    }
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! Process constructor for a source process
/*! This class is used to build a souce process which only has an output.
 * Given an initial state and a function, the process repeatedly applies
 * the function to the current state to produce next state, which is
 * also the process output. It can be used in test-benches.
 */
template <class T>
class source : public dde_process,
               public ForSyDe::detail::bindable<source<T>>
{
public:
    DDE_out<T> oport1;        ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<source<T>>::operator();
    auto out_ports() {return std::tie(oport1);}

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(ttn_event<T>&, const ttn_event<T>&)> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * and writes the result using the output port
     */
    source(sc_module_name _name,   ///< The module name
           functype _func,         ///< function to be passed
           ttn_event<T> init_st,   ///< Initial state
           unsigned long long take=0 ///< number of tokens produced (0 for infinite)
          ) : dde_process(_name), oport1("oport1"),
              init_st(init_st), take(take), _func(_func)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_func",func_name+std::string("_func")));
        std::stringstream ss;
        ss << init_st;
        arg_vec.push_back(std::make_tuple("init_st", ss.str()));
        ss.str("");
        ss << take;
        arg_vec.push_back(std::make_tuple("take", ss.str()));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DDE::source";}

private:
    ttn_event<T> init_st;        // The current state
    unsigned long long take;    // Number of tokens produced

    ttn_event<T>* cur_st;        // The current state of the process
    unsigned long long tok_cnt;
    bool infinite;

    //! The function passed to the process constructor
    functype _func;

    //Implementing the abstract semantics
    void init()
    {
        cur_st = new ttn_event<T>;
        *cur_st = init_st;
        write_multiport(oport1, *cur_st);
        wait_until(get_time(*cur_st), name());
        if (take==0) infinite = true;
        tok_cnt = 1;
    }

    void prep() {}

    void exec()
    {
        _func(*cur_st, *cur_st);
    }

    void prod()
    {
        if (tok_cnt++ < take || infinite)
        {
            write_multiport(oport1, *cur_st);
            wait_until(get_time(*cur_st), name());
        }
        else wait();
    }

    void clean()
    {
        delete cur_st;
    }

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! Process constructor for a source process with vector input
/*! This class is used to build a souce process which only has an output.
 * Given the test bench vector, the process iterates over the emenets
 * of the vector and outputs one value on each evaluation cycle.
 */
template <class T>
class vsource : public dde_process,
                public ForSyDe::detail::bindable<vsource<T>>
{
public:
    DDE_out<T> oport1;     ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<vsource<T>>::operator();
    auto out_ports() {return std::tie(oport1);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which writes the result using the output
     * port.
     */
    vsource(sc_module_name _name,                   ///< the module name
            const std::vector<T>& values,           ///< event values
            const std::vector<sc_time>& offsets     ///< event offsets
            ) : dde_process(_name), oport1("oport1"),
                values(values), offsets(offsets)
    {
        if (values.size()<offsets.size())
            SC_REPORT_ERROR(name(),"Error matching values and offsets vectors!");
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << values;
        arg_vec.push_back(std::make_tuple("values", ss.str()));
        ss.str("");
        ss << offsets;
        arg_vec.push_back(std::make_tuple("offsets", ss.str()));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DDE::vsource";}
private:
    std::vector<T> values;
    std::vector<sc_time> offsets;
    size_t iter;

    //Implementing the abstract semantics
    void init()
    {
        iter = 0;
    }

    void prep() {}

    void exec() {}

    void prod()
    {
        write_multiport(oport1, ttn_event<T>(abst_ext<T>(values[iter]), offsets[iter]));
        wait_until(offsets[iter], name());
        iter++;
        if (iter == values.size())
        {
            // Promise no more values
            write_multiport(oport1, ttn_event<T>(abst_ext<T>(), sc_max_time()));
            wait();
        }
    }

    void clean() {}

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! Process constructor for a sink process
/*! This class is used to build a sink process which only has an input.
 * Its main purpose is to be used in test-benches. The process repeatedly
 * applies a given function to the current input.
 */
template <class T>
class sink : public dde_process,
             public ForSyDe::detail::bindable<sink<T>>
{
public:
    DDE_in<T> iport1;         ///< port for the input channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<sink<T>>::operator();
    auto in_ports()  {return std::tie(iport1);}

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(const ttn_event<T>&)> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * in each cycle.
     */
    sink(sc_module_name _name,      ///< process name
         functype _func             ///< function to be passed
        ) : dde_process(_name), iport1("iport1"), _func(_func)

    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_func",func_name+std::string("_func")));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DDE::sink";}

private:
    ttn_event<T>* val;         // The current state of the process

    //! The function passed to the process constructor
    functype _func;

    //Implementing the abstract semantics
    void init()
    {
        val = new ttn_event<T>;
    }

    void prep()
    {
        *val = iport1.read();
    }

    void exec()
    {
        _func(*val);
    }

    void prod()
    {
        // A DDE process advances its local clock to the latest time for
        // which it has complete input information -- for one input, the
        // tag it has just consumed.
        // Having no output does not exempt it: a sink that reports using
        // sc_time_stamp() would otherwise read a clock that never moves.
        wait_until(get_time(*val), name());
    }

    void clean()
    {
        delete val;
    }

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
    }
#endif
};

//! The zip process with two inputs and one output
/*! This process "zips" two incoming signals into one signal of tuples.
 */
template <class T1, class T2>
class zip : public dde_process,
            public ForSyDe::detail::bindable<zip<T1,T2>>
{
public:
    DDE_in<T1> iport1;        ///< port for the input channel 1
    DDE_in<T2> iport2;        ///< port for the input channel 2
    DDE_out< std::tuple<abst_ext<T1>,abst_ext<T2>> > oport1;///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<zip<T1,T2>>::operator();
    auto in_ports()  {return std::tie(iport1,iport2);}
    auto out_ports() {return std::tie(oport1);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * zips them together and writes the results using the output port
     */
    zip(sc_module_name _name)
         :dde_process(_name), iport1("iport1"), iport2("iport2"), oport1("oport1")
    { }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DDE::zip";}

private:
    // inputs and output variables
    ttn_event<T1> *next_iev1;
    ttn_event<T2> *next_iev2;
    abst_ext<T1> *cur_ival1;
    abst_ext<T2> *cur_ival2;
    abst_ext< std::tuple<abst_ext<T1>,abst_ext<T2>> >* oval;

    // the current time (local time)
    sc_time tl;

    // clocks of the input ports (channel times)
    sc_time in1T, in2T;

    void init()
    {
        next_iev1 = new ttn_event<T1>;
        next_iev2 = new ttn_event<T2>;
        cur_ival1 = new abst_ext<T1>;
        cur_ival2 = new abst_ext<T2>;
        in1T = in2T = tl = SC_ZERO_TIME;
        oval = new abst_ext< std::tuple<abst_ext<T1>,abst_ext<T2>> >();
    }

    void prep()
    {
        if (in1T == tl)
        {
            *next_iev1 = iport1.read();
            in1T = get_time(*next_iev1);
        }
        if (in2T == tl)
        {
            *next_iev2 = iport2.read();
            in2T = get_time(*next_iev2);
        }

        // update channel clocks and the local clock
        tl = std::min(in1T, in2T);

        // update current values
        if (get_time(*next_iev1) == tl)
            *cur_ival1 = get_value(*next_iev1);
        else
            *cur_ival1 = abst_ext<T1>();
        if (get_time(*next_iev2) == tl)
            *cur_ival2 = get_value(*next_iev2);
        else
            *cur_ival2 = abst_ext<T2>();
    }

    void exec()
    {
        if (is_absent(*cur_ival1) && is_absent(*cur_ival2))
            oval->set_abst();
        else
            *oval = abst_ext< std::tuple<abst_ext<T1>,abst_ext<T2>> >(
                std::make_tuple(*cur_ival1,*cur_ival2)
            );
    }

    void prod()
    {
        auto temp_event = ttn_event<std::tuple<abst_ext<T1>,abst_ext<T2>>>(*oval,tl);
        write_multiport(oport1,temp_event);
        wait_until(tl, name());
    }

    void clean()
    {
        delete next_iev1;
        delete next_iev2;
        delete cur_ival1;
        delete cur_ival2;
        delete oval;
    }

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! The zipX process with a vector of inputs and one output
/*! This process "zips" a vector of incoming signals into one signal of
 * vector type.
 */
template <class T1, std::size_t N>
class zipX : public dde_process,
             public ForSyDe::detail::bindable<zipX<T1,N>>
{
public:
    std::array<DDE_in<T1>,N> iport;    ///< port array for the input channels
    DDE_out<std::array<abst_ext<T1>,N>> oport1;  ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<zipX<T1,N>>::operator();
    auto in_ports()  {return std::apply([](auto&... p){return std::tie(p...);}, iport);}
    auto out_ports() {return std::tie(oport1);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * zips them together and writes the results using the output port
     */
    zipX(sc_module_name _name)
         :dde_process(_name), oport1("oport1")
    { }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DDE::zipX";}

private:
    // inputs and output variables
    std::array<ttn_event<T1>,N> next_ievs;
    std::array<abst_ext<T1>,N> cur_ivals;
    abst_ext< std::array<abst_ext<T1>,N> >* oval;

    // the current time (local time)
    sc_time tl;

    // clocks of the input ports (channel times)
    std::array<sc_time,N> insT;

    void init()
    {
        insT.fill(SC_ZERO_TIME);
        tl = SC_ZERO_TIME;
        oval = new abst_ext< std::array<abst_ext<T1>,N> >();
    }

    void prep()
    {
        for (size_t i=0;i<N;i++)
            if (insT[i] == tl)
            {
                next_ievs[i] = iport[i].read();
                insT[i] = get_time(next_ievs[i]);
            }

        // update channel clocks and the local clock
        tl = *std::min_element(insT.begin(), insT.end());

        // update current values
        for (size_t i=0;i<N;i++)
            if (get_time(next_ievs[i]) == tl)
                cur_ivals[i] = get_value(next_ievs[i]);
            else
                cur_ivals[i] = abst_ext<T1>();
    }

    void exec()
    {
        if (std::all_of(cur_ivals.begin(), cur_ivals.end(), [](abst_ext<T1> el){
            return is_absent(el);
        }))
            oval->set_abst();
        else
            *oval = abst_ext< std::array<abst_ext<T1>,N> >(cur_ivals);
    }

    void prod()
    {
        auto temp_event = ttn_event<std::array<abst_ext<T1>,N>>(*oval,tl);
        write_multiport(oport1,temp_event);
        wait_until(tl, name());
    }

    void clean()
    {
        delete oval;
    }

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! The unzip process with one input and two outputs
/*! This process "unzips" a signal of tuples into two separate signals
 */
template <class T1, class T2>
class unzip : public dde_process,
              public ForSyDe::detail::bindable<unzip<T1,T2>>
{
public:
    DDE_in<std::tuple<abst_ext<T1>,abst_ext<T2>>> iport1;///< port for the input channel
    DDE_out<T1> oport1;        ///< port for the output channel 1
    DDE_out<T2> oport2;        ///< port for the output channel 2

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<unzip<T1,T2>>::operator();
    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1,oport2);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input ports,
     * unzips them and writes the results using the output ports
     */
    unzip(sc_module_name _name)
         :dde_process(_name), iport1("iport1"), oport1("oport1"), oport2("oport2")
    {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DDE::unzip";}
private:
    // intermediate values
    ttn_event< std::tuple<abst_ext<T1>,abst_ext<T2>> >* in_ev;
    abst_ext<T1>* out_val1;
    abst_ext<T2>* out_val2;

    void init()
    {
        in_ev = new ttn_event< std::tuple<abst_ext<T1>,abst_ext<T2>> >;
        out_val1 = new abst_ext<T1>;
        out_val2 = new abst_ext<T2>;
    }

    void prep()
    {
        *in_ev = iport1.read();
    }

    void exec()
    {
        if (is_absent(get_value(*in_ev)))
        {
            *out_val1 = abst_ext<T1>();
            *out_val2 = abst_ext<T2>();
        }
        else
        {
            *out_val1 = std::get<0>(unsafe_from_abst_ext(get_value(*in_ev)));
            *out_val2 = std::get<1>(unsafe_from_abst_ext(get_value(*in_ev)));
        }
    }

    void prod()
    {
        sc_time te(get_time(*in_ev));
        write_multiport(oport1,ttn_event<T1>(*out_val1,te));  // write to the output 1
        write_multiport(oport2,ttn_event<T2>(*out_val2,te));  // write to the output 2
        // A DDE process advances its local time to the largest input tag
        // it has consumed. unzip did not, while its sibling unzipX did;
        // the two now agree.
        wait_until(te, name());
    }

    void clean()
    {
        delete in_ev;
        delete out_val1;
        delete out_val2;
    }

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! The unzipX process with a vector of outputs and one input
/*! This process "unzips" a signal of vector type into a vector of
 * output signals.
 */
template <class T1, std::size_t N>
class unzipX : public dde_process,
               public ForSyDe::detail::bindable<unzipX<T1,N>>
{
public:
    DDE_in<std::array<abst_ext<T1>,N>> iport1;  ///< port for the input channel
    std::array<DDE_out<T1>,N> oport;    ///< port array for the output channels

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<unzipX<T1,N>>::operator();
    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::apply([](auto&... p){return std::tie(p...);}, oport);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * zips them together and writes the results using the output port
     */
    unzipX(sc_module_name _name)
         :dde_process(_name), iport1("iport1")
    { }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DDE::unzipX";}

private:
    // intermediate values
    ttn_event<std::array<abst_ext<T1>,N>>* in_ev;

    // output events
    std::array<ttn_event<T1>,N> oevs;

    sc_time tl;

    void init()
    {
        in_ev = new ttn_event<std::array<abst_ext<T1>,N>>;
        tl = SC_ZERO_TIME;
    }

    void prep()
    {
        *in_ev = iport1.read();
    }

    void exec()
    {
        // in_ev is a ttn_event -- a time tag wrapped around an
        // absent-extended value -- so the presence test and the
        // unwrapping both have to go through get_value(), and tl has to
        // be taken from this event before it is used to stamp the
        // outputs rather than after.
        tl = get_time(*in_ev);
        if (is_absent(get_value(*in_ev)))
        {
            for (size_t i=0; i<N; i++)
                oevs[i] = ttn_event<T1>(abst_ext<T1>(),tl);
        }
        else
        {
            const auto& vals = unsafe_from_abst_ext(get_value(*in_ev));
            for (size_t i=0; i<N; i++)
                oevs[i] = ttn_event<T1>(vals[i],tl);
        }
    }

    void prod()
    {
        for (size_t i=0; i<N; i++)
            write_multiport(oport[i],oevs[i]);  // write to the output i
        wait_until(tl, name());
    }

    void clean()
    {
        delete in_ev;
    }

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};


//! The zipN process with a variable number of inputs and one output
/*! The heterogeneous counterpart of zipX: where that one takes N inputs
 * of a single type and emits an array, this takes one input per type and
 * emits a tuple. The merge is the same Chandy-Misra one -- read a channel
 * only when its clock has caught up with the local one, then advance the
 * local clock to the earliest of the channel clocks and emit whichever
 * inputs are tagged with it, the rest absent.
 */
template <class... Ts>
class zipN : public dde_process,
             public ForSyDe::detail::bindable<zipN<Ts...>>
{
public:
    std::tuple<DDE_in<Ts>...> iport;                    ///< tuple of ports for the input channels
    DDE_out<std::tuple<abst_ext<Ts>...>> oport1;        ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<zipN<Ts...>>::operator();
    auto in_ports()  {return std::apply([](auto&... p){return std::tie(p...);}, iport);}
    auto out_ports() {return std::tie(oport1);}

    //! The constructor requires the module name
    zipN(sc_module_name _name      ///< process name
        ) : dde_process(_name), oport1("oport1") {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DDE::zipN";}

private:
    static constexpr std::size_t N = sizeof...(Ts);

    std::tuple<ttn_event<Ts>...> next_ievs;
    std::tuple<abst_ext<Ts>...>  cur_ivals;
    abst_ext<std::tuple<abst_ext<Ts>...>> oval;

    sc_time tl;                     ///< local clock
    std::array<sc_time,N> insT;     ///< channel clocks, one per input

    void init()
    {
        insT.fill(SC_ZERO_TIME);
        tl = SC_ZERO_TIME;
    }

    void prep()
    {
        // Read only the channels the local clock has caught up with
        std::size_t n{0};
        std::apply([&](auto&... port){
            std::apply([&](auto&... ev){
                ([&](auto& p, auto& e){
                    if (insT[n] == tl)
                    {
                        e = p.read();
                        insT[n] = get_time(e);
                    }
                    n++;
                }(port, ev), ...);
            }, next_ievs);
        }, iport);

        tl = *std::min_element(insT.begin(), insT.end());

        // Whatever is tagged with the new local time is present this
        // cycle; everything else is absent for it
        std::apply([&](auto&... ev){
            std::apply([&](auto&... val){
                ((val = (get_time(ev) == tl)
                        ? get_value(ev)
                        : std::decay_t<decltype(val)>()), ...);
            }, cur_ivals);
        }, next_ievs);
    }

    void exec()
    {
        const bool all_absent = std::apply(
            [](auto&... val){return (is_absent(val) && ...);}, cur_ivals);
        if (all_absent)
            oval.set_abst();
        else
            oval = abst_ext<std::tuple<abst_ext<Ts>...>>(cur_ivals);
    }

    void prod()
    {
        write_multiport(oport1, ttn_event<std::tuple<abst_ext<Ts>...>>(oval, tl));
        wait_until(tl, name());
    }

    void clean() {}

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! The unzipN process with one input and a variable number of outputs
/*! The heterogeneous counterpart of unzipX, and the inverse of zipN: it
 * reads one tuple-valued event and writes each element to its own
 * output, all carrying the tag the event arrived with. An absent input
 * makes every output absent at that tag.
 */
template <class... Ts>
class unzipN : public dde_process,
               public ForSyDe::detail::bindable<unzipN<Ts...>>
{
public:
    DDE_in<std::tuple<abst_ext<Ts>...>> iport1;     ///< port for the input channel
    std::tuple<DDE_out<Ts>...> oport;               ///< tuple of ports for the output channels

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<unzipN<Ts...>>::operator();
    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::apply([](auto&... p){return std::tie(p...);}, oport);}

    //! The constructor requires the module name
    unzipN(sc_module_name _name      ///< process name
          ) : dde_process(_name), iport1("iport1") {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DDE::unzipN";}

private:
    static constexpr std::size_t N = sizeof...(Ts);

    ttn_event<std::tuple<abst_ext<Ts>...>> in_ev;
    std::tuple<abst_ext<Ts>...> out_vals;
    sc_time tl;

    void init() {tl = SC_ZERO_TIME;}

    void prep() {in_ev = iport1.read();}

    void exec()
    {
        tl = get_time(in_ev);
        if (is_absent(get_value(in_ev)))
            out_vals = std::tuple<abst_ext<Ts>...>();
        else
            out_vals = unsafe_from_abst_ext(get_value(in_ev));
    }

    // Indexed rather than std::apply'd, because each output needs the
    // *element* type Ts to name its ttn_event, and abst_ext does not
    // carry the value type as a member typedef to recover it from.
    template <std::size_t... Is>
    void write_outputs(std::index_sequence<Is...>)
    {
        (write_multiport(std::get<Is>(oport),
            ttn_event<std::tuple_element_t<Is, std::tuple<Ts...>>>(
                std::get<Is>(out_vals), tl)), ...);
    }

    void prod()
    {
        write_outputs(std::index_sequence_for<Ts...>{});
        wait_until(tl, name());
    }

    void clean() {}

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! Process constructor for a fan-out process with one input and one output
/*! This class is used to build a fanout processes with one input
 * and one output. The class is parameterized for input and output
 * data-types.
 *
 * This class exist because it is impossible to connect channels
 * directly to ports in SystemC (which may be needed in hierarchical
 * designs). It will be used when it is needed to connect an input
 * port of a module to the input channels of multiple processes (modules).
 */
template <class T>
class fanout : public dde_process,
               public ForSyDe::detail::bindable<fanout<T>>
{
public:
    DDE_in<T> iport1;        ///< port for the input channel
    DDE_out<T> oport1;       ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<fanout<T>>::operator();
    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies and writes the results using the output port
     */
    fanout(sc_module_name _name)  // module name
         : dde_process(_name) { }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DDE::fanout";}

private:
    // Inputs and output variables
    ttn_event<T>* val;

    //Implementing the abstract semantics
    void init()
    {
        val = new ttn_event<T>;
    }

    void prep()
    {
        *val = iport1.read();
    }

    void exec() {}

    void prod()
    {
        write_multiport(oport1, *val);
        // A DDE process advances its local clock to the latest time for
        // which it has complete input information -- for one input, the
        // tag it has just consumed.
        wait_until(get_time(*val), name());
    }

    void clean()
    {
        delete val;
    }
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

}
}

#endif
