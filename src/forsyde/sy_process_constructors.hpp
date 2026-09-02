/**********************************************************************           
    * sy_process_constructors.hpp -- Process constructors in the SY   *
    *                                MOC                              *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Providing basic process constructors for modeling      *
    *          synchronous systems in ForSyDe-SystemC                 *
    *                                                                 *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef SY_PROCESS_CONSTRUCTORS_HPP
#define SY_PROCESS_CONSTRUCTORS_HPP

/*! \file sy_process_constructors.hpp
 * \brief Implements the basic process constructors in the SY MoC
 * 
 *  This file includes the basic process constructors used for modeling
 * in the synchronous model of computation.
 */

#include <systemc>
#include <functional>
#include <tuple>
#include <array>
#include <algorithm>
// std::index_sequence (combX's port pack) and std::decay (comb_core's
// bind_all) -- both used directly here rather than left to whichever
// other header happens to drag them in.
#include <utility>
#include <type_traits>

// Streams a std::vector under FORSYDE_INTROSPECTION via prettyprint.hpp's
// generic container operator<<, which this file otherwise relies on
// forsyde.hpp having included first.
#include "prettyprint.hpp"

#include "abst_ext.hpp"
#include "sy_process.hpp"

namespace ForSyDe
{

namespace SY
{

using namespace sc_core;

namespace detail
{

#ifdef FORSYDE_INTROSPECTION
//! Record a tuple of port references in one of a process's bound-channel vectors
/*! Shared by all three cores below: whatever shape a family's ports are
 * declared in, bindInfo() sees them as one flat tuple of references in
 * port order, which is the order the introspection XML lists them in.
 */
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

//! How a family's token pack relates to what its ports actually carry
/*! An SY signal always carries abst_ext<T>. What differs between a
 * process constructor and its strict counterpart is not the signal, the
 * port, or the loop -- it is only whether the user's function is shown
 * the absent-extended token or the value inside it:
 *
 *  - \c total  keeps the token as it came off the port, so the token
 *              pack holds abst_ext<T> and the function decides for
 *              itself what an absent event means;
 *  - \c strict unwraps it on the way in and re-wraps it on the way out,
 *              so the token pack holds a plain T and the function never
 *              sees an absent event at all -- receiving one is an error
 *              raised here, before the function is ever called.
 *
 * That is the whole of what "strict" ever meant. It used to be spelled
 * as a parallel copy of the entire MoC (sy_process_constructors_strict.hpp,
 * 2,154 lines beside SY's own 2,282), one s-prefixed class per class,
 * each repeating the same read/apply/write loop with the unwrapping
 * inlined into it. It is a two-value policy on the conduit instead.
 */
enum class token_policy {total, strict};

//! Read one token from an input port, as \a Policy sees it
template <token_policy Policy, typename Val, typename Port>
inline void read_one(Val& val, Port& port, const char* pname)
{
    auto tok = port.read();
    if constexpr (Policy == token_policy::strict)
    {
        // The strict classes each open-coded this as CHECK_PRESENCE,
        // whose expansion needs to sit in a member function to reach
        // this->name(); passing the name in lets it live here instead.
        if (is_absent(tok))
            SC_REPORT_ERROR(pname, "Unexpected absent value received in");
        val = unsafe_from_abst_ext(tok);
    }
    else
        val = tok;
}

//! Read one token from each of a tuple of input ports into a token pack
template <token_policy Policy = token_policy::total, typename Ports, typename Vals>
inline void read_all(Ports&& ports, Vals& vals, const char* pname = "")
{
    std::apply([&](auto&&... port){
        std::apply([&](auto&&... val){
            (read_one<Policy>(val, port, pname), ...);
        }, vals);
    }, ports);
}

//! Write one token to an output port, as \a Policy produces it
template <token_policy Policy, typename Port, typename Val>
inline void write_one(Port& port, const Val& val)
{
    if constexpr (Policy == token_policy::strict)
        write_multiport(port, abst_ext<Val>(val));
    else
        write_multiport(port, val);
}

//! Write one token from a token pack to each of a tuple of output ports
template <token_policy Policy = token_policy::total, typename Ports, typename Vals>
inline void write_all(Ports&& ports, const Vals& vals)
{
    std::apply([&](auto&&... port){
        std::apply([&](auto&&... val){
            (write_one<Policy>(port, val), ...);
        }, vals);
    }, ports);
}

//! Shared implementation of the SY combinational (comb*) family
/*! Every process in this family does the same three things on every
 * activation -- read one token from each input port, apply the user
 * function, write one token to each output port -- and the same one
 * thing once, at the end of elaboration: register both port sets for
 * introspection. The only things that vary between comb, comb2 ...
 * combN and combMN are how many ports there are, what they are named,
 * and how the user function is spelled. Those stay in the derived
 * classes below; the semantics live here, once.
 *
 * \a Derived supplies three members, and this class is a friend of it so
 * that they can stay private:
 *   - \c in_ports()  -- a tuple of references to its input ports, in the
 *                       same order as \a IVals;
 *   - \c out_ports() -- likewise for its output ports and \a OVals;
 *   - \c exec()      -- the call to the user function.
 *
 * \a OVals and \a IVals hold one token per port. They only have to model
 * the tuple protocol, so a std::array is as good as a std::tuple and is
 * what combX uses -- its user function is handed the whole array.
 *
 * The ports themselves are deliberately *not* pulled up here. Their
 * SystemC names appear verbatim in the introspection XML as
 * <port name="iport1" .../>, and only an individually declared member
 * can be given a name at construction -- an element of a std::tuple or
 * std::array cannot be. Keeping the port declarations in the derived
 * classes is what makes this refactoring invisible in the generated XML.
 */
template <typename Derived, typename OVals, typename IVals,
          token_policy Policy = token_policy::total>
class comb_core : public sy_process
{
protected:
    OVals ovals;    ///< output tokens, one per output port
    IVals ivals;    ///< input tokens, one per input port

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input ports,
     * applies the user-implemented function to them and writes the
     * results using the output ports.
     */
    comb_core(const sc_module_name& _name) : sy_process(_name)
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

    // The token variables used to be allocated with new here and freed
    // in clean(), in every one of the seven classes below. They are
    // plain members now, so there is nothing left for either stage to
    // do -- but both are pure virtual in ForSyDe::process, so both still
    // have to be defined.
    void init() {}

    void clean() {}

    void prep() {read_all<Policy>(self().in_ports(), ivals, name());}

    void prod() {write_all<Policy>(self().out_ports(), ovals);}

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        bind_all(boundInChans, self().in_ports());
        bind_all(boundOutChans, self().out_ports());
    }
#endif
};

//! Shared implementation of the SY zip family
/*! zip, zipX and zipN all read one token from each of their input ports
 * and write the whole collection as a single token on one output port.
 * \a Pack is that collection -- a std::tuple for zip and zipN, a
 * std::array for zipX -- and is both the input token pack and the token
 * type of the output port, which is why the output port lives here
 * rather than in the derived class. \a Derived supplies only in_ports().
 *
 * \a PropagatesAbsence says what to emit when *every* input token is
 * absent: an absent output token, or a present \a Pack whose elements
 * happen all to be absent. It exists solely because zip and zipX do the
 * former and zipN does the latter, which is an inconsistency in the
 * library as it stands rather than a designed distinction -- preserved
 * here verbatim, and marked, rather than quietly unified. It is
 * unobservable through a matching unzip (which looks at the elements,
 * not at the pack) and observable through anything else.
 */
template <typename Derived, typename Pack, bool PropagatesAbsence,
          token_policy Policy = token_policy::total>
class zip_core : public sy_process
{
public:
    SY_out<Pack> oport1;    ///< port for the output channel

protected:
    Pack ivals;             ///< input tokens, one per input port

    zip_core(const sc_module_name& _name) : sy_process(_name), oport1("oport1") {}

private:
    Derived& self() {return static_cast<Derived&>(*this);}

    void init() {}

    void clean() {}

    void exec() {}

    void prep() {read_all<Policy>(self().in_ports(), ivals, name());}

    void prod()
    {
        if constexpr (PropagatesAbsence)
        {
            if (std::apply([](auto&... val){return (val.is_absent() && ...);}, ivals))
            {
                write_multiport(oport1, abst_ext<Pack>());
                return;
            }
        }
        write_multiport(oport1, abst_ext<Pack>(ivals));
    }

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        bind_all(boundInChans, self().in_ports());
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
    }
#endif
};

//! Shared implementation of the SY unzip family
/*! The mirror image of zip_core: unzip, unzipX and unzipN all read a
 * single \a Pack token from one input port -- which is why that port
 * lives here -- and write its elements one per output port, emitting an
 * absent token on every output when the input token is absent. All three
 * agree on that, so unlike zip_core there is no PropagatesAbsence
 * parameter. \a Derived supplies only out_ports().
 */
template <typename Derived, typename Pack,
          token_policy Policy = token_policy::total>
class unzip_core : public sy_process
{
public:
    SY_in<Pack> iport1;     ///< port for the input channel

protected:
    //! The token read from iport1
    /*! Under the strict policy read_one() has already unwrapped and
     * presence-checked it, so this is the bare pack; under the total
     * policy it is still absent-extended and prod() has to decide what
     * an absent input means for each output.
     */
    typename std::conditional<Policy == token_policy::strict,
                              Pack, abst_ext<Pack>>::type in_val;

    Pack ovals;             ///< output tokens, one per output port

    unzip_core(const sc_module_name& _name) : sy_process(_name), iport1("iport1") {}

private:
    Derived& self() {return static_cast<Derived&>(*this);}

    void init() {}

    void clean() {}

    void exec() {}

    void prep() {read_one<Policy>(in_val, iport1, name());}

    void prod()
    {
        if constexpr (Policy == token_policy::strict)
            // A strict unzip cannot have read an absent token -- prep()
            // raises rather than returning one -- so there is no absent
            // case to spread across the outputs, and in_val already holds
            // the unwrapped pack.
            write_all<Policy>(self().out_ports(), in_val);
        else
        {
            // A default-constructed Pack is a pack of absent tokens, which
            // is exactly what an absent input has to produce on every output.
            ovals = in_val.is_absent() ? Pack() : in_val.unsafe_from_abst_ext();
            write_all<Policy>(self().out_ports(), ovals);
        }
    }

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

//! Process constructor for a combinational process with one input and one output
/*! This class is used to build combinational processes with one input
 * and one output. The class is parameterized for input and output
 * data-types.
 */
template <typename T0, typename T1>
class comb : public detail::comb_core<comb<T0,T1>,
                                      std::tuple<abst_ext<T0>>,
                                      std::tuple<abst_ext<T1>>>
{
    typedef detail::comb_core<comb<T0,T1>,
                              std::tuple<abst_ext<T0>>,
                              std::tuple<abst_ext<T1>>> base;
    friend base;
public:
    SY_in<T1>  iport1;       ///< port for the input channel
    SY_out<T0> oport1;        ///< port for the output channel

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(abst_ext<T0>&,const abst_ext<T1>&)> functype;

    //! The constructor requires the module name
    comb(const sc_module_name& _name,      ///< process name
         const functype& _func             ///< function to be passed
         ) : base(_name), iport1("iport1"), oport1("oport1"), _func(_func) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::comb";}

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
                                       std::tuple<abst_ext<T0>>,
                                       std::tuple<abst_ext<T1>,abst_ext<T2>>>
{
    typedef detail::comb_core<comb2<T0,T1,T2>,
                              std::tuple<abst_ext<T0>>,
                              std::tuple<abst_ext<T1>,abst_ext<T2>>> base;
    friend base;
public:
    SY_in<T1> iport1;        ///< port for the input channel 1
    SY_in<T2> iport2;        ///< port for the input channel 2
    SY_out<T0> oport1;        ///< port for the output channel

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(abst_ext<T0>&, const abst_ext<T1>&,
                                              const abst_ext<T2>&)> functype;

    //! The constructor requires the module name
    comb2(const sc_module_name& _name,      ///< process name
           const functype& _func             ///< function to be passed
          ) : base(_name), iport1("iport1"), iport2("iport2"), oport1("oport1"),
              _func(_func) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::comb2";}

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
                                       std::tuple<abst_ext<T0>>,
                                       std::tuple<abst_ext<T1>,abst_ext<T2>,abst_ext<T3>>>
{
    typedef detail::comb_core<comb3<T0,T1,T2,T3>,
                              std::tuple<abst_ext<T0>>,
                              std::tuple<abst_ext<T1>,abst_ext<T2>,abst_ext<T3>>> base;
    friend base;
public:
    SY_in<T1> iport1;        ///< port for the input channel 1
    SY_in<T2> iport2;        ///< port for the input channel 2
    SY_in<T3> iport3;        ///< port for the input channel 3
    SY_out<T0> oport1;        ///< port for the output channel

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(abst_ext<T0>&, const abst_ext<T1>&,
                                              const abst_ext<T2>&,
                                              const abst_ext<T3>&)> functype;

    //! The constructor requires the module name
    comb3(const sc_module_name& _name,      ///< process name
           const functype& _func             ///< function to be passed
          ) : base(_name), iport1("iport1"), iport2("iport2"), iport3("iport3"),
              oport1("oport1"), _func(_func) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::comb3";}

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
                                       std::tuple<abst_ext<T0>>,
                                       std::tuple<abst_ext<T1>,abst_ext<T2>,abst_ext<T3>,abst_ext<T4>>>
{
    typedef detail::comb_core<comb4<T0,T1,T2,T3,T4>,
                              std::tuple<abst_ext<T0>>,
                              std::tuple<abst_ext<T1>,abst_ext<T2>,abst_ext<T3>,abst_ext<T4>>> base;
    friend base;
public:
    SY_in<T1> iport1;       ///< port for the input channel 1
    SY_in<T2> iport2;       ///< port for the input channel 2
    SY_in<T3> iport3;       ///< port for the input channel 3
    SY_in<T4> iport4;       ///< port for the input channel 4
    SY_out<T0> oport1;        ///< port for the output channel

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(abst_ext<T0>&, const abst_ext<T1>&,
                                             const abst_ext<T2>&,
                                             const abst_ext<T3>&,
                                             const abst_ext<T4>&)> functype;

    //! The constructor requires the module name
    comb4(const sc_module_name& _name,      ///< process name
           const functype& _func             ///< function to be passed
          ) : base(_name), iport1("iport1"), iport2("iport2"),
              iport3("iport3"), iport4("iport4"), _func(_func) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "SY::comb4";}

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

//! Process constructor for a combinational process with an array of inputs and one output
/*! similar to comb with an array of inputs
 */
template <typename T0, typename T1, std::size_t N>
class combX : public detail::comb_core<combX<T0,T1,N>,
                                       std::tuple<abst_ext<T0>>,
                                       std::array<abst_ext<T1>,N>>
{
    typedef detail::comb_core<combX<T0,T1,N>,
                              std::tuple<abst_ext<T0>>,
                              std::array<abst_ext<T1>,N>> base;
    friend base;
public:
    std::array<SY_in<T1>,N> iport;       ///< port for the input channel 1
    SY_out<T0> oport1;        ///< port for the output channel

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(abst_ext<T0>&, const std::array<abst_ext<T1>,N>&)> functype;

    //! The constructor requires the module name
    combX(const sc_module_name& _name,      ///< process name
           const functype& _func             ///< function to be passed
          ) : base(_name), _func(_func) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "SY::combX";}

private:
    //! The function passed to the process constructor
    functype _func;

    template <std::size_t... Is>
    auto in_ports(std::index_sequence<Is...>) {return std::tie(iport[Is]...);}
    auto in_ports()  {return in_ports(std::make_index_sequence<N>{});}
    auto out_ports() {return std::tie(oport1);}

    void exec() {_func(std::get<0>(this->ovals), this->ivals);}
};

//! Process constructor for a combinational process with N inputs and one output
/*! similar to comb with N inputs
 */
template <typename T0, typename... Ts>
class combN : public detail::comb_core<combN<T0,Ts...>,
                                       std::tuple<abst_ext<T0>>,
                                       std::tuple<abst_ext<Ts>...>>
{
    typedef detail::comb_core<combN<T0,Ts...>,
                              std::tuple<abst_ext<T0>>,
                              std::tuple<abst_ext<Ts>...>> base;
    friend base;
public:
    std::tuple <SY_in<Ts>...> iport;///< tuple of ports for the input channels
    SY_out<T0> oport1;              ///< port for the output channel

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(abst_ext<T0>&, const std::tuple<abst_ext<Ts>...>&)> functype;

    //! The constructor requires the module name
    combN(const sc_module_name& _name,      ///< process name
           const functype& _func             ///< function to be passed
          ) : base(_name), oport1("oport1"), _func(_func) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "SY::combN";}

private:
    //! The function passed to the process constructor
    functype _func;

    auto in_ports()  {return std::apply([](auto&... p){return std::tie(p...);}, iport);}
    auto out_ports() {return std::tie(oport1);}

    void exec() {_func(std::get<0>(this->ovals), this->ivals);}
};

//! Process constructor for a combinational process with M inputs and N outputs
/*! similar to comb with M inputs and an unzip with N outputs
 */
template<typename TO_tuple, typename TI_tuple> class combMN;

template <typename... TOs, typename... TIs>
class combMN<std::tuple<TOs...>,std::tuple<TIs...>>
    : public detail::comb_core<combMN<std::tuple<TOs...>,std::tuple<TIs...>>,
                               std::tuple<abst_ext<TOs>...>,
                               std::tuple<abst_ext<TIs>...>>
{
    typedef detail::comb_core<combMN<std::tuple<TOs...>,std::tuple<TIs...>>,
                              std::tuple<abst_ext<TOs>...>,
                              std::tuple<abst_ext<TIs>...>> base;
    friend base;
public:
    std::tuple<SY_in<TIs>...>  iport;///< tuple of ports for the input channels
    std::tuple<SY_out<TOs>...> oport;///< tuple of ports for the output channels

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(std::tuple<abst_ext<TOs>...>&, const std::tuple<abst_ext<TIs>...>&)> functype;

    //! The constructor requires the module name
    combMN(const sc_module_name& _name,      ///< process name
           const functype& _func             ///< function to be passed
          ) : base(_name), _func(_func) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "SY::combMN";}

private:
    //! The function passed to the process constructor
    functype _func;

    auto in_ports()  {return std::apply([](auto&... p){return std::tie(p...);}, iport);}
    auto out_ports() {return std::apply([](auto&... p){return std::tie(p...);}, oport);}

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
class delay : public sy_process
{
public:
    SY_in<T>  iport1;       ///< port for the input channel
    SY_out<T> oport1;        ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which inserts the initial element, reads
     * data from its input port, and writes the results using the output
     * port.
     */
    delay(const sc_module_name& _name,      ///< process name
           const abst_ext<T>& init_val      ///< initial value
          ) : sy_process(_name), iport1("iport1"), oport1("oport1"),
              init_val(init_val)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << init_val;
        arg_vec.push_back(std::make_tuple("init_val", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::delay";}
    
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
class delayn : public sy_process
{
public:
    SY_in<T>  iport1;       ///< port for the input channel
    SY_out<T> oport1;        ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which inserts the initial elements,
     * reads data from its input port, and writes the results using the
     * output port.
     */
    delayn(const sc_module_name& _name,      ///< process name
            const abst_ext<T>& init_val,    ///< initial value
            const unsigned int& n            ///< number of delay elements
          ) : sy_process(_name), iport1("iport1"), oport1("oport1"),
              init_val(init_val), ns(n)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << init_val;
        arg_vec.push_back(std::make_tuple("init_val", ss.str()));
        ss.str("");
        ss << n;
        arg_vec.push_back(std::make_tuple("n", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::delayn";}
    
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
        for (int i=0; i<ns; i++)
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

//! Process constructor for a Moore machine
/*! This class is used to build a finite state machine of type Moore.
 * Given an initial state, a next-state function, and an output decoding
 * function it creates a Moore process.
 */
template <class IT, class ST, class OT>
class moore : public sy_process
{
public:
    SY_in<IT>  iport1;        ///< port for the input channel
    SY_out<OT> oport1;        ///< port for the output channel
    
    //! Type of the next-state function to be passed to the process constructor
    typedef std::function<void(ST&, const ST&, const abst_ext<IT>&)> ns_functype;
    
    //! Type of the output-decoding function to be passed to the process constructor
    typedef std::function<void(abst_ext<OT>&, const ST&)> od_functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented functions to the input and current
     * state and writes the results using the output port
     */
    moore(const sc_module_name& _name,      ///< process name
           const ns_functype& _ns_func, ///< The next_state function
           const od_functype& _od_func, ///< The output-decoding function
           const ST& init_st  ///< Initial state
          ) : sy_process(_name), _ns_func(_ns_func), _od_func(_od_func),
              init_st(init_st)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_ns_func",func_name+std::string("_ns_func")));
        arg_vec.push_back(std::make_tuple("_od_func",func_name+std::string("_od_func")));
        std::stringstream ss;
        ss << init_st;
        arg_vec.push_back(std::make_tuple("init_st",ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "SY::moore";}
    
private:
    //! The functions passed to the process constructor
    ns_functype _ns_func;
    od_functype _od_func;
    // Initial value
    ST init_st;
    
    bool first_run;
    
    // Input, output, current state, and next state variables
    abst_ext<IT>* ival;
    ST* stval;
    ST* nsval;
    abst_ext<OT>* oval;

    //Implementing the abstract semantics
    void init()
    {
        ival = new abst_ext<IT>;
        stval = new ST;
        *stval = init_st;
        nsval = new ST;
        oval = new abst_ext<OT>;
        // First evaluation cycle
        first_run = true;
    }
    
    void prep()
    {
        if (!first_run)
            *ival = iport1.read();
    }
    
    void exec()
    {
        if (first_run)
            first_run = false;
        else
        {
            _ns_func(*nsval, *stval, *ival);
            *stval = *nsval;
        }
        _od_func(*oval, *stval);
    }
    
    void prod()
    {
        write_multiport(oport1, *oval);
    }
    
    void clean()
    {
        delete ival;
        delete stval;
        delete nsval;
        delete oval;
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

//! Process constructor for a Mealy machine
/*! This class is used to build a finite state machine of type Mealy.
 * Given an initial state, a next-state function, and an output decoding
 * function it creates a Mealy process.
 */
template <class IT, class ST, class OT>
class mealy : public sy_process
{
public:
    SY_in<IT>  iport1;        ///< port for the input channel
    SY_out<OT> oport1;        ///< port for the output channel
    
    //! Type of the next-state function to be passed to the process constructor
    typedef std::function<void(ST&, const ST&, 
                                                const abst_ext<IT>&)> ns_functype;
    
    //! Type of the output-decoding function to be passed to the process constructor
    typedef std::function<void(abst_ext<OT>&, const ST&,
                                                const abst_ext<IT>&)> od_functype;
    
    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented functions to the input and current
     * state and writes the results using the output port
     */
    mealy(const sc_module_name& _name,      ///< process name
           const ns_functype& _ns_func, ///< The next_state function
           const od_functype& _od_func, ///< The output-decoding function
           const ST& init_st  ///< Initial state
          ) : sy_process(_name), _ns_func(_ns_func), _od_func(_od_func),
              init_st(init_st)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_ns_func",func_name+std::string("_ns_func")));
        arg_vec.push_back(std::make_tuple("_od_func",func_name+std::string("_od_func")));
        std::stringstream ss;
        ss << init_st;
        arg_vec.push_back(std::make_tuple("init_st",ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "SY::mealy";}
    
private:
    //! The functions passed to the process constructor
    ns_functype _ns_func;
    od_functype _od_func;
    // Initial value
    ST init_st;
    
    // Input, output, current state, and next state variables
    abst_ext<IT>* ival;
    ST* stval;
    ST* nsval;
    abst_ext<OT>* oval;

    //Implementing the abstract semantics
    void init()
    {
        ival = new abst_ext<IT>;
        stval = new ST;
        *stval = init_st;
        nsval = new ST;
        oval = new abst_ext<OT>;
    }
    
    void prep()
    {
        *ival = iport1.read();
    }
    
    void exec()
    {
        _od_func(*oval, *stval, *ival);
        _ns_func(*nsval, *stval, *ival);
        *stval = *nsval;
    }
    
    void prod()
    {
        write_multiport(oport1, *oval);
    }
    
    void clean()
    {
        delete ival;
        delete stval;
        delete nsval;
        delete oval;
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

//! Process constructor for a fill process
/*! The process constructor fill creates a process that fills an absent-
 * extended signal with present values by replacing absent values with a
 * given value.
 */
template <class T>
class fill : public sy_process
{
public:
    SY_in<T> iport1;              ///< port for the input channel
    SY_out<T> oport1;             ///< port for the output channel

    //! The constructor requires the process name and a default value
    /*! It creates an SC_THREAD which fills the signal result using the
     * output port
     */
    fill(const sc_module_name& _name,      ///< process name
          const T& def_val                  ///< default value
         ) : sy_process(_name), def_val(def_val)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << def_val;
        arg_vec.push_back(std::make_tuple("def_val", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::fill";}
    
private:
    // The default value
    T def_val;
    
    // Inputs and output variables
    abst_ext<T>* ival;
    abst_ext<T>* oval;

    //Implementing the abstract semantics
    void init()
    {
        ival = new abst_ext<T>;
        oval = new abst_ext<T>;
    }
    
    void prep()
    {
        *ival = iport1.read();
    }
    
    void exec()
    {
        *oval = abst_ext<T>(ival->from_abst_ext(def_val));
    }
    
    void prod()
    {
        write_multiport(oport1, *oval);
    }
    
    void clean()
    {
        delete ival;
        delete oval;
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

//! Process constructor for a hold process
/*! The process constructor hold creates a process that fills an absent-
 * extended signal with values by replacing absent values by the
 * preceding present value. Only in cases, where no preceding value
 * exists, the absent value is replaced by a default value.
 */
template <class T>
class hold : public sy_process
{
public:
    SY_in<T> iport1;              ///< port for the input channel
    SY_out<T> oport1;             ///< port for the output channel

    //! The constructor requires the process name and a default value
    /*! It creates an SC_THREAD which fills the signal result using the
     * output port
     */
    hold(const sc_module_name& _name,      ///< process name
          const T& def_val                   ///< default value
         ) : sy_process(_name), def_val(def_val)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << def_val;
        arg_vec.push_back(std::make_tuple("def_val", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::hold";}
    
private:
    // The efault value
    T def_val;
    
    // Input and default output variables
    abst_ext<T>* ival;
    abst_ext<T>* oval;

    //Implementing the abstract semantics
    void init()
    {
        ival = new abst_ext<T>;
        oval = new abst_ext<T>;
        *oval = abst_ext<T>(def_val);
    }
    
    void prep()
    {
        *ival = iport1.read();
    }
    
    void exec()
    {
        *oval = ival->is_present() ? *ival : *oval;
    }
    
    void prod()
    {
        write_multiport(oport1, *oval);
    }
    
    void clean()
    {
        delete ival;
        delete oval;
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

//! Process constructor for a constant source process
/*! This class is used to build a souce process with constant output.
 * Its main purpose is to be used in test-benches.
 * 
 * This class can directly be instantiated to build a process.
 */
template <class T>
class constant : public sy_process
{
public:
    SY_out<T> oport1;            ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * and writes the result using the output port
     */
    constant(const sc_module_name& _name,      ///< process name
              const abst_ext<T>& init_val,     ///< The constant output value
              const unsigned long long& take=0 ///< number of tokens produced (0 for infinite)
             ) : sy_process(_name), oport1("oport1"),
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
    std::string forsyde_kind() const {return "SY::constant";}
    
private:
    abst_ext<T> init_val;
    unsigned long long take;    // Number of tokens produced
    
    unsigned long long tok_cnt;
    bool infinite;
    
    //Implementing the abstract semantics
    void init()
    {
        infinite = take==0 ? true : false;
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
class source : public sy_process
{
public:
    SY_out<T> oport1;        ///< port for the output channel
    
    //! Type of the function to be passed to the process constructor
    typedef std::function<void(abst_ext<T>&, const abst_ext<T>&)> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * and writes the result using the output port
     */
    source(const sc_module_name& _name,      ///< process name
            const functype& _func,         ///< function to be passed
            const abst_ext<T>& init_val,    ///< Initial state
            const unsigned long long& take=0 ///< number of tokens produced (0 for infinite)
          ) : sy_process(_name), oport1("oport1"),
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
    std::string forsyde_kind() const {return "SY::source";}
    
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
        infinite = take==0 ? true : false;
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

//! Process constructor for a file_source process
/*! This class is used to build a souce process which only has an output.
 * Given a file name and a function, the process repeatedly reads lines
 * from the text file and applies the function to convert it to a value
 * which will be written to the output.
 * It can be used in test-benches.
 */
template <class T>
class file_source : public sy_process
{
public:
    SY_out<T> oport1;        ///< port for the output channel
    
    //! Type of the function to be passed to the process constructor
    typedef std::function<void(abst_ext<T>&, const std::string&)> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * and writes the result using the output port
     */
    file_source(sc_module_name _name,   ///< process name
           functype _func,              ///< function to be passed
           std::string file_name        ///< the file name
          ) : sy_process(_name), oport1("oport1"),
              file_name(file_name), _func(_func)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_func",func_name+std::string("_func")));
        arg_vec.push_back(std::make_tuple("file_name", file_name));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::file_source";}
    
private:
    std::string file_name;
    
    std::string cur_str;        // The current string read from the input
    std::ifstream ifs;
    abst_ext<T>* cur_val;
    
    //! The function passed to the process constructor
    functype _func;
    
    //Implementing the abstract semantics
    void init()
    {
        cur_val = new abst_ext<T>;
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
template <class T>
class vsource : public sy_process
{
public:
    SY_out<T> oport1;     ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which writes the result using the output
     * port.
     */
    vsource(const sc_module_name& _name,      ///< process name
            const std::vector<abst_ext<T>>& in_vec  ///< Initial vector
            ) : sy_process(_name), in_vec(in_vec)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << in_vec;
        arg_vec.push_back(std::make_tuple("in_vec", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::vsource";}
    
private:
    std::vector<abst_ext<T>> in_vec;
    
    unsigned long tok_cnt;

    //Implementing the abstract semantics
    void init()
    {
        tok_cnt = 0;
    }
    
    void prep() {}
    
    void exec() {}
    
    void prod()
    {
        if (tok_cnt < in_vec.size())
        {
            write_multiport(oport1, in_vec[tok_cnt]);
            tok_cnt++;
        }
        else
            wait();
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

//! Process constructor for a sink process
/*! This class is used to build a sink process which only has an input.
 * Its main purpose is to be used in test-benches. The process repeatedly
 * applies a given function to the current input.
 */
template <class T>
class sink : public sy_process
{
public:
    SY_in<T> iport1;         ///< port for the input channel
    
    //! Type of the function to be passed to the process constructor
    typedef std::function<void(const abst_ext<T>&)> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * in each cycle.
     */
    sink(const sc_module_name& _name,      ///< process name
          const functype& _func             ///< function to be passed
        ) : sy_process(_name), iport1("iport1"), _func(_func)
            
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_func",func_name+std::string("_func")));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::sink";}
    
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
        boundInChans.resize(1);    // only one output port
        boundInChans[0].port = &iport1;
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
class file_sink : public sy_process
{
public:
    SY_in<T> iport1;         ///< port for the input channel
    
    //! Type of the function to be passed to the process constructor
    typedef std::function<void(std::string&, const abst_ext<T>&)> functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which runs the user-imlpemented function
     * in each cycle.
     */
    file_sink(sc_module_name _name, ///< process name
         functype _func,            ///< function to be passed
         std::string file_name      ///< the file name
        ) : sy_process(_name), iport1("iport1"), file_name(file_name),
            _func(_func)
            
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_func",func_name+std::string("_func")));
        arg_vec.push_back(std::make_tuple("file_name", file_name));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::file_sink";}
    
private:
    std::string file_name;
    
    std::string ostr;        // The current string to be written to the output
    std::ofstream ofs;
    abst_ext<T>* cur_val;         // The current state of the process

    //! The function passed to the process constructor
    functype _func;
    
    //Implementing the abstract semantics
    void init()
    {
        cur_val = new abst_ext<T>;
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
        boundInChans.resize(1);    // only one output port
        boundInChans[0].port = &iport1;
    }
#endif
};

//! The zip process with two inputs and one output
/*! This process "zips" two incoming signals into one signal of tuples.
 */
template <class T1, class T2>
class zip : public detail::zip_core<zip<T1,T2>,
                                    std::tuple<abst_ext<T1>,abst_ext<T2>>, true>
{
    typedef detail::zip_core<zip<T1,T2>,
                             std::tuple<abst_ext<T1>,abst_ext<T2>>, true> base;
    friend base;
public:
    SY_in<T1> iport1;        ///< port for the input channel 1
    SY_in<T2> iport2;        ///< port for the input channel 2

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * zips them together and writes the results using the output port
     */
    zip(const sc_module_name& _name      ///< process name
        ) : base(_name), iport1("iport1"), iport2("iport2") {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::zip";}

private:
    auto in_ports() {return std::tie(iport1,iport2);}
};

//! The zipX process with an array of inputs and one output
/*! This process "zips" an array of incoming signals into one signal of arrays.
 */
template <class T1, std::size_t N>
class zipX : public detail::zip_core<zipX<T1,N>, std::array<abst_ext<T1>,N>, true>
{
    typedef detail::zip_core<zipX<T1,N>, std::array<abst_ext<T1>,N>, true> base;
    friend base;
public:
    std::array<SY_in<T1>,N> iport;              ///< port array for the input channels

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * zips them together and writes the results using the output port
     */
    zipX(const sc_module_name& _name      ///< process name
        ) : base(_name) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::zipX";}

private:
    template <std::size_t... Is>
    auto in_ports(std::index_sequence<Is...>) {return std::tie(iport[Is]...);}
    auto in_ports() {return in_ports(std::make_index_sequence<N>{});}
};

//! The zip process with variable number of inputs and one output
/*! This process "zips" the incoming signals into one signal of tuples.
 *
 * Unlike zip and zipX, this one has never emitted an absent output token
 * for an all-absent input -- see zip_core's PropagatesAbsence.
 */
template <class... Ts>
class zipN : public detail::zip_core<zipN<Ts...>, std::tuple<abst_ext<Ts>...>, false>
{
    typedef detail::zip_core<zipN<Ts...>, std::tuple<abst_ext<Ts>...>, false> base;
    friend base;
public:
    std::tuple <SY_in<Ts>...> iport;///< tuple of ports for the input channels

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * zips them together and writes the results using the output port
     */
    zipN(const sc_module_name& _name      ///< process name
         ) : base(_name) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::zipN";}

private:
    auto in_ports() {return std::apply([](auto&... p){return std::tie(p...);}, iport);}
};

//! The unzip process with one input and two outputs
/*! This process "unzips" a signal of tuples into two separate signals
 */
template <class T1, class T2>
class unzip : public detail::unzip_core<unzip<T1,T2>,
                                        std::tuple<abst_ext<T1>,abst_ext<T2>>>
{
    typedef detail::unzip_core<unzip<T1,T2>,
                               std::tuple<abst_ext<T1>,abst_ext<T2>>> base;
    friend base;
public:
    SY_out<T1> oport1;        ///< port for the output channel 1
    SY_out<T2> oport2;        ///< port for the output channel 2

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input ports,
     * unzips them and writes the results using the output ports
     */
    unzip(const sc_module_name& _name      ///< process name
          ) : base(_name), oport1("oport1"), oport2("oport2") {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::unzip";}

private:
    auto out_ports() {return std::tie(oport1,oport2);}
};

//! The unzipX process with one input and an array of outputs
/*! This process "unzips" a signal of arrays into an array of separate signals
 */
template <class T1, std::size_t N>
class unzipX : public detail::unzip_core<unzipX<T1,N>, std::array<abst_ext<T1>,N>>
{
    typedef detail::unzip_core<unzipX<T1,N>, std::array<abst_ext<T1>,N>> base;
    friend base;
public:
    std::array<SY_out<T1>,N> oport;///< port array for the output channels

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input ports,
     * unzips them and writes the results using the output ports
     */
    unzipX(const sc_module_name& _name      ///< process name
          ) : base(_name) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::unzipX";}

private:
    template <std::size_t... Is>
    auto out_ports(std::index_sequence<Is...>) {return std::tie(oport[Is]...);}
    auto out_ports() {return out_ports(std::make_index_sequence<N>{});}
};

//! The unzip process with one input and variable number of outputs
/*! This process "unzips" the incoming signal into a tuple of signals.
 */
template <class... Ts>
class unzipN : public detail::unzip_core<unzipN<Ts...>, std::tuple<abst_ext<Ts>...>>
{
    typedef detail::unzip_core<unzipN<Ts...>, std::tuple<abst_ext<Ts>...>> base;
    friend base;
public:
    std::tuple<SY_out<Ts>...> oport;///< tuple of ports for the output channels

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * unzips it and writes the results using the output ports
     */
    unzipN(const sc_module_name& _name      ///< process name
           ) : base(_name) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::unzipN";}

private:
    auto out_ports() {return std::apply([](auto&... p){return std::tie(p...);}, oport);}
};

// ---------------------------------------------------------------------
//  The strict variants.
//
//  Each of these used to be a full class of its own in
//  sy_process_constructors_strict.hpp, repeating the whole read /
//  apply / write loop of its counterpart above with the unwrapping
//  inlined into it. They are the same class, over a token pack of plain
//  values instead of absent-extended ones, under detail::token_policy::
//  strict -- which is what does the presence check on the way in and the
//  re-wrapping on the way out. See the token_policy comment above.
//
//  They keep their own forsyde_kind(), so a model's introspection output
//  still distinguishes an SY::scomb from an SY::comb.
// ---------------------------------------------------------------------

//! Process constructor for a strict combinational process with one input and one output
/*! The strict counterpart of comb: the function is handed the value
 * inside the token, and an absent input is an error rather than
 * something the function has to consider.
 */
template <typename T0, typename T1>
class scomb : public detail::comb_core<scomb<T0,T1>,
                                       std::tuple<T0>,
                                       std::tuple<T1>,
                                       detail::token_policy::strict>
{
    typedef detail::comb_core<scomb<T0,T1>,
                              std::tuple<T0>,
                              std::tuple<T1>,
                              detail::token_policy::strict> base;
    friend base;
public:
    SY_in<T1>  iport1;       ///< port for the input channel
    SY_out<T0> oport1;        ///< port for the output channel

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(T0&,const T1&)> functype;

    //! The constructor requires the module name
    scomb(const sc_module_name& _name,      ///< process name
         const functype& _func             ///< function to be passed
         ) : base(_name), iport1("iport1"), oport1("oport1"), _func(_func) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::scomb";}

private:
    //! The function passed to the process constructor
    functype _func;

    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}

    void exec() {_func(std::get<0>(this->ovals), std::get<0>(this->ivals));}
};

//! Process constructor for a strict combinational process with two inputs and one output
/*! similar to scomb with two inputs
 */
template <typename T0, typename T1, typename T2>
class scomb2 : public detail::comb_core<scomb2<T0,T1,T2>,
                                        std::tuple<T0>,
                                        std::tuple<T1,T2>,
                                        detail::token_policy::strict>
{
    typedef detail::comb_core<scomb2<T0,T1,T2>,
                              std::tuple<T0>,
                              std::tuple<T1,T2>,
                              detail::token_policy::strict> base;
    friend base;
public:
    SY_in<T1> iport1;        ///< port for the input channel 1
    SY_in<T2> iport2;        ///< port for the input channel 2
    SY_out<T0> oport1;        ///< port for the output channel

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(T0&, const T1&, const T2&)> functype;

    //! The constructor requires the module name
    scomb2(const sc_module_name& _name,      ///< process name
           const functype& _func             ///< function to be passed
          ) : base(_name), iport1("iport1"), iport2("iport2"), oport1("oport1"),
              _func(_func) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::scomb2";}

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

//! Process constructor for a strict combinational process with three inputs and one output
/*! similar to scomb with three inputs
 */
template <typename T0, typename T1, typename T2, typename T3>
class scomb3 : public detail::comb_core<scomb3<T0,T1,T2,T3>,
                                        std::tuple<T0>,
                                        std::tuple<T1,T2,T3>,
                                        detail::token_policy::strict>
{
    typedef detail::comb_core<scomb3<T0,T1,T2,T3>,
                              std::tuple<T0>,
                              std::tuple<T1,T2,T3>,
                              detail::token_policy::strict> base;
    friend base;
public:
    SY_in<T1> iport1;        ///< port for the input channel 1
    SY_in<T2> iport2;        ///< port for the input channel 2
    SY_in<T3> iport3;        ///< port for the input channel 3
    SY_out<T0> oport1;        ///< port for the output channel

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(T0&, const T1&, const T2&, const T3&)> functype;

    //! The constructor requires the module name
    scomb3(const sc_module_name& _name,      ///< process name
           const functype& _func             ///< function to be passed
          ) : base(_name), iport1("iport1"), iport2("iport2"), iport3("iport3"),
              oport1("oport1"), _func(_func) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::scomb3";}

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

//! Process constructor for a strict combinational process with four inputs and one output
/*! similar to scomb with four inputs
 */
template <typename T0, typename T1, typename T2, typename T3, typename T4>
class scomb4 : public detail::comb_core<scomb4<T0,T1,T2,T3,T4>,
                                        std::tuple<T0>,
                                        std::tuple<T1,T2,T3,T4>,
                                        detail::token_policy::strict>
{
    typedef detail::comb_core<scomb4<T0,T1,T2,T3,T4>,
                              std::tuple<T0>,
                              std::tuple<T1,T2,T3,T4>,
                              detail::token_policy::strict> base;
    friend base;
public:
    SY_in<T1> iport1;       ///< port for the input channel 1
    SY_in<T2> iport2;       ///< port for the input channel 2
    SY_in<T3> iport3;       ///< port for the input channel 3
    SY_in<T4> iport4;       ///< port for the input channel 4
    SY_out<T0> oport1;        ///< port for the output channel

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(T0&, const T1&, const T2&, const T3&, const T4&)> functype;

    //! The constructor requires the module name
    scomb4(const sc_module_name& _name,      ///< process name
           const functype& _func             ///< function to be passed
          ) : base(_name), iport1("iport1"), iport2("iport2"),
              iport3("iport3"), iport4("iport4"), oport1("oport1"), _func(_func) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "SY::scomb4";}

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

//! Process constructor for a strict combinational process with an array of inputs and one output
/*! similar to scomb with an array of inputs
 */
template <typename T0, typename T1, std::size_t N>
class scombX : public detail::comb_core<scombX<T0,T1,N>,
                                        std::tuple<T0>,
                                        std::array<T1,N>,
                                        detail::token_policy::strict>
{
    typedef detail::comb_core<scombX<T0,T1,N>,
                              std::tuple<T0>,
                              std::array<T1,N>,
                              detail::token_policy::strict> base;
    friend base;
public:
    std::array<SY_in<T1>,N> iport;       ///< port for the input channel 1
    SY_out<T0> oport1;        ///< port for the output channel

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(T0&, const std::array<T1,N>&)> functype;

    //! The constructor requires the module name
    scombX(const sc_module_name& _name,      ///< process name
           const functype& _func             ///< function to be passed
          ) : base(_name), _func(_func) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "SY::scombX";}

private:
    //! The function passed to the process constructor
    functype _func;

    template <std::size_t... Is>
    auto in_ports(std::index_sequence<Is...>) {return std::tie(iport[Is]...);}
    auto in_ports()  {return in_ports(std::make_index_sequence<N>{});}
    auto out_ports() {return std::tie(oport1);}

    void exec() {_func(std::get<0>(this->ovals), this->ivals);}
};

//! Process constructor for a strict combinational process with N inputs and one output
/*! similar to scomb with N inputs
 */
template <typename T0, typename... Ts>
class scombN : public detail::comb_core<scombN<T0,Ts...>,
                                        std::tuple<T0>,
                                        std::tuple<Ts...>,
                                        detail::token_policy::strict>
{
    typedef detail::comb_core<scombN<T0,Ts...>,
                              std::tuple<T0>,
                              std::tuple<Ts...>,
                              detail::token_policy::strict> base;
    friend base;
public:
    std::tuple <SY_in<Ts>...> iport;///< tuple of ports for the input channels
    SY_out<T0> oport1;        ///< port for the output channel

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(T0&, const std::tuple<Ts...>&)> functype;

    //! The constructor requires the module name
    scombN(const sc_module_name& _name,      ///< process name
           const functype& _func             ///< function to be passed
          ) : base(_name), oport1("oport1"), _func(_func) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "SY::scombN";}

private:
    //! The function passed to the process constructor
    functype _func;

    auto in_ports()  {return std::apply([](auto&... p){return std::tie(p...);}, iport);}
    auto out_ports() {return std::tie(oport1);}

    void exec() {_func(std::get<0>(this->ovals), this->ivals);}
};

//! Process constructor for a strict combinational process with M inputs and N outputs
/*! similar to scomb with M inputs and an unzip with N outputs
 */
template<typename TO_tuple, typename TI_tuple> class scombMN;

template <typename... TOs, typename... TIs>
class scombMN<std::tuple<TOs...>,std::tuple<TIs...>>
    : public detail::comb_core<scombMN<std::tuple<TOs...>,std::tuple<TIs...>>,
                               std::tuple<TOs...>,
                               std::tuple<TIs...>,
                               detail::token_policy::strict>
{
    typedef detail::comb_core<scombMN<std::tuple<TOs...>,std::tuple<TIs...>>,
                              std::tuple<TOs...>,
                              std::tuple<TIs...>,
                              detail::token_policy::strict> base;
    friend base;
public:
    std::tuple<SY_in<TIs>...>  iport;///< tuple of ports for the input channels
    std::tuple<SY_out<TOs>...> oport;///< tuple of ports for the output channels

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(std::tuple<TOs...>&, const std::tuple<TIs...>&)> functype;

    //! The constructor requires the module name
    scombMN(const sc_module_name& _name,      ///< process name
           const functype& _func             ///< function to be passed
          ) : base(_name), _func(_func) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "SY::scombMN";}

private:
    //! The function passed to the process constructor
    functype _func;

    auto in_ports()  {return std::apply([](auto&... p){return std::tie(p...);}, iport);}
    auto out_ports() {return std::apply([](auto&... p){return std::tie(p...);}, oport);}

    void exec() {_func(this->ovals, this->ivals);}
};

//! The strict zip process with two inputs and one output
/*! This process "zips" two incoming signals into one signal of tuples.
 */
template <class T1, class T2>
class szip : public detail::zip_core<szip<T1,T2>, std::tuple<T1,T2>, false,
                                     detail::token_policy::strict>
{
    typedef detail::zip_core<szip<T1,T2>, std::tuple<T1,T2>, false,
                             detail::token_policy::strict> base;
    friend base;
public:
    SY_in<T1> iport1;        ///< port for the input channel 1
    SY_in<T2> iport2;        ///< port for the input channel 2

    //! The constructor requires the module name
    szip(const sc_module_name& _name      ///< process name
        ) : base(_name), iport1("iport1"), iport2("iport2") {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::szip";}

private:
    auto in_ports() {return std::tie(iport1,iport2);}
};

//! The strict zipX process with an array of inputs and one output
/*! This process "zips" an array of incoming signals into one signal of arrays.
 */
template <class T1, std::size_t N>
class szipX : public detail::zip_core<szipX<T1,N>, std::array<T1,N>, false,
                                      detail::token_policy::strict>
{
    typedef detail::zip_core<szipX<T1,N>, std::array<T1,N>, false,
                             detail::token_policy::strict> base;
    friend base;
public:
    std::array<SY_in<T1>,N> iport;      ///< port array for the input channels

    //! The constructor requires the module name
    szipX(const sc_module_name& _name      ///< process name
        ) : base(_name) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::szipX";}

private:
    template <std::size_t... Is>
    auto in_ports(std::index_sequence<Is...>) {return std::tie(iport[Is]...);}
    auto in_ports() {return in_ports(std::make_index_sequence<N>{});}
};

//! The strict zip process with variable number of inputs and one output
/*! This process "zips" the incoming signals into one signal of tuples.
 */
template <class... Ts>
class szipN : public detail::zip_core<szipN<Ts...>, std::tuple<Ts...>, false,
                                      detail::token_policy::strict>
{
    typedef detail::zip_core<szipN<Ts...>, std::tuple<Ts...>, false,
                             detail::token_policy::strict> base;
    friend base;
public:
    std::tuple<SY_in<Ts>...> iport;///< tuple of ports for the input channels

    //! The constructor requires the module name
    szipN(const sc_module_name& _name      ///< process name
         ) : base(_name) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::szipN";}

private:
    auto in_ports() {return std::apply([](auto&... p){return std::tie(p...);}, iport);}
};

//! The strict unzip process with one input and two outputs
/*! This process "unzips" a signal of tuples into two separate signals
 */
template <class T1, class T2>
class sunzip : public detail::unzip_core<sunzip<T1,T2>, std::tuple<T1,T2>,
                                         detail::token_policy::strict>
{
    typedef detail::unzip_core<sunzip<T1,T2>, std::tuple<T1,T2>,
                               detail::token_policy::strict> base;
    friend base;
public:
    SY_out<T1> oport1;        ///< port for the output channel 1
    SY_out<T2> oport2;        ///< port for the output channel 2

    //! The constructor requires the module name
    sunzip(const sc_module_name& _name      ///< process name
          ) : base(_name), oport1("oport1"), oport2("oport2") {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::sunzip";}

private:
    auto out_ports() {return std::tie(oport1,oport2);}
};

//! The strict unzipX process with one input and an array of outputs
/*! This process "unzips" a signal of arrays into an array of separate signals
 */
template <class T1, std::size_t N>
class sunzipX : public detail::unzip_core<sunzipX<T1,N>, std::array<T1,N>,
                                          detail::token_policy::strict>
{
    typedef detail::unzip_core<sunzipX<T1,N>, std::array<T1,N>,
                               detail::token_policy::strict> base;
    friend base;
public:
    std::array<SY_out<T1>,N> oport;///< port array for the output channels

    //! The constructor requires the module name
    sunzipX(const sc_module_name& _name      ///< process name
          ) : base(_name) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::sunzipX";}

private:
    template <std::size_t... Is>
    auto out_ports(std::index_sequence<Is...>) {return std::tie(oport[Is]...);}
    auto out_ports() {return out_ports(std::make_index_sequence<N>{});}
};

//! The strict unzip process with one input and variable number of outputs
/*! This process "unzips" the incoming signal into a tuple of signals.
 */
template <class... Ts>
class sunzipN : public detail::unzip_core<sunzipN<Ts...>, std::tuple<Ts...>,
                                          detail::token_policy::strict>
{
    typedef detail::unzip_core<sunzipN<Ts...>, std::tuple<Ts...>,
                               detail::token_policy::strict> base;
    friend base;
public:
    std::tuple<SY_out<Ts>...> oport;///< tuple of ports for the output channels

    //! The constructor requires the module name
    sunzipN(const sc_module_name& _name      ///< process name
           ) : base(_name) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::sunzipN";}

private:
    auto out_ports() {return std::apply([](auto&... p){return std::tie(p...);}, oport);}
};

//! The group process with one input and one absent-extended output
/*! It groups values into a vector of specified size n, which takes n
 * cycles. While the grouping takes place the output from this process
 * consists of absent values.
 */
template <class T>
class group : public sy_process
{
public:
    SY_in<T> iport1;                           ///< port for the input channel
    SY_out<std::vector<abst_ext<T>>> oport1;    ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * groups together n samples and writes the results using the output
     * port.
     */
    group(const sc_module_name& _name,      ///< process name
           const unsigned long& samples       ///< Number of samples in each group
          )
         :sy_process(_name), samples(samples)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << samples;
        arg_vec.push_back(std::make_tuple("samples", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::group";}
    
private:
    // Number of samples in each group
    unsigned long samples;
    
    unsigned long samples_took;
    
    // The output vector
    std::vector<abst_ext<T>>* oval;
    
    //Implementing the abstract semantics
    void init()
    {
        oval = new std::vector<abst_ext<T>>;
        oval->resize(samples);
        samples_took = 0;
    }
    
    void prep()
    {
        (*oval)[samples_took] = iport1.read();
        samples_took++;
    }
    
    void exec() {}
    
    void prod()
    {
        if (samples_took==samples)
        {
            write_multiport(oport1, abst_ext<std::vector<abst_ext<T>>>(*oval));
            samples_took = 0;
        }
        else
            write_multiport(oport1, abst_ext<std::vector<abst_ext<T>>>());
    }
    
    void clean()
    {
        delete oval;
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
class fanout : public sy_process
{
public:
    SY_in<T> iport1;        ///< port for the input channel
    SY_out<T> oport1;       ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies and writes the results using the output port
     */
    fanout(const sc_module_name& _name      ///< process name
           ) 
         : sy_process(_name) { }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SY::fanout";}
    
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
