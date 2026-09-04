/**********************************************************************           
    * dt_process_constructors.hpp -- Process constructors in the DT   *
    *                                MOC                              *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Providing basic process constructors for modeling      *
    *          discrete-time systems in ForSyDe-SystemC               *
    *                                                                 *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef DT_PROCESS_CONSTRUCTORS_HPP
#define DT_PROCESS_CONSTRUCTORS_HPP

/*! \file dt_process_constructors.hpp
 * \brief Implements the basic process constructors in the DT MoC
 * 
 *  This file includes the basic process constructors used for modeling
 * in the discrete-time model of computation.
 */

#include <systemc>
#include <functional>
#include <tuple>

// Streams a std::vector under FORSYDE_INTROSPECTION (e.g. "ss << values"
// below) via prettyprint.hpp's generic container operator<<, which this
// file otherwise relies on forsyde.hpp having included first.
#include "prettyprint.hpp"

#include "abst_ext.hpp"
#include "dt_process.hpp"

namespace ForSyDe
{

namespace DT
{

using namespace sc_core;

namespace detail
{

//! Record a tuple of port references in one of a process's bound-channel vectors
#ifdef FORSYDE_INTROSPECTION
template <typename Ports>
inline void bind_all(std::vector<PortInfo>& chans, Ports&& ports)
{
    chans.resize(std::tuple_size<typename std::decay<Ports>::type>::value);
    std::apply
    (
        [&](auto&... port)
        {
            std::size_t n{0};
            ((chans[n++].port = &port),...);
        }, ports
    );
}
#endif

//! The plain token type behind a token slot: abst_ext<T> -> T, T -> T
/*! The base variant hands its functions absent-extended tokens and the
 * P/S/T variants hand them bare values, so their output vectors have
 * different element types -- but both have to emit a genuine absent
 * event for the causality delay, never a value-initialised token.
 */
template <typename T> struct token_of {typedef T type;};
template <typename T> struct token_of<abst_ext<T>> {typedef T type;};

//! Shared implementation of the DT state-machine family
/*! A timed process is a blend of an untimed and a synchronous one: it
 * may consume and emit any number of events per evaluation cycle, and it
 * must obey causality -- an output event that reacts to an input event
 * cannot precede it. Jantsch (5.1) enforces that by prepending absent
 * events to each cycle's output:
 *
 *      b_i = <absent>^{K_i} + c_i
 *      K_i = T_i(i) - T_o(i-1) - 1
 *
 * where T_i and T_o are the running counts of tokens consumed and
 * produced. That bookkeeping -- tin, touts, ks -- is the same for every
 * member of this family and lives here, with one T_o per output port.
 *
 * What the four variants actually differ in is one thing: which tokens
 * of the cycle the user's functions get to see. That is \a Derived's
 * read_inputs(), which fills the input pack and returns how many tokens
 * it consumed from the port, and it is the whole of the distinction
 * Jantsch draws in section 5.2.1:
 *
 *   DT::mealy      mealyT   the functions see the absent events too
 *   DT::P::mealy   mealyPT  timer-based: gamma is a time period, the
 *                           interface strips absent events, and the
 *                           functions only ever see present values
 *   DT::S::mealy   mealyST  event-count-based: gamma is a number of
 *                           *nonabsent* events, so the process keeps
 *                           reading until it has that many
 *   DT::T::mealy   mealyTT  event count with a time-out: as S, but it
 *                           gives up after gamma's time period whether
 *                           or not the count was reached
 *
 * Those are four points on one axis -- how far the timing and
 * synchronisation of a process are separated from its behaviour -- not
 * four unrelated constructors, and the more separated they are the
 * cheaper they are to implement in a distributed setting and the less
 * they can express.
 */
template <typename Derived, typename OVals, typename IVals, typename ST>
class fsm_core : public dt_process,
                 public ForSyDe::detail::bindable<Derived>
{
public:
    //! Hides sc_module's positional binding; see bindable
    using ForSyDe::detail::bindable<Derived>::operator();

protected:
    static constexpr std::size_t n_outs = std::tuple_size<OVals>::value;

    OVals ovals;    ///< output tokens, one vector per output port
    IVals ivals;    ///< input tokens, one vector per input port

    ST stval;       ///< the current state
    ST nsval;       ///< the next state, as computed by this cycle
    ST init_st;     ///< the initial state

    std::size_t tin;                          ///< T_i: tokens consumed so far
    std::array<std::size_t, n_outs> touts;    ///< T_o, one per output port
    std::array<std::size_t, n_outs> ks;       ///< K_i, one per output port

    fsm_core(sc_module_name _name,   ///< process name
             const ST& init_st              ///< initial state
             ) : dt_process(_name), init_st(init_st)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        // "_gamma_func", not the scalar variants' old "_gamma_func": the other
        // two arguments are _ns_func and _od_func, the MN variants and
        // every UT state machine already spell it this way, and the four
        // of them have to agree now that one core registers all of them.
        arg_vec.push_back(std::make_tuple("_gamma_func",func_name+std::string("_gamma_func")));
        arg_vec.push_back(std::make_tuple("_ns_func",func_name+std::string("_ns_func")));
        arg_vec.push_back(std::make_tuple("_od_func",func_name+std::string("_od_func")));
        std::stringstream ss;
        ss << init_st;
        arg_vec.push_back(std::make_tuple("init_st",ss.str()));
#endif
    }

private:
    Derived& self() {return static_cast<Derived&>(*this);}

    //! Emit one output port's share of this cycle: its delay, then its tokens
    template <typename Port, typename Vec>
    void emit(Port& port, const Vec& val, std::size_t n)
    {
        typedef typename token_of<typename Vec::value_type>::type T;
        for (std::size_t i=0; i<ks[n]; i++)
            write_multiport(port, abst_ext<T>());
        write_vec_multiport(port, val);
        touts[n] += ks[n] + val.size();
    }

    //Implementing the abstract semantics
    void init()
    {
        stval = init_st;
        tin = 0;
        touts.fill(0);
        ks.fill(0);
    }

    void clean() {}

    void prep() {tin += self().read_inputs();}

    void prod()
    {
        for (std::size_t i=0; i<n_outs; i++)
            ks[i] = std::max((int)tin-(int)touts[i]-1, 0);

        std::apply([&](auto&... port){
            std::apply([&](auto&... val){
                std::size_t n{0};
                (emit(port, val, n++), ...);
            }, ovals);
        }, self().out_ports());

        std::apply([](auto&... val){(val.clear(), ...);}, ovals);
        std::apply([](auto&... val){(val.clear(), ...);}, ivals);
    }

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        bind_all(boundInChans, self().in_ports());
        bind_all(boundOutChans, self().out_ports());
    }
#endif
};

}

//! Process constructor for a delay element
/*! This class is used to build the most basic sequential process which
 * is a delay element. Given an initial value, it inserts this value at
 * the beginning of output stream and passes the rest of the inputs to
 * its output untouched. The class is parameterized for its input/output
 * data-type.
 * 
 * It is mandatory to include at least one delay element in all feedback
 * loops since combinational loops are forbidden in ForSyDe.
 */
template <class T>
class delay : public dt_process,
              public ForSyDe::detail::bindable<delay<T>>
{
public:
    DT_in<T>  iport1;       ///< port for the input channel
    DT_out<T> oport1;        ///< port for the output channel

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
          abst_ext<T> init_val      ///< initial value
          ) : dt_process(_name), iport1("iport1"), oport1("oport1"),
              init_val(init_val)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << init_val;
        arg_vec.push_back(std::make_tuple("init_val", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DT::delay";}
    
private:
    // Initial value
    abst_ext<T> init_val;
    
    // Inputs and output variables
    abst_ext<T>* val;
    
    //Implementing the abstract semantics
    void init()
    {
        val = new abst_ext<T>;
        write_multiport(oport1, init_val);
    }
    
    void prep()
    {
        *val = iport1.read();
    }
    
    void exec() {}
    
    void prod()
    {
        write_multiport(oport1, *val);
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

//! Process constructor for a n-delay element
/*! This class is used to build a sequential process similar to dalay
 * but with an extra initial variable which sets the number of delay
 * elements (initial tokens). Given an initial value, it inserts the
 * initial value n times at the the beginning of output stream and
 * passes the rest of the inputs to its output untouched. The class is
 * parameterized for its input/output data-type.
 */
template <class T>
class delayn : public dt_process,
               public ForSyDe::detail::bindable<delayn<T>>
{
public:
    DT_in<T>  iport1;       ///< port for the input channel
    DT_out<T> oport1;        ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<delayn<T>>::operator();
    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which inserts the initial elements,
     * reads data from its input port, and writes the results using the
     * output port.
     */
    delayn(sc_module_name _name,    ///< process name
           abst_ext<T> init_val,   ///< initial value
           unsigned int ns         ///< number of delay elements
          ) : dt_process(_name), iport1("iport1"), oport1("oport1"),
              init_val(init_val), ns(ns)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << init_val;
        arg_vec.push_back(std::make_tuple("init_val", ss.str()));
        arg_vec.push_back(std::make_tuple("ns", std::to_string(ns)));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DT::delayn";}
    
private:
    // Initial value
    abst_ext<T> init_val;
    unsigned int ns;
    
    // Inputs and output variables
    abst_ext<T>* val;
    
    //Implementing the abstract semantics
    void init()
    {
        val = new abst_ext<T>;
        for (unsigned int i=0; i<ns; i++)
            write_multiport(oport1, abst_ext<T>());
    }
    
    void prep()
    {
        *val = iport1.read();
    }
    
    void exec() {}
    
    void prod()
    {
        write_multiport(oport1, *val);
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

//! Process constructor for a mealy machine
/*! This class is used to build a timed Mealy state machine.
 * Given a partitioning function, a next-state function, an output decoding
 * function, and an initial state, it creates a timed Mealy process.
 */
template <class IT, class ST, class OT>
class mealy : public detail::fsm_core<mealy<IT,ST,OT>,
                                    std::tuple<std::vector<abst_ext<OT>>>,
                                    std::tuple<std::vector<abst_ext<IT>>>, ST>
{
    typedef detail::fsm_core<mealy<IT,ST,OT>,
                             std::tuple<std::vector<abst_ext<OT>>>,
                             std::tuple<std::vector<abst_ext<IT>>>, ST> base;
    friend base;
public:
    DT_in<IT>  iport1;        ///< port for the input channel
    DT_out<OT> oport1;        ///< port for the output channel
    
    //! Type of the partitioning function to be passed to the process constructor
    typedef std::function<void(size_t&, const ST&)> gamma_functype;
    
    //! Type of the next-state function to be passed to the process constructor
    typedef std::function<void(ST&, 
                                const ST&,
                                const std::vector<abst_ext<IT>>&)> ns_functype;
    
    //! Type of the output-decoding function to be passed to the process constructor
    typedef std::function<void(std::vector<abst_ext<OT>>&, 
                                const ST&,
                                const std::vector<abst_ext<IT>>&)> od_functype;
    
    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented functions to the input and current
     * state and writes the results using the output port
     */
    mealy(sc_module_name _name,    ///< The module name
           gamma_functype _gamma_func,    ///< The input partitioning function
           ns_functype _ns_func,    ///< The next_state function
           od_functype _od_func,    ///< The output-decoding function
           ST init_st               ///< Initial state
          ) : base(_name, init_st), iport1("iport1"), oport1("oport1"), _gamma_func(_gamma_func), _ns_func(_ns_func),
              _od_func(_od_func) {}
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "DT::mealy";}
    
private:
    //! The functions passed to the process constructor
    gamma_functype _gamma_func;
    ns_functype _ns_func;
    od_functype _od_func;

public:
    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}
private:

    // mealyT: gamma is a time period, and the functions see every token
    // of it, absent events included.
    std::size_t read_inputs()
    {
        std::size_t itoks;
        _gamma_func(itoks, this->stval);
        auto& in = std::get<0>(this->ivals);
        in.resize(itoks);
        for (std::size_t i=0; i<itoks; i++)
            in[i] = iport1.read();
        return itoks;
    }

    void exec()
    {
        _ns_func(this->nsval, this->stval, std::get<0>(this->ivals));
        _od_func(std::get<0>(this->ovals), this->stval, std::get<0>(this->ivals));
        this->stval = this->nsval;
    }
};

//! Process constructor for a Mealy machine
/*! This class is used to build a finite state machine of type Mealy.
 * Given an initial state, a next-state function, and an output decoding
 * function it creates a Mealy process.
 */
template<typename TO_tuple, typename TI_tuple, typename TS> class mealyMN;

template <typename... TOs, typename... TIs, typename TS>
class mealyMN<std::tuple<TOs...>,std::tuple<TIs...>,TS> : public detail::fsm_core<mealyMN<std::tuple<TOs...>,std::tuple<TIs...>,TS>,
                                    std::tuple<std::vector<abst_ext<TOs>>...>,
                                    std::tuple<std::vector<abst_ext<TIs>>...>, TS>
{
    typedef detail::fsm_core<mealyMN<std::tuple<TOs...>,std::tuple<TIs...>,TS>,
                             std::tuple<std::vector<abst_ext<TOs>>...>,
                             std::tuple<std::vector<abst_ext<TIs>>...>, TS> base;
    friend base;
public:
    std::tuple<DT_in<TIs>...>  iport;///< tuple of ports for the input channels
    std::tuple<DT_out<TOs>...> oport;///< tuple of ports for the output channels
    
    //! Type of the partitioning function to be passed to the process constructor
    typedef std::function<void(size_t&,
                                const TS&)> gamma_functype;
    
    //! Type of the next-state function to be passed to the process constructor
    typedef std::function<void(TS&,
                                const TS&,
                                const std::tuple<std::vector<abst_ext<TIs>>...>&)> ns_functype;
    
    //! Type of the output-decoding function to be passed to the process constructor
    typedef std::function<void(std::tuple<std::vector<abst_ext<TOs>>...>&,
                                const TS&,
                                const std::tuple<std::vector<abst_ext<TIs>>...>&)> od_functype;
    
    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented functions to the input and current
     * state and writes the results using the output port
     */
    mealyMN(sc_module_name _name,        ///< The module name
            const gamma_functype& _gamma_func,  ///< The partitioning function
            const ns_functype& _ns_func,        ///< The next_state function
            const od_functype& _od_func,        ///< The output-decoding function
            const TS& init_st   ///< Initial state
            ) : base(_name, init_st), _gamma_func(_gamma_func), _ns_func(_ns_func),
              _od_func(_od_func) {}
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "DT::mealyMN";}
    
private:
    //! The functions passed to the process constructor
    gamma_functype _gamma_func;
    ns_functype _ns_func;
    od_functype _od_func;

public:
    auto in_ports()  {return std::apply([](auto&... p){return std::tie(p...);}, iport);}
    auto out_ports() {return std::apply([](auto&... p){return std::tie(p...);}, oport);}
private:

    // mealyT, several inputs: _gamma_func gives one time period and every
    // input port is read for it.
    std::size_t read_inputs()
    {
        std::size_t itoks;
        _gamma_func(itoks, this->stval);
        std::apply([&](auto&... ival){(ival.resize(itoks), ...);}, this->ivals);
        std::apply([&](auto&... inport){
            std::apply([&](auto&... ival){
                ([&](auto& port, auto& vals){
                    for (auto it=vals.begin(); it!=vals.end(); it++)
                        *it = port.read();
                }(inport, ival), ...);
            }, this->ivals);
        }, iport);
        return itoks;
    }

    void exec()
    {
        _ns_func(this->nsval, this->stval, this->ivals);
        _od_func(this->ovals, this->stval, this->ivals);
        this->stval = this->nsval;
    }
};

//! Process constructor for a constant source process
/*! This class is used to build a souce process with constant output.
 * Its main purpose is to be used in test-benches.
 * 
 * This class can directly be instantiated to build a process.
 */
template <class T>
class constant : public dt_process,
                 public ForSyDe::detail::bindable<constant<T>>
{
public:
    DT_out<T> oport1;            ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<constant<T>>::operator();
    auto out_ports() {return std::tie(oport1);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * and writes the result using the output port
     */
    constant(sc_module_name _name,      ///< The module name
              abst_ext<T> init_val,     ///< The constant output value
              unsigned long long take=0 ///< number of tokens produced (0 for infinite)
             ) : dt_process(_name), oport1("oport1"),
                 init_val(init_val), take(take)
                 
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << init_val;
        arg_vec.push_back(std::make_tuple("init_val", ss.str()));
        ss.str("");
        ss << take;
        arg_vec.push_back(std::make_tuple("take", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DT::constant";}
    
private:
    abst_ext<T> init_val;
    unsigned long long take;    // Number of tokens produced
    
    unsigned long long tok_cnt;
    bool infinite;
    
    //Implementing the abstract semantics
    void init()
    {
        if (take==0) infinite = true;
        tok_cnt = 0;
    }
    
    void prep() {}
    
    void exec() {}
    
    void prod()
    {
        if (tok_cnt++ < take || infinite)
            write_multiport(oport1, init_val);
        else wait();
    }
    
    void clean() {}

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
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
class source : public dt_process,
               public ForSyDe::detail::bindable<source<T>>
{
public:
    DT_out<T> oport1;        ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<source<T>>::operator();
    auto out_ports() {return std::tie(oport1);}
    
    //! Type of the function to be passed to the process constructor
    typedef std::function<void(abst_ext<T>&, const abst_ext<T>&)> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * and writes the result using the output port
     */
    source(sc_module_name _name,   ///< The module name
           functype _func,         ///< function to be passed
           abst_ext<T> init_val,    ///< Initial state
           unsigned long long take=0 ///< number of tokens produced (0 for infinite)
          ) : dt_process(_name), oport1("oport1"),
              init_st(init_val), take(take), _func(_func)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_func",func_name+std::string("_func")));
        std::stringstream ss;
        ss << init_val;
        arg_vec.push_back(std::make_tuple("init_val", ss.str()));
        ss.str("");
        ss << take;
        arg_vec.push_back(std::make_tuple("take", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DT::source";}
    
private:
    abst_ext<T> init_st;        // The current state
    unsigned long long take;    // Number of tokens produced
    
    abst_ext<T>* cur_st;        // The current state of the process
    unsigned long long tok_cnt;
    bool infinite;
    
    //! The function passed to the process constructor
    functype _func;
    
    //Implementing the abstract semantics
    void init()
    {
        cur_st = new abst_ext<T>;
        *cur_st = init_st;
        write_multiport(oport1, *cur_st);
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
            write_multiport(oport1, *cur_st);
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
 * of the vector and outputs the values on the given times and absents
 * otherwise.
 */
template <class T>
class vsource : public dt_process,
                public ForSyDe::detail::bindable<vsource<T>>
{
public:
    DT_out<T> oport1;     ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<vsource<T>>::operator();
    auto out_ports() {return std::tie(oport1);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which writes the result using the output
     * port.
     */
    vsource(sc_module_name _name,           ///< The module name
            const std::vector<std::tuple<size_t,T>>& in_vec  ///< Initial vector
            ) : dt_process(_name), in_vec(in_vec)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << in_vec;
        arg_vec.push_back(std::make_tuple("in_vec", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DT::vsource";}
    
private:
    std::vector<std::tuple<size_t,T>> in_vec;
    
    typename std::vector<std::tuple<size_t,T>>::iterator it;
    size_t local_time;
    
    //Implementing the abstract semantics
    void init()
    {
        it = in_vec.begin();
        local_time = 0;
    }
    
    void prep() {}
    
    void exec() {}
    
    void prod()
    {
        if (std::get<0>(*it) > local_time)
            write_multiport(oport1, abst_ext<T>());
        else
        {
            write_multiport(oport1, std::get<1>(*it));
            if (it != in_vec.end()-1) it++; else wait();
        }
            
        local_time++;
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
class sink : public dt_process,
             public ForSyDe::detail::bindable<sink<T>>
{
public:
    DT_in<T> iport1;         ///< port for the input channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<sink<T>>::operator();
    auto in_ports()  {return std::tie(iport1);}
    
    //! Type of the function to be passed to the process constructor
    typedef std::function<void(const abst_ext<T>&)> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * in each cycle.
     */
    sink(sc_module_name _name,      ///< process name
         functype _func             ///< function to be passed
        ) : dt_process(_name), iport1("iport1"), _func(_func)
            
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_func",func_name+std::string("_func")));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DT::sink";}
    
private:
    abst_ext<T>* val;         // The current state of the process

    //! The function passed to the process constructor
    functype _func;
    
    //Implementing the abstract semantics
    void init()
    {
        val = new abst_ext<T>;
    }
    
    void prep()
    {
        *val = iport1.read();
    }
    
    void exec()
    {
        _func(*val);
    }
    
    void prod() {}
    
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

//! Process constructor for a multi-input print process
/*! This class is used to build a sink process which has a multi-port input.
 * Its main purpose is to be used in test-benches.
 * 
 * The resulting process prints the sampled data as a trace in the
 * standard output.
 */
template <class ITYP>
class printSigs : public sc_module,
                  public ForSyDe::detail::bindable<printSigs<ITYP>>
{
public:
    sc_fifo_in<ITYP> iport;         ///< multi-port for the input channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<printSigs<ITYP>>::operator();
    auto in_ports()  {return std::tie(iport);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * in each cycle.
     */
    printSigs(sc_module_name _name     ///< Process name
              ):sc_module(_name)
    {
        SC_THREAD(worker);
    }

private:

    //! The main and only execution thread of the module
    void worker()
    {
        // write the header
        for (int i=0;i<iport.size();i++)
            std::cout << " " << name() << "(" << i << ")";
        std::cout << std::endl;
        // start reading from the ports
        ITYP in_val[iport.size()];
        while (1)
        {
            for (int i=0;i<iport.size();i++)
                in_val[i] = iport[i]->read();
            // print one line
            for (int i=0;i<iport.size();i++)
                std::cout << " " << in_val[i];
            std::cout << std::endl;
        }
    }
};

//! The zips process with two inputs and one output
/*! This process "zips" two incoming signals into one signal of tuples.
 */
template <class T1, class T2>
class zips : public dt_process,
             public ForSyDe::detail::bindable<zips<T1,T2>>
{
public:
    DT_in<T1> iport1;        ///< port for the input channel 1
    DT_in<T2> iport2;        ///< port for the input channel 2
    DT_out<std::tuple<std::vector<abst_ext<T1>>,std::vector<abst_ext<T2>>>> oport1;///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<zips<T1,T2>>::operator();
    auto in_ports()  {return std::tie(iport1,iport2);}
    auto out_ports() {return std::tie(oport1);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * zips them together and writes the results using the output port
     */
    zips(sc_module_name _name,  ///< The module name
        size_t itoks            ///< The input/output token rate
        ): dt_process(_name),
        iport1("iport1"), iport2("iport2"), oport1("oport1"), itoks(itoks)
    { }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DT::zips";}
    
private:
    // intermediate values
    std::vector<abst_ext<T1>> ival1;
    std::vector<abst_ext<T2>> ival2;

    size_t itoks;
    
    void init()
    {
        ival1.resize(itoks);
        ival2.resize(itoks);
    }
    
    void prep()
    {
        for (size_t i=0;i<itoks;i++)
        {
            ival1[i] = iport1.read();
            ival2[i] = iport2.read();
        }
    }
    
    void exec() {}
    
    void prod()
    {
        // write sufficient absent events to the output to ensure causality
        for (size_t i=0;i<itoks-1;i++)
            write_multiport(oport1,
                abst_ext<std::tuple<std::vector<abst_ext<T1>>,std::vector<abst_ext<T2>>>>()
            );
        
        // Write the zipped output
        write_multiport(oport1,std::make_tuple(ival1,ival2));
    }
    
    void clean()
    {}
    
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! The zip process with two data inputs, one control input, and one output
/*! This process "zips" two incoming signals into one signal of tuples.
 */
template <class T1, class T2, class TC>
class zip : public dt_process,
            public ForSyDe::detail::bindable<zip<T1,T2,TC>>
{
public:
    DT_in<T1> iport1;       ///< port for the input channel 1
    DT_in<T2> iport2;       ///< port for the input channel 2
    DT_in<TC> iport3;   ///< port for the control channel
    DT_out<std::tuple<std::vector<abst_ext<T1>>,std::vector<abst_ext<T2>>>> oport1;///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<zip<T1,T2,TC>>::operator();
    auto in_ports()  {return std::tie(iport1,iport2,iport3);}
    auto out_ports() {return std::tie(oport1);}

    //! Type of the partitioning function to be passed to the process constructor
    typedef std::function<void(size_t&, const TC&)> gamma_functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input ports,
     * zips them together and writes the results using the output port
     */
    zip(sc_module_name _name,   ///< The module name
        gamma_functype _gamma_func    ///< The input partitioning function
    ): dt_process(_name), iport1("iport1"),
        iport2("iport2"), iport3("iport3"), oport1("oport1"), _gamma_func(_gamma_func)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_gamma_func",func_name+std::string("_gamma_func")));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DT::zip";}
    
private:
    //! The functions passed to the process constructor
    gamma_functype _gamma_func;

    // intermediate values
    std::vector<abst_ext<T1>> ival1;
    std::vector<abst_ext<T2>> ival2;
    std::vector<abst_ext<TC>> ival3;

    size_t itoks;
    TC k;
    
    void init()
    {
        k = TC();
    }
    
    void prep()
    {
        _gamma_func(itoks, k);
        ival1.resize(itoks);
        ival2.resize(itoks);
        ival3.resize(itoks);
        for (size_t i=0;i<itoks;i++)
        {
            ival1[i] = iport1.read();
            ival2[i] = iport2.read();
            ival3[i] = iport3.read();
        }

        // update k for the next iteration
        if (ival3[0].is_absent())
            SC_REPORT_ERROR(name(), "Absent event received in zip control port");
        else
            k = unsafe_from_abst_ext(ival3[0]);

    }
    
    void exec() {}
    
    void prod()
    {
        // write sufficient absent events to the output to ensure causality
        for (size_t i=0;i<itoks-1;i++)
            write_multiport(oport1,
                abst_ext<std::tuple<std::vector<abst_ext<T1>>,std::vector<abst_ext<T2>>>>()
            );
        // Write the zipped output
        write_multiport(oport1,std::make_tuple(ival1,ival2));
    }
    
    void clean()
    {}
    
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! The zip process with variable number of inputs and one output
/*! This process "zips" the incoming signals into one signal of tuples.
 */
template <class TC, class... ITYPs>
class zipN : public dt_process,
             public ForSyDe::detail::bindable<zipN<TC,ITYPs...>>
{
public:
    std::tuple<DT_in<ITYPs>...> iport;  ///< tuple of ports for the input channels
    DT_in<TC> iport2;               ///< port for the control channel
    DT_out<std::tuple<ITYPs...> > oport1;///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<zipN<TC,ITYPs...>>::operator();
    auto in_ports()  {return std::tie(iport,iport2);}
    auto out_ports() {return std::tie(oport1);}

    //! Type of the partitioning function to be passed to the process constructor
    typedef std::function<void(size_t&, const TC&)> gamma_functype;
    
    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input ports,
     * zips them together and writes the results using the output port
     */
    zipN(sc_module_name _name,  ///< The module name
        gamma_functype _gamma_func    ///< The input partitioning function
    ):  dt_process(_name), _gamma_func(_gamma_func), iport2("iport2"), oport1("oport1")
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_gamma_func",func_name+std::string("_gamma_func")));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DT::zipN";}

private:
   //! The functions passed to the process constructor
    gamma_functype _gamma_func;

    // intermediate values
    std::tuple<std::vector<abst_ext<ITYPs>>...> ival;
    std::vector<abst_ext<TC>> ival3;

    size_t itoks;
    TC k;
    
    void init()
    {
        k = TC();
    }
    
    void prep()
    {
        _gamma_func(itoks, k);
        std::apply([&](auto&... args) {
            (..., args.resize(itoks));
        }, ival);
        std::apply([&](auto&... port) {
            std::apply([&](auto&... val) {
                (..., (val = port.read()));
            }, ival);
        }, iport);
        // update k for the next iteration
        k = ival3[0];
    }
    
    void exec() {}
    
    void prod()
    {
        // write sufficient absent events to the output to ensure causality
        for (size_t i=0;i<itoks-1;i++)
            write_multiport(oport1,abst_ext<std::tuple<abst_ext<ITYPs>>...>());
        // Write the zipped output
        write_multiport(oport1,ival);
    }
    
    void clean()
    {}

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
class unzip : public dt_process,
              public ForSyDe::detail::bindable<unzip<T1,T2>>
{
public:
    DT_in<std::tuple<abst_ext<T1>,abst_ext<T2>>> iport1;///< port for the input channel
    DT_out<T1> oport1;        ///< port for the output channel 1
    DT_out<T2> oport2;        ///< port for the output channel 2

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<unzip<T1,T2>>::operator();
    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1,oport2);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input ports,
     * unzips them and writes the results using the output ports
     */
    unzip(sc_module_name _name      ///< process name
        ): dt_process(_name), iport1("iport1"), oport1("oport1"), oport2("oport2")
    {}
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DT::unzip";}
private:
    // intermediate values
    abst_ext<std::tuple<abst_ext<T1>,abst_ext<T2>>>* in_val;
    
    void init()
    {
        in_val = new abst_ext<std::tuple<abst_ext<T1>,abst_ext<T2>>>();
    }
    
    void prep()
    {
        *in_val = iport1.read();
    }
    
    void exec() {}
    
    void prod()
    {
        if (in_val->is_absent())
        {
            write_multiport(oport1,abst_ext<T1>());  // write to the output 1
            write_multiport(oport2,abst_ext<T2>());  // write to the output 2
        }
        else
        {
            write_multiport(oport1,std::get<0>(in_val->unsafe_from_abst_ext()));  // write to the output 1
            write_multiport(oport2,std::get<1>(in_val->unsafe_from_abst_ext()));  // write to the output 2
        }
    }
    
    void clean()
    {
        delete in_val;
    }
    
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! The unzip process with one input and variable number of outputs
/*! This process "unzips" the incoming signal into a tuple of signals.
 */
template <class... Ts>
class unzipN : public dt_process,
               public ForSyDe::detail::bindable<unzipN<Ts...>>
{
public:
    DT_in<std::tuple<abst_ext<Ts>...>> iport1;///< port for the input channel
    std::tuple<DT_out<Ts>...> oport;///< tuple of ports for the output channels

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<unzipN<Ts...>>::operator();
    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::apply([](auto&... p){return std::tie(p...);}, oport);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * unzips it and writes the results using the output ports
     */
    unzipN(sc_module_name _name)
          :dt_process(_name), iport1("iport1")
    { }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DT::unzipN";}
private:
    // intermediate values
    abst_ext<std::tuple<abst_ext<Ts>...>>* in_val;
    
    void init()
    {
        in_val = new abst_ext<std::tuple<abst_ext<Ts>...>>;
    }
    
    void prep()
    {
        *in_val = iport1.read();
    }
    
    void exec() {}
    
    void prod()
    {
        if (in_val->is_absent())
        {
            std::tuple<abst_ext<Ts>...> all_abs;
            fifo_tuple_write<Ts...>(all_abs, oport);
        }
        else
        {
            fifo_tuple_write<Ts...>(in_val->unsafe_from_abst_ext(), oport);
        }
    }
    
    void clean()
    {
        delete in_val;
    }
    
    template<size_t N,class R,  class T>
    struct fifo_write_helper
    {
        static void write(const R& vals, T& t)
        {
            fifo_write_helper<N-1,R,T>::write(vals,t);
            std::get<N>(t).write(std::get<N>(vals));
        }
    };

    template<class R, class T>
    struct fifo_write_helper<0,R,T>
    {
        static void write(const R& vals, T& t)
        {
            std::get<0>(t).write(std::get<0>(vals));
        }
    };

    template<class... T>
    void fifo_tuple_write(const std::tuple<abst_ext<T>...>& vals,
                             std::tuple<DT_out<T>...>& ports)
    {
        fifo_write_helper<sizeof...(T)-1,
                          std::tuple<abst_ext<T>...>,
                          std::tuple<DT_out<T>...>>::write(vals,ports);
    }

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
    
    template<size_t N, class T>
    struct register_ports_helper
    {
        static void reg_port(std::vector<PortInfo>& boundChans, T& t)
        {
            register_ports_helper<N-1,T>::reg_port(boundChans,t);
            boundChans[N].port = &std::get<N>(t);
        }
    };

    template<class T>
    struct register_ports_helper<0,T>
    {
        static void reg_port(std::vector<PortInfo>& boundChans, T& t)
        {
            boundChans[0].port = &std::get<0>(t);
        }
    };

    template<class... T>
    void register_ports(std::vector<PortInfo>& boundChans,
                             std::tuple<DT_out<T>...>& ports)
    {
        register_ports_helper<sizeof...(T)-1,
                              std::tuple<DT_out<T>...>&>::reg_port(boundChans,ports);
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
class fanout : public dt_process,
               public ForSyDe::detail::bindable<fanout<T>>
{
public:
    DT_in<T> iport1;        ///< port for the input channel
    DT_out<T> oport1;       ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<fanout<T>>::operator();
    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies and writes the results using the output port
     */
    fanout(sc_module_name _name)  // module name
         : dt_process(_name) { }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DT::fanout";}
    
private:
    // Inputs and output variables
    abst_ext<T>* val;
    
    //Implementing the abstract semantics
    void init()
    {
        val = new abst_ext<T>;
    }
    
    void prep()
    {
        *val = iport1.read();
    }
    
    void exec() {}
    
    void prod()
    {
        write_multiport(oport1, *val);
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

//! Deduction guides: the template arguments, from the constructor call
/*! DT carries abst_ext<T>, like SY, so the same arg_t reasoning applies
 * to delay and sink. mealy's IT and OT are one layer deeper: ns_functype
 * and od_functype carry them as std::vector<abst_ext<T>>, a whole tick's
 * worth of events rather than one, so getting to T needs arg_t's single
 * unwrap and then one more -- token_value is deliberately not recursive
 * (see its definition in binding.hpp for why), so the second layer is
 * named here rather than assumed away. mealy's state comes from the
 * initial value directly. zip and zips take no argument that mentions a
 * single token type, so they keep their template arguments explicit --
 * as do the _p/_s/_t mealy variants, which this file does not declare.
 */
template <class T> delay(sc_module_name, abst_ext<T>) -> delay<T>;
template <class F> sink(sc_module_name, F) -> sink<ForSyDe::detail::arg_t<0,F>>;
template <class G, class NS, class OD, class ST>
mealy(sc_module_name, G, NS, OD, ST)
    -> mealy<typename ForSyDe::detail::token_value<ForSyDe::detail::arg_t<2,NS>>::type,
             ST,
             typename ForSyDe::detail::token_value<ForSyDe::detail::arg_t<0,OD>>::type>;

}
}

#endif
