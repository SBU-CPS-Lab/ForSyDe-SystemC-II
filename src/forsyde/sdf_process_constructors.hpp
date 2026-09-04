/**********************************************************************           
    * sdf_process_constructors.hpp -- Process constructors in the SDF *
    *                                MOC                              *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Providing basic process constructors for modeling      *
    *          SDF systems in ForSyDe-SystemC                         *
    *                                                                 *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef SDF_PROCESS_CONSTRUCTORS_HPP
#define SDF_PROCESS_CONSTRUCTORS_HPP

/*! \file sdf_process_constructors.hpp
 * \brief Implements the basic process constructors in the SDF MoC
 * 
 *  This file includes the basic process constructors used for modeling
 * in the SDF model of computation.
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

// Streams a std::vector under FORSYDE_INTROSPECTION via prettyprint.hpp's
// generic container operator<<, which this file otherwise relies on
// forsyde.hpp having included first.
#include "prettyprint.hpp"

#include "sdf_process.hpp"

namespace ForSyDe
{

namespace SDF
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

//! Read one actor firing's worth of tokens from one port
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

//! Shared implementation of the SDF combinational (comb*) family
/*! The SDF counterpart of ForSyDe::SY::detail::comb_core, and the same
 * idea: comb, comb2, comb3, comb4 and combMN all resize a vector per
 * port to that port's rate, read a whole firing's worth of tokens from
 * each input port, apply the user function, and write a whole firing's
 * worth to each output port. Only the port count, the port names and the
 * spelling of the user function differ, so only those stay in the
 * derived classes.
 *
 * \a Derived supplies in_ports(), out_ports() and exec(), and is
 * befriended so they can stay private. Its own constructor is also what
 * adds this family's rate arguments to arg_vec, because the arity
 * variants name them individually (o1toks, i1toks, i2toks ...) while
 * combMN emits two arrays.
 *
 * The ports stay declared in the derived classes: their SystemC names go
 * into the introspection XML verbatim, and a tuple element cannot be
 * given one.
 */
template <typename Derived, typename OVals, typename IVals>
class comb_core : public sdf_process,
                  public ForSyDe::detail::bindable<Derived>
{
public:
    //! Hides sc_module's positional binding; see bindable
    using ForSyDe::detail::bindable<Derived>::operator();

protected:
    static constexpr std::size_t n_outs = std::tuple_size<OVals>::value;
    static constexpr std::size_t n_ins  = std::tuple_size<IVals>::value;

    //! production and consumption rates, one per output and input port
    std::array<size_t,n_outs> otoks;
    std::array<size_t,n_ins>  itoks;

    OVals ovals;    ///< output tokens, one vector per output port
    IVals ivals;    ///< input tokens, one vector per input port

    comb_core(sc_module_name _name,                  ///< process name
              const std::array<size_t,n_outs>& otoks,       ///< production rates
              const std::array<size_t,n_ins>& itoks         ///< consumption rates
              ) : sdf_process(_name), otoks(otoks), itoks(itoks)
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
    void init()
    {
        resize_all(ovals, otoks);
        resize_all(ivals, itoks);
    }

    void clean() {}

    void prep() {read_all(self().in_ports(), ivals);}

    void prod() {write_all(self().out_ports(), ovals);}

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        bind_all(boundInChans, self().in_ports());
        bind_all(boundOutChans, self().out_ports());
    }
#endif
};

//! Shared implementation of the SDF zip family
/*! zip and zipN read a firing's worth of tokens from each input port and
 * write the whole collection as a single token on one output port. That
 * collection, \a Pack, is both the input token pack and the output
 * port's token type, so the output port lives here. \a Derived supplies
 * in_ports() and its own rate arguments for arg_vec.
 */
template <typename Derived, typename Pack>
class zip_core : public sdf_process,
                 public ForSyDe::detail::bindable<Derived>
{
public:
    //! Hides sc_module's positional binding; see bindable
    using ForSyDe::detail::bindable<Derived>::operator();

public:
    SDF_out<Pack> oport1;   ///< port for the output channel

protected:
    static constexpr std::size_t n_ins = std::tuple_size<Pack>::value;

    std::array<size_t,n_ins> itoks; ///< consumption rate, one per input port
    Pack ivals;                     ///< input tokens, one vector per input port

    zip_core(sc_module_name _name,               ///< process name
             const std::array<size_t,n_ins>& itoks      ///< consumption rates
             ) : sdf_process(_name), oport1("oport1"), itoks(itoks) {}

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

//! Shared implementation of the SDF unzip family
/*! The mirror image of zip_core: unzip and unzipN read a single \a Pack
 * token from one input port -- which is why that port lives here -- and
 * write each of its vectors to the matching output port. \a Derived
 * supplies out_ports() and its own rate arguments for arg_vec.
 *
 * Note what is *not* here: the declared output rates play no part in
 * what this actually writes. Each output gets however many tokens the
 * vector it was handed happens to hold, which is whatever the upstream
 * zip put there, so an unzip whose declared rates disagree with its
 * input's vector lengths silently produces at a rate other than the one
 * its introspection XML reports -- exactly the assumption SDF schedule
 * analysis rests on. unzipN used to resize the vectors to the declared
 * rates in init(), which looks like a guard against that but is not one:
 * prep() assigns the whole pack from the port read before prod() ever
 * sees it, so the resize was overwritten every firing and had no effect.
 * The dead resize is gone; the rates are still recorded, and reconciling
 * them with reality is a real fix that needs a decision (truncate, pad,
 * or report) rather than a silent change of behaviour here.
 */
template <typename Derived, typename Pack>
class unzip_core : public sdf_process,
                   public ForSyDe::detail::bindable<Derived>
{
public:
    //! Hides sc_module's positional binding; see bindable
    using ForSyDe::detail::bindable<Derived>::operator();

public:
    SDF_in<Pack> iport1;    ///< port for the input channel

protected:
    Pack in_val;            ///< the token read from iport1

    unzip_core(sc_module_name _name      ///< process name
               ) : sdf_process(_name), iport1("iport1") {}

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
    SDF_in<T1>  iport1;       ///< port for the input channel
    SDF_out<T0> oport1;       ///< port for the output channel

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(std::vector<T0>&,
                                const std::vector<T1>&
                                )> functype;

    //! The constructor requires the module name ad the number of tokens to be produced
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented function to it and writes the
     * results using the output port
     */
    comb(sc_module_name _name,      ///< process name
         functype _func,           ///< function to be passed
         unsigned int o1toks,      ///< consumption rate for the first output
         unsigned int i1toks       ///< consumption rate for the first input
         ) : base(_name,{o1toks},{i1toks}), iport1("iport1"), oport1("oport1"),
             _func(_func)
    {
#ifdef FORSYDE_INTROSPECTION
        this->arg_vec.push_back(std::make_tuple("o1toks",std::to_string(o1toks)));
        this->arg_vec.push_back(std::make_tuple("i1toks",std::to_string(i1toks)));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SDF::comb";}

private:
    //! The function passed to the process constructor
    functype _func;

public:
    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}
private:

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
    SDF_in<T1> iport1;        ///< port for the input channel 1
    SDF_in<T2> iport2;        ///< port for the input channel 2
    SDF_out<T0> oport1;        ///< port for the output channel

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
    comb2(sc_module_name _name,      ///< process name
          functype _func,            ///< function to be passed
          unsigned int o1toks,      ///< consumption rate for the first output
          unsigned int i1toks,      ///< consumption rate for the first input
          unsigned int i2toks       ///< consumption rate for the second input
          ) : base(_name,{o1toks},{i1toks,i2toks}),
              iport1("iport1"), iport2("iport2"), oport1("oport1"), _func(_func)
    {
#ifdef FORSYDE_INTROSPECTION
        this->arg_vec.push_back(std::make_tuple("o1toks",std::to_string(o1toks)));
        this->arg_vec.push_back(std::make_tuple("i1toks",std::to_string(i1toks)));
        this->arg_vec.push_back(std::make_tuple("i2toks",std::to_string(i2toks)));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SDF::comb2";}

private:
    //! The function passed to the process constructor
    functype _func;

public:
    auto in_ports()  {return std::tie(iport1,iport2);}
    auto out_ports() {return std::tie(oport1);}
private:

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
    SDF_in<T1> iport1;        ///< port for the input channel 1
    SDF_in<T2> iport2;        ///< port for the input channel 2
    SDF_in<T3> iport3;        ///< port for the input channel 3
    SDF_out<T0> oport1;        ///< port for the output channel

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
    comb3(sc_module_name _name,      ///< process name
          functype _func,            ///< function to be passed
          unsigned int o1toks,      ///< consumption rate for the first output
          unsigned int i1toks,      ///< consumption rate for the first input
          unsigned int i2toks,      ///< consumption rate for the second input
          unsigned int i3toks       ///< consumption rate for the third input
          ) : base(_name,{o1toks},{i1toks,i2toks,i3toks}),
              iport1("iport1"), iport2("iport2"), iport3("iport3"),
              oport1("oport1"), _func(_func)
    {
#ifdef FORSYDE_INTROSPECTION
        this->arg_vec.push_back(std::make_tuple("o1toks",std::to_string(o1toks)));
        this->arg_vec.push_back(std::make_tuple("i1toks",std::to_string(i1toks)));
        this->arg_vec.push_back(std::make_tuple("i2toks",std::to_string(i2toks)));
        this->arg_vec.push_back(std::make_tuple("i3toks",std::to_string(i3toks)));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SDF::comb3";}

private:
    //! The function passed to the process constructor
    functype _func;

public:
    auto in_ports()  {return std::tie(iport1,iport2,iport3);}
    auto out_ports() {return std::tie(oport1);}
private:

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
    SDF_in<T1> iport1;        ///< port for the input channel 1
    SDF_in<T2> iport2;        ///< port for the input channel 2
    SDF_in<T3> iport3;        ///< port for the input channel 3
    SDF_in<T4> iport4;        ///< port for the input channel 4
    SDF_out<T0> oport1;        ///< port for the output channel

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
    comb4(sc_module_name _name,      ///< process name
          functype _func,            ///< function to be passed
          unsigned int o1toks,      ///< consumption rate for the first output
          unsigned int i1toks,      ///< consumption rate for the first input
          unsigned int i2toks,      ///< consumption rate for the second input
          unsigned int i3toks,      ///< consumption rate for the third input
          unsigned int i4toks       ///< consumption rate for the forth input
          ) : base(_name,{o1toks},{i1toks,i2toks,i3toks,i4toks}),
              iport1("iport1"), iport2("iport2"), iport3("iport3"), iport4("iport4"),
              oport1("oport1"), _func(_func)
    {
#ifdef FORSYDE_INTROSPECTION
        this->arg_vec.push_back(std::make_tuple("o1toks",std::to_string(o1toks)));
        this->arg_vec.push_back(std::make_tuple("i1toks",std::to_string(i1toks)));
        this->arg_vec.push_back(std::make_tuple("i2toks",std::to_string(i2toks)));
        this->arg_vec.push_back(std::make_tuple("i3toks",std::to_string(i3toks)));
        this->arg_vec.push_back(std::make_tuple("i4toks",std::to_string(i4toks)));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SDF::comb4";}

private:
    //! The function passed to the process constructor
    functype _func;

public:
    auto in_ports()  {return std::tie(iport1,iport2,iport3,iport4);}
    auto out_ports() {return std::tie(oport1);}
private:

    void exec()
    {
        _func(std::get<0>(this->ovals),
              std::get<0>(this->ivals), std::get<1>(this->ivals),
              std::get<2>(this->ivals), std::get<3>(this->ivals));
    }
};

//! Process constructor for a combinational process with M inputs and N outputs
/*! similar to comb with M inputs and unzipN
 */
template<typename TO_tuple, typename TI_tuple> class combMN;

template <typename... TOs, typename... TIs>
class combMN<std::tuple<TOs...>,std::tuple<TIs...>>
    : public detail::comb_core<combMN<std::tuple<TOs...>,std::tuple<TIs...>>,
                               std::tuple<std::vector<TOs>...>,
                               std::tuple<std::vector<TIs>...>>
{
    typedef detail::comb_core<combMN<std::tuple<TOs...>,std::tuple<TIs...>>,
                              std::tuple<std::vector<TOs>...>,
                              std::tuple<std::vector<TIs>...>> base;
    friend base;
public:
        // Carrier-U ports, not SDF ports. This constructor is one of the
    // four that SADF re-exports rather than repeating (see
    // sadf_process_constructors.hpp), because an SADF process is an SDF
    // process within any one scenario. Typing the ports SDF_in/SDF_out
    // would make an SADF model binding to them a narrowing, which the
    // D13 check would reject -- correctly, since the type would be
    // claiming something the component does not mean. It is generic over
    // the untimed carrier, so it says so, and SDF and SADF both reach it
    // by widening.
std::tuple<UT::UT_in<TIs>...>  iport;///< tuple of ports for the input channels
    std::tuple<UT::UT_out<TOs>...> oport;///< tuple of ports for the output channels

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(std::tuple<std::vector<TOs>...>&,
                                const std::tuple<std::vector<TIs>...>&)> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input ports,
     * applies the user-imlpemented function to them and writes the
     * results using the output ports
     */
    combMN(sc_module_name _name,      ///< process name
          functype _func,            ///< function to be passed
          std::array<size_t, sizeof...(TOs)> otoks, ///< consumption rate for the outputs
          std::array<size_t, sizeof...(TIs)> itoks  ///< consumption rates for the inputs
          ) : base(_name,otoks,itoks), _func(_func)
    {
#ifdef FORSYDE_INTROSPECTION
        // These two used to be reported the wrong way round: the value
        // streamed under the name "otoks" was itoks and vice versa, so
        // every SDF::combMN in every generated XML advertised its
        // consumption rates as its production rates and vice versa.
        std::stringstream ss;
        ss << otoks;
        this->arg_vec.push_back(std::make_tuple("otoks",ss.str()));
        ss.clear();
        ss.str(std::string());
        ss << itoks;
        this->arg_vec.push_back(std::make_tuple("itoks",ss.str()));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SDF::combMN";}

private:
    //! The function passed to the process constructor
    functype _func;

public:
    auto in_ports()  {return std::apply([](auto&... p){return std::tie(p...);}, iport);}
    auto out_ports() {return std::apply([](auto&... p){return std::tie(p...);}, oport);}
private:

    void exec() {_func(this->ovals, this->ivals);}
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
class delay : public sdf_process,
              public ForSyDe::detail::bindable<delay<T>>
{
public:
    SDF_in<T>  iport1;       ///< port for the input channel
    SDF_out<T> oport1;       ///< port for the output channel

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
          T init_val                 ///< initial value
          ) : sdf_process(_name), iport1("iport1"), oport1("oport1"),
              init_val(init_val)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << init_val;
        arg_vec.push_back(std::make_tuple("init_val", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SDF::delay";}
    
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
class delayn : public sdf_process,
               public ForSyDe::detail::bindable<delayn<T>>
{
public:
    // Carrier-U ports, not SDF ports. This constructor is one of the
    // four that SADF re-exports rather than repeating (see
    // sadf_process_constructors.hpp), because an SADF process is an SDF
    // process within any one scenario. Typing the ports SDF_in/SDF_out
    // would make an SADF model binding to them a narrowing, which the
    // D13 check would reject -- correctly, since the type would be
    // claiming something the component does not mean. It is generic over
    // the untimed carrier, so it says so, and SDF and SADF both reach it
    // by widening.
    UT::UT_in<T>  iport1;       ///< port for the input channel
    UT::UT_out<T> oport1;        ///< port for the output channel

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
           T init_val,               ///< initial value
           unsigned int n            ///< number of delay elements
          ) : sdf_process(_name), iport1("iport1"), oport1("oport1"),
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
    std::string forsyde_kind() const {return "SDF::delayn";}
    
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
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! Process constructor for a constant source process
/*! This class is used to build a souce process with constant output.
 * Its main purpose is to be used in test-benches.
 * 
 * This class can directly be instantiated to build a process.
 */
template <class T>
class constant : public sdf_process,
                 public ForSyDe::detail::bindable<constant<T>>
{
public:
    SDF_out<T> oport1;            ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<constant<T>>::operator();
    auto out_ports() {return std::tie(oport1);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * and writes the result using the output port
     */
    constant(sc_module_name _name,      ///< The module name
              T init_val,                ///< The constant output value
              unsigned long long take=0 ///< number of tokens produced (0 for infinite)
             ) : sdf_process(_name), oport1("oport1"),
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
    std::string forsyde_kind() const {return "SDF::constant";}
    
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
class source : public sdf_process,
               public ForSyDe::detail::bindable<source<T>>
{
public:
    // Carrier-U ports, not SDF ports. This constructor is one of the
    // four that SADF re-exports rather than repeating (see
    // sadf_process_constructors.hpp), because an SADF process is an SDF
    // process within any one scenario. Typing the ports SDF_in/SDF_out
    // would make an SADF model binding to them a narrowing, which the
    // D13 check would reject -- correctly, since the type would be
    // claiming something the component does not mean. It is generic over
    // the untimed carrier, so it says so, and SDF and SADF both reach it
    // by widening.
    UT::UT_out<T> oport1;        ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<source<T>>::operator();
    auto out_ports() {return std::tie(oport1);}
    
    //! Type of the function to be passed to the process constructor
    typedef std::function<void(T&, const T&)> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * and writes the result using the output port
     */
    source(sc_module_name _name,   ///< The module name
           functype _func,         ///< function to be passed
           T init_val,              ///< Initial state
           unsigned long long take=0 ///< number of tokens produced (0 for infinite)
          ) : sdf_process(_name), oport1("oport1"),
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
    std::string forsyde_kind() const {return "SDF::source";}
    
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
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! Process constructor for a file_source process
/*! This class is used to build a souce process which only has an output.
 * Given a file name and a function, the process repeatedly reads lines
 * from the text file and applies the function to convert it to a value
 * which will be written to the output.
 * It can be used in test-benches.
 */
template <class T>
class file_source : public sdf_process,
                    public ForSyDe::detail::bindable<file_source<T>>
{
public:
    SDF_out<T> oport1;        ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<file_source<T>>::operator();
    auto out_ports() {return std::tie(oport1);}
    
    //! Type of the function to be passed to the process constructor
    typedef std::function<void(T&, const std::string&)> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * and writes the result using the output port
     */
    file_source(sc_module_name _name,   ///< process name
           functype _func,              ///< function to be passed
           std::string file_name        ///< the file name
          ) : sdf_process(_name), oport1("oport1"),
              file_name(file_name), _func(_func)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_func",func_name+std::string("_func")));
        arg_vec.push_back(std::make_tuple("file_name", file_name));
        arg_vec.push_back(std::make_tuple("o1toks", std::to_string(1)));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SDF::file_source";}
    
private:
    std::string file_name;
    
    std::string cur_str;        // The current string read from the input
    std::ifstream ifs;
    T* cur_val;
    
    //! The function passed to the process constructor
    functype _func;
    
    //Implementing the abstract semantics
    void init()
    {
        cur_val = new T;
        ifs.open(file_name);
        if (!ifs.is_open())
        {
            SC_REPORT_ERROR(name(),"cannot open the file.");
        }
    }
    
    void prep()
    {
        if (!getline(ifs,cur_str))
        {
            wait();
        }
    }
    
    void exec()
    {
        _func(*cur_val, cur_str);
    }
    
    void prod()
    {
        write_multiport(oport1, *cur_val);
    }
    
    void clean()
    {
        ifs.close();
        delete cur_val;
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
class vsource : public sdf_process,
                public ForSyDe::detail::bindable<vsource<T>>
{
public:
    SDF_out<T> oport1;     ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<vsource<T>>::operator();
    auto out_ports() {return std::tie(oport1);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which writes the result using the output
     * port.
     */
    vsource(sc_module_name _name,      ///< process name
            const std::vector<T>& in_vec  ///< Initial vector
            ) : sdf_process(_name), in_vec(in_vec)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << in_vec;
        arg_vec.push_back(std::make_tuple("in_vec", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SDF::vsource";}
    
private:
    std::vector<T> in_vec;
    
    typename std::vector<T>::iterator itr;

    //Implementing the abstract semantics
    void init()
    {
        itr = in_vec.begin();
    }
    
    void prep() {}
    
    void exec() {}
    
    void prod()
    {
        if (itr != in_vec.end())
        {
            write_multiport(oport1, *itr);
            ++itr;
        }
        else
            wait();
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
class sink : public sdf_process,
             public ForSyDe::detail::bindable<sink<T>>
{
public:
    // Carrier-U ports, not SDF ports. This constructor is one of the
    // four that SADF re-exports rather than repeating (see
    // sadf_process_constructors.hpp), because an SADF process is an SDF
    // process within any one scenario. Typing the ports SDF_in/SDF_out
    // would make an SADF model binding to them a narrowing, which the
    // D13 check would reject -- correctly, since the type would be
    // claiming something the component does not mean. It is generic over
    // the untimed carrier, so it says so, and SDF and SADF both reach it
    // by widening.
    UT::UT_in<T> iport1;         ///< port for the input channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<sink<T>>::operator();
    auto in_ports()  {return std::tie(iport1);}
    
    //! Type of the function to be passed to the process constructor
    typedef std::function<void(const T&)> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * in each cycle.
     */
    sink(sc_module_name _name,      ///< process name
         functype _func             ///< function to be passed
        ) : sdf_process(_name), iport1("iport1"), _func(_func)
            
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_func",func_name+std::string("_func")));
        arg_vec.push_back(std::make_tuple("i1toks", std::to_string(1)));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SDF::sink";}
    
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
        ForSyDe::detail::record_ports(boundInChans, in_ports());
    }
#endif
};

//! Process constructor for a file_sink process
/*! This class is used to build a file_sink process which only has an input.
 * Its main purpose is to be used in test-benches. The process repeatedly
 * passes the current input to a given function to generate a string and
 * write the string to a new line of an output file.
 */
template <class T>
class file_sink : public sdf_process,
                  public ForSyDe::detail::bindable<file_sink<T>>
{
public:
    SDF_in<T> iport1;         ///< port for the input channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<file_sink<T>>::operator();
    auto in_ports()  {return std::tie(iport1);}
    
    //! Type of the function to be passed to the process constructor
    typedef std::function<void(std::string&, const T&)> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * in each cycle.
     */
    file_sink(sc_module_name _name, ///< process name
         functype _func,            ///< function to be passed
         std::string file_name      ///< the file name
        ) : sdf_process(_name), iport1("iport1"), file_name(file_name),
            _func(_func)
            
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_func",func_name+std::string("_func")));
        arg_vec.push_back(std::make_tuple("file_name", file_name));
        arg_vec.push_back(std::make_tuple("i1toks", std::to_string(1)));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SDF::file_sink";}
    
private:
    std::string file_name;
    
    std::string ostr;        // The current string to be written to the output
    std::ofstream ofs;
    T* cur_val;         // The current state of the process

    //! The function passed to the process constructor
    functype _func;
    
    //Implementing the abstract semantics
    void init()
    {
        cur_val = new T;
        ofs.open(file_name);
        if (!ofs.is_open())
        {
            SC_REPORT_ERROR(name(),"cannot open the file.");
        }
    }
    
    void prep()
    {
        *cur_val = iport1.read();
    }
    
    void exec()
    {
        _func(ostr, *cur_val);
    }
    
    void prod()
    {
        ofs << ostr << std::endl;
    }
    
    void clean()
    {
        ofs.close();
        delete cur_val;
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

//! The zip process with two inputs and one output
/*! This process "zips" two incoming signals into one signal of tuples.
 */
template <class T1, class T2>
class zip : public detail::zip_core<zip<T1,T2>,
                                    std::tuple<std::vector<T1>,std::vector<T2>>>
{
    typedef detail::zip_core<zip<T1,T2>,
                             std::tuple<std::vector<T1>,std::vector<T2>>> base;
    friend base;
public:
    SDF_in<T1> iport1;        ///< port for the input channel 1
    SDF_in<T2> iport2;        ///< port for the input channel 2

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input ports,
     * zips them together and writes the results using the output port
     */
    zip(sc_module_name _name,       ///< process name
        unsigned int i1toks,        ///< consumption rate for the first input
        unsigned int i2toks         ///< consumption rate for the second input
    ) : base(_name,{i1toks,i2toks}), iport1("iport1"), iport2("iport2")
    {
#ifdef FORSYDE_INTROSPECTION
        this->arg_vec.push_back(std::make_tuple("i1toks",std::to_string(i1toks)));
        this->arg_vec.push_back(std::make_tuple("i2toks",std::to_string(i2toks)));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SDF::zip";}

private:
public:
    auto in_ports() {return std::tie(iport1,iport2);}
private:
};

//! The zip process with variable number of inputs and one output
/*! This process "zips" the incoming signals into one signal of tuples.
 */
template <class... Ts>
class zipN : public detail::zip_core<zipN<Ts...>, std::tuple<std::vector<Ts>...>>
{
    typedef detail::zip_core<zipN<Ts...>, std::tuple<std::vector<Ts>...>> base;
    friend base;
public:
    std::tuple <SDF_in<Ts>...> iport;///< tuple of ports for the input channels

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * zips them together and writes the results using the output port
     */
    zipN(sc_module_name _name,                          ///< process name
         std::array<size_t, sizeof...(Ts)> in_toks      ///< consumption rates
         ) : base(_name,in_toks)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << in_toks;
        this->arg_vec.push_back(std::make_tuple("itoks",ss.str()));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SDF::zipN";}

private:
public:
    auto in_ports() {return std::apply([](auto&... p){return std::tie(p...);}, iport);}
private:
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
    SDF_out<T1> oport1;        ///< port for the output channel 1
    SDF_out<T2> oport2;        ///< port for the output channel 2

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input ports,
     * unzips them and writes the results using the output ports
     */
    unzip(sc_module_name _name,         ///< process name
           unsigned int o1toks,      ///< consumption rate for the first output
           unsigned int o2toks       ///< consumption rate for the second output
           ) : base(_name), oport1("oport1"), oport2("oport2")
    {
#ifdef FORSYDE_INTROSPECTION
        this->arg_vec.push_back(std::make_tuple("o1toks",std::to_string(o1toks)));
        this->arg_vec.push_back(std::make_tuple("o2toks",std::to_string(o2toks)));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SDF::unzip";}

private:
public:
    auto out_ports() {return std::tie(oport1,oport2);}
private:
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
    std::tuple<SDF_out<Ts>...> oport;///< tuple of ports for the output channels

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * unzips it and writes the results using the output ports
     */
    unzipN(sc_module_name _name,                        ///< process name
            std::array<size_t, sizeof...(Ts)> out_toks  ///< production rates
            ) : base(_name)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << out_toks;
        this->arg_vec.push_back(std::make_tuple("otoks",ss.str()));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SDF::unzipN";}

private:
public:
    auto out_ports() {return std::apply([](auto&... p){return std::tie(p...);}, oport);}
private:
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
class fanout : public sdf_process,
               public ForSyDe::detail::bindable<fanout<T>>
{
public:
    SDF_in<T> iport1;        ///< port for the input channel
    SDF_out<T> oport1;       ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<fanout<T>>::operator();
    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies and writes the results using the output port
     */
    fanout(sc_module_name _name)  // module name
         : sdf_process(_name) { }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SDF::fanout";}
    
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
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

}
}

#endif
