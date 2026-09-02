/**********************************************************************           
    * ut_process_constructors.hpp -- Process constructors in the      *
    *                                untimed MOC.                     *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Providing basic process constructors for modeling      *
    *          UT systems in ForSyDe-SystemC                          *
    *                                                                 *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef UT_PROCESS_CONSTRUCTORS_HPP
#define UT_PROCESS_CONSTRUCTORS_HPP

/*! \file ut_process_constructors.hpp
 * \brief Implements the basic process constructors in the UT MoC
 * 
 *  This file includes the basic process constructors used for modeling
 * in the untimed model of computation.
 */

#include <systemc>
#include <functional>
#include <tuple>
#include <vector>
// std::array (the rate packs) and std::tuple_size/std::decay (the shared
// cores' port and token plumbing) -- used directly here rather than left
// to whichever other header happens to drag them in.
#include <array>
#include <type_traits>

#include "ut_process.hpp"

namespace ForSyDe
{

namespace UT
{

using namespace sc_core;

namespace detail
{

#ifdef FORSYDE_INTROSPECTION
//! Record a tuple of port references in one of a process's bound-channel vectors
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

//! Give each vector in a token pack the length of its port's rate
template <typename Vals, typename Toks>
inline void resize_all(Vals& vals, const Toks& toks)
{
    std::apply([&](auto&... val){
        std::apply([&](auto&... tok){
            (val.resize(tok), ...);
        }, toks);
    }, vals);
}

//! Read one firing's worth of tokens from one port
template <typename Port, typename Vec>
inline void read_vec(Port& port, Vec& vals)
{
    for (auto it=vals.begin();it!=vals.end();it++)
        *it = port.read();
}

//! Read one firing's worth of tokens from each of a tuple of input ports
template <typename Ports, typename Vals>
inline void read_all(Ports&& ports, Vals& vals)
{
    std::apply([&](auto&... port){
        std::apply([&](auto&... val){
            (read_vec(port, val), ...);
        }, vals);
    }, ports);
}

//! Write one firing's worth of tokens to each of a tuple of output ports
template <typename Ports, typename Vals>
inline void write_all(Ports&& ports, const Vals& vals)
{
    std::apply([&](auto&... port){
        std::apply([&](auto&... val){
            (write_vec_multiport(port, val), ...);
        }, vals);
    }, ports);
}

//! Shared implementation of the UT state-machine family
/*! scan, scand, moore, mooreMN, mealy and mealyMN all carry a state and
 * all consume a state-dependent number of tokens: the partitioning
 * function gamma is handed the current state and answers how many tokens
 * to read this cycle. That is Jantsch's gamma(w_i) -- the untimed
 * constructors are defined with a consumption rate that is a function of
 * the state, which is what separates them from their SDF counterparts,
 * where the rate is a constant fixed at construction.
 *
 * This core owns the state, the first-cycle flag, the read and write
 * loops and the _gamma_func/_ns_func argument pair that all six share.
 * \a Derived supplies:
 *   - in_ports() and out_ports();
 *   - resize_inputs(), which calls its own gamma -- the arity variants
 *     take an unsigned int and the MN ones a std::array, so the call
 *     itself cannot be hoisted;
 *   - exec(), where the next-state and output-decoding functions are
 *     applied in the order that distinguishes the constructors;
 *   - the remaining introspection arguments, since scan and scand have
 *     no output-decoding function to register.
 *
 * \a EmitsBeforeFirstRead marks the constructors whose first evaluation
 * cycle emits without consuming: scand, moore and mooreMN. For scand
 * that is its whole definition -- Jantsch (3.8) gives scandU as scanU
 * with the initial state prepended. For the Moore machines it is the
 * initial output that makes them usable in a feedback loop.
 *
 * Those two are analogous, not identical, and the difference matters:
 * a scand emits the initial *state*, a Moore machine emits od(w0), and
 * the scan family has no output decoder at all. tests/fsm_semantics
 * pins both, with a non-identity od so that they cannot be confused.
 */
template <typename Derived, typename OVals, typename IVals, typename ST,
          bool EmitsBeforeFirstRead = false>
class fsm_core : public ut_process
{
protected:
    OVals ovals;    ///< output tokens, one vector per output port
    IVals ivals;    ///< input tokens, one vector per input port

    ST stval;       ///< the current state
    ST nsval;       ///< the next state, as computed by this cycle
    ST init_st;     ///< the initial state

    //! True until the first evaluation cycle has run
    bool first_run;

    fsm_core(const sc_module_name& _name,   ///< process name
             const ST& init_st              ///< initial state
             ) : ut_process(_name), init_st(init_st)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_gamma_func",func_name+std::string("_gamma_func")));
        arg_vec.push_back(std::make_tuple("_ns_func",func_name+std::string("_ns_func")));
#endif
    }

private:
    Derived& self() {return static_cast<Derived&>(*this);}

    //Implementing the abstract semantics
    void init()
    {
        stval = init_st;
        first_run = true;
    }

    void clean() {}

    void prep()
    {
        if constexpr (EmitsBeforeFirstRead)
            if (first_run) return;      // nothing consumed on the first cycle
        self().resize_inputs();
        read_all(self().in_ports(), ivals);
    }

    void prod()
    {
        // An empty output token pack means this constructor does not
        // write a vector per output port -- scan and scand emit their
        // state as a single event -- and overrides prod() itself. The
        // test cannot be left to the override alone: a virtual member of
        // a class template is instantiated along with the class whether
        // or not anything calls it, so without `if constexpr` the write
        // loop below would be instantiated for them too, and fail on the
        // mismatch between an empty pack and a one-port tuple.
        if constexpr (std::tuple_size<OVals>::value > 0)
        {
            write_all(self().out_ports(), ovals);
            // An untimed output has no declared rate: the user function
            // appends whatever this cycle produced, so the vectors are
            // emptied again ready for the next one.
            std::apply([](auto&... val){(val.clear(), ...);}, ovals);
        }
    }

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        bind_all(boundInChans, self().in_ports());
        bind_all(boundOutChans, self().out_ports());
    }
#endif
};

//! Shared implementation of the UT combinational (comb*) family
/*! The untimed counterpart of ForSyDe::SDF::detail::comb_core, and it
 * differs from it in exactly the way the two MoCs differ: an input port
 * has a fixed consumption rate and so its vector is sized once, up
 * front, but an output has no declared rate at all. The user function
 * appends however many tokens this firing produced, prod() writes all of
 * them, and the output vector is emptied again ready for the next
 * firing. Sizing the outputs is the whole of what SDF's core does that
 * this one must not.
 *
 * \a Derived supplies in_ports(), out_ports() and exec(), and adds its
 * own consumption rates to arg_vec.
 */
template <typename Derived, typename OVals, typename IVals>
class comb_core : public ut_process
{
protected:
    static constexpr std::size_t n_ins = std::tuple_size<IVals>::value;

    //! consumption rates, one per input port
    std::array<size_t,n_ins> itoks;

    OVals ovals;    ///< output tokens, one vector per output port
    IVals ivals;    ///< input tokens, one vector per input port

    comb_core(const sc_module_name& _name,              ///< process name
              const std::array<size_t,n_ins>& itoks     ///< consumption rates
              ) : ut_process(_name), itoks(itoks)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_func",func_name+std::string("_func")));
#endif
    }

private:
    Derived& self() {return static_cast<Derived&>(*this);}

    //Implementing the abstract semantics
    void init() {resize_all(ivals, itoks);}

    void clean() {}

    void prep() {read_all(self().in_ports(), ivals);}

    void prod()
    {
        write_all(self().out_ports(), ovals);
        std::apply([](auto&... val){(val.clear(), ...);}, ovals);
    }

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        bind_all(boundInChans, self().in_ports());
        bind_all(boundOutChans, self().out_ports());
    }
#endif
};

//! Shared implementation of the UT zips family
/*! zips and zipsN read a firing's worth of tokens from each input port
 * and write the whole collection as a single token on one output port.
 * That collection, \a Pack, is both the input token pack and the output
 * port's token type, so the output port lives here.
 */
template <typename Derived, typename Pack>
class zips_core : public ut_process
{
public:
    UT_out<Pack> oport1;    ///< port for the output channel

protected:
    static constexpr std::size_t n_ins = std::tuple_size<Pack>::value;

    std::array<size_t,n_ins> itoks; ///< consumption rate, one per input port
    Pack ivals;                     ///< input tokens, one vector per input port

    zips_core(const sc_module_name& _name,              ///< process name
              const std::array<size_t,n_ins>& itoks     ///< consumption rates
              ) : ut_process(_name), oport1("oport1"), itoks(itoks) {}

private:
    Derived& self() {return static_cast<Derived&>(*this);}

    void init() {resize_all(ivals, itoks);}

    void clean() {}

    void exec() {}

    void prep() {read_all(self().in_ports(), ivals);}

    void prod() {write_multiport(oport1, ivals);}

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        bind_all(boundInChans, self().in_ports());
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
    }
#endif
};

//! Shared implementation of the UT unzip family
/*! The mirror image of zips_core: unzip and unzipN read a single \a Pack
 * token from one input port -- which is why that port lives here -- and
 * write each of its vectors to the matching output port. Neither takes
 * rates: an untimed unzip simply forwards whatever lengths it was given.
 */
template <typename Derived, typename Pack>
class unzip_core : public ut_process
{
public:
    UT_in<Pack> iport1;     ///< port for the input channel

protected:
    Pack in_val;            ///< the token read from iport1

    unzip_core(const sc_module_name& _name      ///< process name
               ) : ut_process(_name), iport1("iport1") {}

private:
    Derived& self() {return static_cast<Derived&>(*this);}

    void init() {}

    void clean() {}

    void exec() {}

    void prep() {in_val = iport1.read();}

    void prod() {write_all(self().out_ports(), in_val);}

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        boundInChans.resize(1);     // only one input port
        boundInChans[0].port = &iport1;
        bind_all(boundOutChans, self().out_ports());
    }
#endif
};

}

//! Process constructor for a combinational process (actor) with one input and one output
/*! This class is used to build combinational processes with one input
 * and one output. The class is parameterized for input and output
 * data-types.
 */
template <typename T0, typename T1>
class comb : public detail::comb_core<comb<T0,T1>,
                                      std::tuple<std::vector<T0>>,
                                      std::tuple<std::vector<T1>>>
{
    typedef detail::comb_core<comb<T0,T1>,
                              std::tuple<std::vector<T0>>,
                              std::tuple<std::vector<T1>>> base;
    friend base;
public:
    UT_in<T1>  iport1;       ///< port for the input channel
    UT_out<T0> oport1;       ///< port for the output channel

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(std::vector<T0>&,
                                const std::vector<T1>&
                                )> functype;

    //! The constructor requires the module name ad the number of tokens to be produced
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented function to it and writes the
     * results using the output port
     */
    comb(const sc_module_name& _name,      ///< process name
         const functype& _func,           ///< function to be passed
         const unsigned int& i1toks       ///< consumption rate for the first input
         ) : base(_name,{i1toks}), iport1("iport1"), oport1("oport1"), _func(_func)
    {
#ifdef FORSYDE_INTROSPECTION
        this->arg_vec.push_back(std::make_tuple("i1toks",std::to_string(i1toks)));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "UT::comb";}

private:
    //! The function passed to the process constructor
    functype _func;

    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}

    void exec() {_func(std::get<0>(this->ovals), std::get<0>(this->ivals));}
};

//! Process constructor for a combinational process with two inputs and one output
/*! similar to comb with two inputs
 */
template <typename T0, typename T1, typename T2>
class comb2 : public detail::comb_core<comb2<T0,T1,T2>,
                                       std::tuple<std::vector<T0>>,
                                       std::tuple<std::vector<T1>,std::vector<T2>>>
{
    typedef detail::comb_core<comb2<T0,T1,T2>,
                              std::tuple<std::vector<T0>>,
                              std::tuple<std::vector<T1>,std::vector<T2>>> base;
    friend base;
public:
    UT_in<T1> iport1;        ///< port for the input channel 1
    UT_in<T2> iport2;        ///< port for the input channel 2
    UT_out<T0> oport1;        ///< port for the output channel

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(std::vector<T0>&,
                                const std::vector<T1>&,
                                const std::vector<T2>&
                                )> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input ports,
     * applies the user-imlpemented function to them and writes the
     * results using the output port
     */
    comb2(const sc_module_name& _name,      ///< process name
          const functype& _func,            ///< function to be passed
          const unsigned int& i1toks,      ///< consumption rate for the first input
          const unsigned int& i2toks       ///< consumption rate for the second input
          ) : base(_name,{i1toks,i2toks}),
              iport1("iport1"), iport2("iport2"), oport1("oport1"), _func(_func)
    {
#ifdef FORSYDE_INTROSPECTION
        this->arg_vec.push_back(std::make_tuple("i1toks",std::to_string(i1toks)));
        this->arg_vec.push_back(std::make_tuple("i2toks",std::to_string(i2toks)));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "UT::comb2";}

private:
    //! The function passed to the process constructor
    functype _func;

    auto in_ports()  {return std::tie(iport1,iport2);}
    auto out_ports() {return std::tie(oport1);}

    void exec()
    {
        _func(std::get<0>(this->ovals),
              std::get<0>(this->ivals), std::get<1>(this->ivals));
    }
};

//! Process constructor for a combinational process with three inputs and one output
/*! similar to comb with three inputs
 */
template <typename T0, typename T1, typename T2, typename T3>
class comb3 : public detail::comb_core<comb3<T0,T1,T2,T3>,
                                       std::tuple<std::vector<T0>>,
                                       std::tuple<std::vector<T1>,std::vector<T2>,std::vector<T3>>>
{
    typedef detail::comb_core<comb3<T0,T1,T2,T3>,
                              std::tuple<std::vector<T0>>,
                              std::tuple<std::vector<T1>,std::vector<T2>,std::vector<T3>>> base;
    friend base;
public:
    UT_in<T1> iport1;        ///< port for the input channel 1
    UT_in<T2> iport2;        ///< port for the input channel 2
    UT_in<T3> iport3;        ///< port for the input channel 3
    UT_out<T0> oport1;        ///< port for the output channel

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(std::vector<T0>&,
                                const std::vector<T1>&,
                                const std::vector<T2>&,
                                const std::vector<T3>&
                                )> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input ports,
     * applies the user-imlpemented function to them and writes the
     * results using the output port
     */
    comb3(const sc_module_name& _name,      ///< process name
          const functype& _func,            ///< function to be passed
          const unsigned int& i1toks,      ///< consumption rate for the first input
          const unsigned int& i2toks,      ///< consumption rate for the second input
          const unsigned int& i3toks       ///< consumption rate for the third input
          ) : base(_name,{i1toks,i2toks,i3toks}),
              iport1("iport1"), iport2("iport2"), iport3("iport3"),
              oport1("oport1"), _func(_func)
    {
#ifdef FORSYDE_INTROSPECTION
        this->arg_vec.push_back(std::make_tuple("i1toks",std::to_string(i1toks)));
        this->arg_vec.push_back(std::make_tuple("i2toks",std::to_string(i2toks)));
        this->arg_vec.push_back(std::make_tuple("i3toks",std::to_string(i3toks)));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "UT::comb3";}

private:
    //! The function passed to the process constructor
    functype _func;

    auto in_ports()  {return std::tie(iport1,iport2,iport3);}
    auto out_ports() {return std::tie(oport1);}

    void exec()
    {
        _func(std::get<0>(this->ovals),
              std::get<0>(this->ivals), std::get<1>(this->ivals),
              std::get<2>(this->ivals));
    }
};

//! Process constructor for a combinational process with four inputs and one output
/*! similar to comb with four inputs
 */
template <typename T0, typename T1, typename T2, typename T3, typename T4>
class comb4 : public detail::comb_core<comb4<T0,T1,T2,T3,T4>,
                                       std::tuple<std::vector<T0>>,
                                       std::tuple<std::vector<T1>,std::vector<T2>,std::vector<T3>,std::vector<T4>>>
{
    typedef detail::comb_core<comb4<T0,T1,T2,T3,T4>,
                              std::tuple<std::vector<T0>>,
                              std::tuple<std::vector<T1>,std::vector<T2>,std::vector<T3>,std::vector<T4>>> base;
    friend base;
public:
    UT_in<T1> iport1;        ///< port for the input channel 1
    UT_in<T2> iport2;        ///< port for the input channel 2
    UT_in<T3> iport3;        ///< port for the input channel 3
    UT_in<T4> iport4;        ///< port for the input channel 4
    UT_out<T0> oport1;        ///< port for the output channel

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(std::vector<T0>&,
                                const std::vector<T1>&,
                                const std::vector<T2>&,
                                const std::vector<T3>&,
                                const std::vector<T4>&
                                )> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input ports,
     * applies the user-imlpemented function to them and writes the
     * results using the output port
     */
    comb4(const sc_module_name& _name,      ///< process name
          const functype& _func,            ///< function to be passed
          const unsigned int& i1toks,      ///< consumption rate for the first input
          const unsigned int& i2toks,      ///< consumption rate for the second input
          const unsigned int& i3toks,      ///< consumption rate for the third input
          const unsigned int& i4toks       ///< consumption rate for the forth input
          ) : base(_name,{i1toks,i2toks,i3toks,i4toks}),
              iport1("iport1"), iport2("iport2"), iport3("iport3"),
              iport4("iport4"), oport1("oport1"), _func(_func)
    {
#ifdef FORSYDE_INTROSPECTION
        this->arg_vec.push_back(std::make_tuple("i1toks",std::to_string(i1toks)));
        this->arg_vec.push_back(std::make_tuple("i2toks",std::to_string(i2toks)));
        this->arg_vec.push_back(std::make_tuple("i3toks",std::to_string(i3toks)));
        this->arg_vec.push_back(std::make_tuple("i4toks",std::to_string(i4toks)));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "UT::comb4";}

private:
    //! The function passed to the process constructor
    functype _func;

    auto in_ports()  {return std::tie(iport1,iport2,iport3,iport4);}
    auto out_ports() {return std::tie(oport1);}

    void exec()
    {
        _func(std::get<0>(this->ovals),
              std::get<0>(this->ivals), std::get<1>(this->ivals),
              std::get<2>(this->ivals), std::get<3>(this->ivals));
    }
};

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
class delay : public ut_process
{
public:
    UT_in<T>  iport1;       ///< port for the input channel
    UT_out<T> oport1;       ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which inserts the initial element, reads
     * data from its input port, and writes the results using the output
     * port.
     */
    delay(const sc_module_name& _name,     ///< process name
           const T& init_val                 ///< initial value
          ) : ut_process(_name), iport1("iport1"), oport1("oport1"),
              init_val(init_val)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << init_val;
        arg_vec.push_back(std::make_tuple("init_val", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "UT::delay";}
    
private:
    // Initial value
    T init_val;
    
    // Inputs and output variables
    T* val;
    
    //Implementing the abstract semantics
    void init()
    {
        val = new T;
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
        boundInChans.resize(1);     // only one input port
        boundInChans[0].port = &iport1;
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
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
class delayn : public ut_process
{
public:
    UT_in<T>  iport1;       ///< port for the input channel
    UT_out<T> oport1;        ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which inserts the initial elements,
     * reads data from its input port, and writes the results using the
     * output port.
     */
    delayn(const sc_module_name& _name,    ///< process name
            const T& init_val,               ///< initial value
            const unsigned int& n            ///< number of delay elements
          ) : ut_process(_name), iport1("iport1"), oport1("oport1"),
              init_val(init_val), ns(n)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << init_val;
        arg_vec.push_back(std::make_tuple("init_val", ss.str()));
        arg_vec.push_back(std::make_tuple("n", std::to_string(n)));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "UT::delayn";}
    
private:
    // Initial value
    T init_val;
    unsigned int ns;
    
    // Inputs and output variables
    T* val;
    
    //Implementing the abstract semantics
    void init()
    {
        val = new T;
        for (unsigned int i=0; i<ns; i++)
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
        boundInChans.resize(1);     // only one input port
        boundInChans[0].port = &iport1;
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
    }
#endif
};


//! Process constructor for a scan process
/*! This class is used to build a state machine which has its internal
 * state directly visible at the output.
 * Given an initial state, a next-state function, is applied iteratively
 * to compute the next state.
 */
template <class IT, class ST>
class scan : public detail::fsm_core<scan<IT,ST>,
                         std::tuple<>,
                         std::tuple<std::vector<IT>>,
                         ST>
{
    typedef detail::fsm_core<scan<IT,ST>,
                             std::tuple<>,
                             std::tuple<std::vector<IT>>,
                             ST> base;
    friend base;
public:
    UT_in<IT>  iport1;        ///< port for the input channel
    UT_out<ST> oport1;        ///< port for the output channel
    
    //! Type of the partitioning function to be passed to the process constructor
    typedef std::function<void(unsigned int&, const ST&)> gamma_functype;
    
    //! Type of the next-state function to be passed to the process constructor
    typedef std::function<void(ST&, const ST&, const std::vector<IT>&)> ns_functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented functions to the input and current
     * state and writes the results using the output port
     */
    scan(const sc_module_name& _name,   ///< The module name
         const gamma_functype& _gamma_func,///< The partitioning function
         const ns_functype& _ns_func, ///< The next_state function
         const ST& init_st  ///< Initial state
         ) : base(_name, init_st), iport1("iport1"), oport1("oport1"), _gamma_func(_gamma_func), _ns_func(_ns_func)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << init_st;
        this->arg_vec.push_back(std::make_tuple("init_st",ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "UT::scan";}
    
private:
    //! The functions passed to the process constructor
    gamma_functype _gamma_func;
    ns_functype _ns_func;

    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}

    void resize_inputs()
    {
        unsigned int itoks;
        _gamma_func(itoks, this->stval);    // determine how many tokens to read
        std::get<0>(this->ivals).resize(itoks);
    }

    void exec()
    {
        _ns_func(this->nsval, this->stval, std::get<0>(this->ivals));
        this->stval = this->nsval;
    }

    // scan makes its state directly visible as a single event rather than
    // as a vector of output tokens, so it carries no output token pack and
    // writes in its own prod() instead of through the core's write loop.
    void prod() {write_multiport(oport1, this->stval);}

};

//! Process constructor for a scand process
/*! This class is used to build a state machine which has its internal
 * state directly visible at the output with a delay.
 * Given an initial state, a next-state function, is applied iteratively
 * to compute the next state.
 */
template <class IT, class ST>
class scand : public detail::fsm_core<scand<IT,ST>,
                          std::tuple<>,
                          std::tuple<std::vector<IT>>,
                          ST, true>
{
    typedef detail::fsm_core<scand<IT,ST>,
                             std::tuple<>,
                             std::tuple<std::vector<IT>>,
                             ST, true> base;
    friend base;
public:
    UT_in<IT>  iport1;        ///< port for the input channel
    UT_out<ST> oport1;        ///< port for the output channel
    
    //! Type of the partitioning function to be passed to the process constructor
    typedef std::function<void(unsigned int&, const ST&)> gamma_functype;
    
    //! Type of the next-state function to be passed to the process constructor
    typedef std::function<void(ST&, const ST&, const std::vector<IT>&)> ns_functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented functions to the input and current
     * state and writes the results using the output port
     */
    scand(const sc_module_name& _name,   ///< The module name
           const gamma_functype& _gamma_func,///< The partitioning function
           const ns_functype& _ns_func, ///< The next_state function
           const ST& init_st  ///< Initial state
           ) : base(_name, init_st), iport1("iport1"), oport1("oport1"), _gamma_func(_gamma_func), _ns_func(_ns_func)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << init_st;
        this->arg_vec.push_back(std::make_tuple("init_st",ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "UT::scan";}
    
private:
    //! The functions passed to the process constructor
    gamma_functype _gamma_func;
    ns_functype _ns_func;

    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}

    void resize_inputs()
    {
        unsigned int itoks;
        _gamma_func(itoks, this->stval);    // determine how many tokens to read
        std::get<0>(this->ivals).resize(itoks);
    }

    void exec()
    {
        if (this->first_run)
            this->first_run = false;
        else
        {
            _ns_func(this->nsval, this->stval, std::get<0>(this->ivals));
            this->stval = this->nsval;
        }
    }

    // scan makes its state directly visible as a single event rather than
    // as a vector of output tokens, so it carries no output token pack and
    // writes in its own prod() instead of through the core's write loop.
    void prod() {write_multiport(oport1, this->stval);}

};

//! Process constructor for a Moore machine
/*! This class is used to build a finite state machine of type Moore.
 * Given an initial state, a next-state function, and an output decoding
 * function it creates a Moore process.
 */
template <class IT, class ST, class OT>
class moore : public detail::fsm_core<moore<IT,ST,OT>,
                          std::tuple<std::vector<OT>>,
                          std::tuple<std::vector<IT>>,
                          ST, true>
{
    typedef detail::fsm_core<moore<IT,ST,OT>,
                             std::tuple<std::vector<OT>>,
                             std::tuple<std::vector<IT>>,
                             ST, true> base;
    friend base;
public:
    UT_in<IT>  iport1;        ///< port for the input channel
    UT_out<OT> oport1;        ///< port for the output channel
    
    //! Type of the partitioning function to be passed to the process constructor
    typedef std::function<void(unsigned int&, const ST&)> gamma_functype;
    
    //! Type of the next-state function to be passed to the process constructor
    typedef std::function<void(ST&, const ST&, const std::vector<IT>&)> ns_functype;
    
    //! Type of the output-decoding function to be passed to the process constructor
    typedef std::function<void(std::vector<OT>&, const ST&)> od_functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented functions to the input and current
     * state and writes the results using the output port
     */
    moore(const sc_module_name& _name,   ///< The module name
           const gamma_functype& _gamma_func,///< The partitioning function
           const ns_functype& _ns_func, ///< The next_state function
           const od_functype& _od_func, ///< The output-decoding function
           const ST& init_st  ///< Initial state
          ) : base(_name, init_st), iport1("iport1"), oport1("oport1"), _gamma_func(_gamma_func), _ns_func(_ns_func),
              _od_func(_od_func)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(this->basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        this->arg_vec.push_back(std::make_tuple("_od_func",func_name+std::string("_od_func")));
        std::stringstream ss;
        ss << init_st;
        this->arg_vec.push_back(std::make_tuple("init_st",ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "UT::moore";}
    
private:
    //! The functions passed to the process constructor
    gamma_functype _gamma_func;
    ns_functype _ns_func;
    od_functype _od_func;

    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}

    void resize_inputs()
    {
        unsigned int itoks;
        _gamma_func(itoks, this->stval);    // determine how many tokens to read
        std::get<0>(this->ivals).resize(itoks);
    }

    void exec()
    {
        // Jantsch (3.4): a mooreU process emits f(w_i) and moves to
        // w_{i+1} = g(w_i, a_i). The first evaluation cycle consumes
        // nothing and emits f(w_0), so from the second cycle on the
        // transition has to happen *before* the decode -- otherwise
        // f(w_0) is emitted a second time and the whole output signal is
        // one cycle stale. See tests/fsm_semantics.
        if (this->first_run)
            this->first_run = false;
        else
        {
            _ns_func(this->nsval, this->stval, std::get<0>(this->ivals));
            this->stval = this->nsval;
        }
        _od_func(std::get<0>(this->ovals), this->stval);
    }

};

//! Process constructor for a Moore machine
/*! This class is used to build a finite state machine of type Moore.
 * Given an initial state, a next-state function, and an output decoding
 * function it creates a Mealy process.
 */
template<typename TO_tuple, typename TI_tuple, typename TS_tuple> class mooreMN;

template <typename... TOs, typename... TIs, typename... TSs>
class mooreMN<std::tuple<TOs...>,std::tuple<TIs...>,std::tuple<TSs...>> : public detail::fsm_core<mooreMN<std::tuple<TOs...>,std::tuple<TIs...>,std::tuple<TSs...>>,
                            std::tuple<std::vector<TOs>...>,
                            std::tuple<std::vector<TIs>...>,
                            std::tuple<TSs...>, true>
{
    typedef detail::fsm_core<mooreMN<std::tuple<TOs...>,std::tuple<TIs...>,std::tuple<TSs...>>,
                             std::tuple<std::vector<TOs>...>,
                             std::tuple<std::vector<TIs>...>,
                             std::tuple<TSs...>, true> base;
    friend base;
public:
    std::tuple<UT_in<TIs>...>  iport;///< tuple of ports for the input channels
    std::tuple<UT_out<TOs>...> oport;///< tuple of ports for the output channels
    
    //! Type of the partitioning function to be passed to the process constructor
    typedef std::function<void(std::array<size_t, sizeof...(TIs)>&,
                                const std::tuple<TSs...>&)> gamma_functype;
    
    //! Type of the next-state function to be passed to the process constructor
    typedef std::function<void(std::tuple<TSs...>&,
                                const std::tuple<TSs...>&,
                                const std::tuple<std::vector<TIs>...>&)> ns_functype;
    
    //! Type of the output-decoding function to be passed to the process constructor
    typedef std::function<void(std::tuple<std::vector<TOs>...>&,
                                const std::tuple<TSs...>&)> od_functype;
    
    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented functions to the input and current
     * state and writes the results using the output port
     */
    mooreMN(const sc_module_name& _name,        ///< The module name
            const gamma_functype& _gamma_func,  ///< The partitioning function
            const ns_functype& _ns_func,        ///< The next_state function
            const od_functype& _od_func,        ///< The output-decoding function
            const std::tuple<TSs...>& init_st   ///< Initial state
            ) : base(_name, init_st), _gamma_func(_gamma_func), _ns_func(_ns_func),
              _od_func(_od_func)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(this->basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        this->arg_vec.push_back(std::make_tuple("_od_func",func_name+std::string("_od_func")));
        std::stringstream ss;
        ss << init_st;
        this->arg_vec.push_back(std::make_tuple("init_st",ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "UT::mooreMN";}
    
private:
    //! The functions passed to the process constructor
    gamma_functype _gamma_func;
    ns_functype _ns_func;
    od_functype _od_func;

    auto in_ports()  {return std::apply([](auto&... p){return std::tie(p...);}, iport);}
    auto out_ports() {return std::apply([](auto&... p){return std::tie(p...);}, oport);}

    void resize_inputs()
    {
        std::array<size_t, sizeof...(TIs)> itoks;
        _gamma_func(itoks, this->stval);    // determine how many tokens to read
        detail::resize_all(this->ivals, itoks);
    }

    void exec()
    {
        // As moore above.
        if (this->first_run)
            this->first_run = false;
        else
        {
            _ns_func(this->nsval, this->stval, this->ivals);
            this->stval = this->nsval;
        }
        _od_func(this->ovals, this->stval);
    }

};

//! Process constructor for a Mealy machine
/*! This class is used to build a finite state machine of type Mealy.
 * Given an initial state, a next-state function, and an output decoding
 * function it creates a Mealy process.
 */
template <class IT, class ST, class OT>
class mealy : public detail::fsm_core<mealy<IT,ST,OT>,
                          std::tuple<std::vector<OT>>,
                          std::tuple<std::vector<IT>>,
                          ST>
{
    typedef detail::fsm_core<mealy<IT,ST,OT>,
                             std::tuple<std::vector<OT>>,
                             std::tuple<std::vector<IT>>,
                             ST> base;
    friend base;
public:
    UT_in<IT>  iport1;        ///< port for the input channel
    UT_out<OT> oport1;        ///< port for the output channel
    
    //! Type of the partitioning function to be passed to the process constructor
    typedef std::function<void(unsigned int&, const ST&)> gamma_functype;
    
    //! Type of the next-state function to be passed to the process constructor
    typedef std::function<void(ST&, const ST&, const std::vector<IT>&)> ns_functype;
    
    //! Type of the output-decoding function to be passed to the process constructor
    typedef std::function<void(std::vector<OT>&, const ST&,
                                 const std::vector<IT>&)> od_functype;
    
    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented functions to the input and current
     * state and writes the results using the output port
     */
    mealy(const sc_module_name& _name,   ///< The module name
           const gamma_functype& _gamma_func,///< The partitioning function
           const ns_functype& _ns_func, ///< The next_state function
           const od_functype& _od_func, ///< The output-decoding function
           const ST& init_st  ///< Initial state
          ) : base(_name, init_st), iport1("iport1"), oport1("oport1"), _gamma_func(_gamma_func), _ns_func(_ns_func),
              _od_func(_od_func)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(this->basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        this->arg_vec.push_back(std::make_tuple("_od_func",func_name+std::string("_od_func")));
        std::stringstream ss;
        ss << init_st;
        this->arg_vec.push_back(std::make_tuple("init_st",ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "UT::mealy";}
    
private:
    //! The functions passed to the process constructor
    gamma_functype _gamma_func;
    ns_functype _ns_func;
    od_functype _od_func;

    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}

    void resize_inputs()
    {
        unsigned int itoks;
        _gamma_func(itoks, this->stval);    // determine how many tokens to read
        std::get<0>(this->ivals).resize(itoks);
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
template<typename TO_tuple, typename TI_tuple, typename TS_tuple> class mealyMN;

template <typename... TOs, typename... TIs, typename... TSs>
class mealyMN<std::tuple<TOs...>,std::tuple<TIs...>,std::tuple<TSs...>> : public detail::fsm_core<mealyMN<std::tuple<TOs...>,std::tuple<TIs...>,std::tuple<TSs...>>,
                            std::tuple<std::vector<TOs>...>,
                            std::tuple<std::vector<TIs>...>,
                            std::tuple<TSs...>>
{
    typedef detail::fsm_core<mealyMN<std::tuple<TOs...>,std::tuple<TIs...>,std::tuple<TSs...>>,
                             std::tuple<std::vector<TOs>...>,
                             std::tuple<std::vector<TIs>...>,
                             std::tuple<TSs...>> base;
    friend base;
public:
    std::tuple<UT_in<TIs>...>  iport;///< tuple of ports for the input channels
    std::tuple<UT_out<TOs>...> oport;///< tuple of ports for the output channels
    
    //! Type of the partitioning function to be passed to the process constructor
    typedef std::function<void(std::array<size_t, sizeof...(TIs)>&,
                                const std::tuple<TSs...>&)> gamma_functype;
    
    //! Type of the next-state function to be passed to the process constructor
    typedef std::function<void(std::tuple<TSs...>&,
                                const std::tuple<TSs...>&,
                                const std::tuple<std::vector<TIs>...>&)> ns_functype;
    
    //! Type of the output-decoding function to be passed to the process constructor
    typedef std::function<void(std::tuple<std::vector<TOs>...>&,
                                const std::tuple<TSs...>&,
                                const std::tuple<std::vector<TIs>...>&)> od_functype;
    
    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented functions to the input and current
     * state and writes the results using the output port
     */
    mealyMN(const sc_module_name& _name,        ///< The module name
            const gamma_functype& _gamma_func,  ///< The partitioning function
            const ns_functype& _ns_func,        ///< The next_state function
            const od_functype& _od_func,        ///< The output-decoding function
            const std::tuple<TSs...>& init_st   ///< Initial state
            ) : base(_name, init_st), _gamma_func(_gamma_func), _ns_func(_ns_func),
              _od_func(_od_func)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(this->basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        this->arg_vec.push_back(std::make_tuple("_od_func",func_name+std::string("_od_func")));
        std::stringstream ss;
        ss << init_st;
        this->arg_vec.push_back(std::make_tuple("init_st",ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "UT::mealyMN";}
    
private:
    //! The functions passed to the process constructor
    gamma_functype _gamma_func;
    ns_functype _ns_func;
    od_functype _od_func;

    auto in_ports()  {return std::apply([](auto&... p){return std::tie(p...);}, iport);}
    auto out_ports() {return std::apply([](auto&... p){return std::tie(p...);}, oport);}

    void resize_inputs()
    {
        std::array<size_t, sizeof...(TIs)> itoks;
        _gamma_func(itoks, this->stval);    // determine how many tokens to read
        detail::resize_all(this->ivals, itoks);
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
class constant : public ut_process
{
public:
    UT_out<T> oport1;            ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * and writes the result using the output port
     */
    constant(const sc_module_name& _name,      ///< The module name
              const T& init_val,                ///< The constant output value
              const unsigned long long& take=0 ///< number of tokens produced (0 for infinite)
             ) : ut_process(_name), oport1("oport1"),
                 init_val(init_val), take(take)
                 
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << init_val;
        arg_vec.push_back(std::make_tuple("init_val", ss.str()));
        arg_vec.push_back(std::make_tuple("take", std::to_string(take)));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "UT::constant";}
    
private:
    T init_val;
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
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
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
class source : public ut_process
{
public:
    UT_out<T> oport1;        ///< port for the output channel
    
    //! Type of the function to be passed to the process constructor
    typedef std::function<void(T&, const T&)> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * and writes the result using the output port
     */
    source(const sc_module_name& _name,   ///< The module name
            const functype& _func,         ///< function to be passed
            const T& init_val,              ///< Initial state
            const unsigned long long& take=0 ///< number of tokens produced (0 for infinite)
          ) : ut_process(_name), oport1("oport1"),
              init_st(init_val), take(take), _func(_func)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_func",func_name+std::string("_func")));
        std::stringstream ss;
        ss << init_val;
        arg_vec.push_back(std::make_tuple("init_val", ss.str()));
        arg_vec.push_back(std::make_tuple("take", std::to_string(take)));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "UT::source";}
    
private:
    T init_st;        // The current state
    unsigned long long take;    // Number of tokens produced
    
    T* cur_st;        // The current state of the process
    unsigned long long tok_cnt;
    bool infinite;
    
    //! The function passed to the process constructor
    functype _func;
    
    //Implementing the abstract semantics
    void init()
    {
        cur_st = new T;
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
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
    }
#endif
};

//! Process constructor for a source process with vector input
/*! This class is used to build a souce process which only has an output.
 * Given the test bench vector, the process iterates over the emenets
 * of the vector and outputs one value on each evaluation cycle.
 */
template <class OTYP>
class vsource : public sc_module
{
public:
    sc_fifo_out<OTYP> oport1;     ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which writes the result using the output
     * port.
     */
    vsource(const sc_module_name& _name,           ///< The module name
             const std::vector<OTYP>& invec  ///< Initial vector
            )
         :sc_module(_name), in_vec(invec)
    {
        SC_THREAD(worker);
    }
private:
    std::vector<OTYP> in_vec;
    SC_HAS_PROCESS(vsource);

    //! The main and only execution thread of the module
    void worker()
    {
        typename std::vector<OTYP>::iterator itr;
        for (itr=in_vec.begin();itr!=in_vec.end();itr++)
        {
            OTYP out_val = *itr;
            write_multiport(oport1,out_val);    // write to the output
        }
    }
};

//! Process constructor for a sink process
/*! This class is used to build a sink process which only has an input.
 * Its main purpose is to be used in test-benches. The process repeatedly
 * applies a given function to the current input.
 */
template <class T>
class sink : public ut_process
{
public:
    UT_in<T> iport1;         ///< port for the input channel
    
    //! Type of the function to be passed to the process constructor
    typedef std::function<void(const T&)> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * in each cycle.
     */
    sink(const sc_module_name& _name,      ///< process name
          const functype& _func             ///< function to be passed
        ) : ut_process(_name), iport1("iport1"), _func(_func)
            
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_func",func_name+std::string("_func")));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "UT::sink";}
    
private:
    T* val;         // The current state of the process

    //! The function passed to the process constructor
    functype _func;
    
    //Implementing the abstract semantics
    void init()
    {
        val = new T;
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
        boundInChans.resize(1);    // only one output port
        boundInChans[0].port = &iport1;
    }
#endif
};

//! The zips process with two inputs and one output
/*! This process "zips" two incoming signals into one signal of tuples.
 */
template <class T1, class T2>
class zips : public detail::zips_core<zips<T1,T2>,
                                      std::tuple<std::vector<T1>,std::vector<T2>>>
{
    typedef detail::zips_core<zips<T1,T2>,
                              std::tuple<std::vector<T1>,std::vector<T2>>> base;
    friend base;
public:
    UT_in<T1> iport1;        ///< port for the input channel 1
    UT_in<T2> iport2;        ///< port for the input channel 2

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * zips them together and writes the results using the output port
     */
    zips(const sc_module_name& _name,       ///< process name
        const unsigned int& i1toks,         ///< consumption rate for the first input
        const unsigned int& i2toks          ///< consumption rate for the second input
        ) : base(_name,{i1toks,i2toks}), iport1("iport1"), iport2("iport2") {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "UT::zips";}

private:
    auto in_ports() {return std::tie(iport1,iport2);}
};

//! The zips process with variable number of inputs and one output
/*! This process "zips" the incoming signals into one signal of tuples.
 */
template <class... Ts>
class zipsN : public detail::zips_core<zipsN<Ts...>, std::tuple<std::vector<Ts>...>>
{
    typedef detail::zips_core<zipsN<Ts...>, std::tuple<std::vector<Ts>...>> base;
    friend base;
public:
    std::tuple <UT_in<Ts>...> iport;                ///< tuple of ports for the input channels

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * zips them together and writes the results using the output port
     */
    zipsN(const sc_module_name& _name,                  ///< process name
            std::array<size_t, sizeof...(Ts)> in_toks   ///< consumption rates for the inputs
            ) : base(_name,in_toks)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << in_toks;
        this->arg_vec.push_back(std::make_tuple("itoks",ss.str()));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "UT::zipsN";}

private:
    auto in_ports() {return std::apply([](auto&... p){return std::tie(p...);}, iport);}
};

//! The unzip process with one input and two outputs
/*! This process "unzips" a signal of tuples into two separate signals
 */
template <class T1, class T2>
class unzip : public detail::unzip_core<unzip<T1,T2>,
                                        std::tuple<std::vector<T1>,std::vector<T2>>>
{
    typedef detail::unzip_core<unzip<T1,T2>,
                               std::tuple<std::vector<T1>,std::vector<T2>>> base;
    friend base;
public:
    UT_out<T1> oport1;        ///< port for the output channel 1
    UT_out<T2> oport2;        ///< port for the output channel 2

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input ports,
     * unzips them and writes the results using the output ports
     */
    unzip(const sc_module_name& _name       ///< process name
           ) : base(_name), oport1("oport1"), oport2("oport2") {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "UT::unzip";}

private:
    auto out_ports() {return std::tie(oport1,oport2);}
};

//! The unzip process with one input and variable number of outputs
/*! This process "unzips" the incoming signal into a tuple of signals.
 */
template <class... Ts>
class unzipN : public detail::unzip_core<unzipN<Ts...>, std::tuple<std::vector<Ts>...>>
{
    typedef detail::unzip_core<unzipN<Ts...>, std::tuple<std::vector<Ts>...>> base;
    friend base;
public:
    std::tuple<UT_out<Ts>...> oport;///< tuple of ports for the output channels

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * unzips it and writes the results using the output ports
     */
    unzipN(const sc_module_name& _name      ///< process name
            ) : base(_name) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "UT::unzipN";}

private:
    auto out_ports() {return std::apply([](auto&... p){return std::tie(p...);}, oport);}
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
class fanout : public ut_process
{
public:
    UT_in<T> iport1;        ///< port for the input channel
    UT_out<T> oport1;       ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies and writes the results using the output port
     */
    fanout(const sc_module_name& _name)  // module name
         : ut_process(_name) { }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "UT::fanout";}
    
private:
    // Inputs and output variables
    T* val;
    
    //Implementing the abstract semantics
    void init()
    {
        val = new T;
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
        boundInChans.resize(1);     // only one input port
        boundInChans[0].port = &iport1;
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
    }
#endif
};

}
}

#endif
