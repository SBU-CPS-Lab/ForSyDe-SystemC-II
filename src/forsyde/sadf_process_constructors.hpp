/**********************************************************************           
    * sadf_process_constructors.hpp -- Process constructors in the    *
    *                                  SADF MOC                       *
    *                                                                 *
    * Author:  Mohammad Vazirpanah (mohammad.vazirpanah@yahoo.com)    *
    *                                                                 *
    * Purpose: Providing basic process constructors for modeling      *
    *          SADF systems in ForSyDe-SystemC                        *
    *                                                                 *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef SADF_PROCESS_CONSTRUCTORS_HPP
#define SADF_PROCESS_CONSTRUCTORS_HPP

/*! \file SADF_process_constructors.hpp
 * \brief Implements the basic process constructors in the SADF MoC
 * 
 *  This file includes the basic process constructors used for modeling
 * in the SADF model of computation.
 */

#include <systemc>
#include <functional>
#include <tuple>
#include <vector>
#include <map>
#include <cstdio>
#include <sstream>

#include "abst_ext.hpp"     // detail::stream_or_placeholder, used by scenario_entry below
#include "sadf_process.hpp"
// A handful of SADF process constructors below (combMN, source, sink,
// delayn) are plain re-exports of their SDF counterparts, so this file
// needs SDF's definitions regardless of whether the including
// translation unit reached sdf_process_constructors.hpp by some other
// path already.
#include "sdf_process_constructors.hpp"

namespace ForSyDe
{

namespace SADF
{

namespace detail
{

//! Whether this build compiles the SADF self-reporting code (D10).
/*! Dependent on a template parameter at every use site, so the
 * static_assert it backs fires only when a self-reporting overload is
 * actually instantiated -- not merely because this header was included.
 */
template <typename...>
inline constexpr bool self_reporting_enabled =
#ifdef FORSYDE_SELF_REPORTING
    true;
#else
    false;
#endif

//! The diagnostic that a self-reporting overload emits without the macro.
#define FORSYDE_REQUIRE_SELF_REPORTING(DEPENDENT_TYPE, WHAT)                  \
    static_assert(::ForSyDe::SADF::detail::self_reporting_enabled<DEPENDENT_TYPE>, \
        WHAT " was given a self-report pipe, but FORSYDE_SELF_REPORTING is "  \
        "not defined, so no report would ever be written. Add "               \
        "-DFORSYDE_SELF_REPORTING to this model's CFLAGS, or drop the pipe "  \
        "argument to use the non-reporting overload.")

//! Looks a scenario up in a scenario table, erroring if it is not there (D8).
/*! Every one of these lookups used to be a plain scenario_table[scen] on
 * a non-const std::map. That is not a lookup: std::map::operator[]
 * *inserts* a default-constructed entry when the key is absent. So a
 * detector emitting a scenario the kernel's table doesn't define -- a
 * modelling mistake, and an easy one to make, since the two tables are
 * written separately and nothing cross-checks them -- did not fail. It
 * silently gained a zero-rate entry, meaning that from then on the
 * process consumed no tokens and produced none, forever: the model
 * quietly stops making progress with no diagnostic. It also mutates a
 * table that is conceptually read-only during simulation, growing it
 * once per distinct bad scenario.
 *
 * This does a find() and raises a real error instead. The offending
 * scenario value is included in the message when its type can be
 * streamed, since knowing *which* scenario arrived is usually the whole
 * diagnosis.
 */
template <typename Table, typename Key>
inline const typename Table::mapped_type& scenario_entry(
    const Table& table,         ///< the scenario table to look in
    const Key& scen,            ///< the scenario received
    const char* process_name,   ///< reporting process, for the message
    const char* table_name      ///< which table, for the message
)
{
    auto it = table.find(scen);
    if (it != table.end())
        return it->second;

    std::ostringstream msg;
    msg << "received scenario ";
    ForSyDe::detail::stream_or_placeholder(msg, scen);
    msg << " which is not defined in its " << table_name << " ("
        << table.size() << " scenario(s) defined). The process driving "
           "this one is emitting a scenario this one does not know how "
           "to fire in; the two scenario tables disagree.";
    SC_REPORT_ERROR(process_name, msg.str().c_str());

    // Only reached if the report handler is configured not to throw on
    // SC_ERROR. Returning a shared zero-rate entry keeps that path
    // defined rather than dereferencing end(); it reproduces the old
    // silent behaviour, but only after the error above has been raised.
    static const typename Table::mapped_type absent{};
    return absent;
}

}

using namespace sc_core;

namespace detail
{

//! Shared implementation of the SADF detector family
/*! A detector is the other half of SADF's adaptivity: where a kernel is
 * told which scenario to fire in, a detector is what decides. Both do
 * the same thing per firing -- read a fixed number of tokens, consult a
 * scenario table, size the outputs from it and apply the user functions
 * -- and they differ only in how many ports they have and in what shape
 * the table's entries are. So this is kernel_core's counterpart, and the
 * SDF read/write/resize/bind helpers do the same work for both.
 *
 * The state here is the *current detector scenario*, which is also what
 * the process emits: cds_func advances it from the tokens just read, its
 * scenario-table entry gives this firing's production rates, and
 * kss_func fills the outputs with the control tokens the kernels
 * downstream will fire in.
 *
 * \a Table is the scenario table type and the scenario type \a TS is
 * read off it as Table::key_type. \a Derived supplies in_ports(),
 * out_ports(), resize_inputs() -- the consumption rates are fixed at
 * construction, so that runs once, in init() -- and exec().
 */
template <typename Derived, typename OVals, typename IVals, typename Table>
class detector_core : public SADF_process
{
public:
    typedef typename Table::key_type TS;    ///< the detector scenario type

protected:
    OVals ovals;    ///< output tokens, one vector per output port
    IVals ivals;    ///< input tokens, one vector per input port

    TS sc_val;      ///< the current detector scenario
    TS init_sc;     ///< the initial detector scenario

    //! The table of the detector's scenarios, passed to the process constructor
    Table scenario_table;

    detector_core(const sc_module_name& _name,  ///< process name
                  const Table& scenario_table,  ///< the detector scenario table
                  const TS& init_sc             ///< the initial scenario
                  ) : SADF_process(_name), sc_val(), init_sc(init_sc),
                      scenario_table(scenario_table)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("cds_func",func_name+std::string("cds_func")));
        arg_vec.push_back(std::make_tuple("kss_func",func_name+std::string("kss_func")));
        std::stringstream ss;
        ss << scenario_table;
        arg_vec.push_back(std::make_tuple("scenario_table",ss.str()));
        ss.clear();
        ss.str(std::string());
        ss << init_sc;
        arg_vec.push_back(std::make_tuple("init_sc",ss.str()));
#endif
    }

    //! This firing's production rates, or a raised error if the scenario is unknown (D8)
    const typename Table::mapped_type& scenario_rates()
    {
        return scenario_entry(scenario_table, sc_val, name(),
                              "detector scenario table");
    }

private:
    Derived& self() {return static_cast<Derived&>(*this);}

    //Implementing the abstract semantics
    void init()
    {
        sc_val = init_sc;
        // Consumption rates are fixed at construction, so the input
        // vectors are sized once here rather than per firing.
        self().resize_inputs();
    }

    void clean() {}

    void prep() {SDF::detail::read_all(self().in_ports(), ivals);}

    void prod() {SDF::detail::write_all(self().out_ports(), ovals);}

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        SDF::detail::bind_all(boundInChans, self().in_ports());
        SDF::detail::bind_all(boundOutChans, self().out_ports());
    }
#endif
};

//! Shared implementation of the SADF kernel family
/*! kernel, kernel2 and kernelMN all do the same five things per firing:
 * read a scenario from the control port, look that scenario up in the
 * kernel's scenario table to get this firing's consumption and
 * production rates, resize a vector per port to match, read that many
 * tokens from each input port, apply the user function, and write the
 * produced tokens out. The control port, the scenario table, the lookup
 * and the loop live here; what varies -- port count, port names, the
 * shape the rates are stored in, and the spelling of the user function
 * -- stays in the derived classes.
 *
 * \a Table is the scenario table type, and the scenario type \a TC is
 * read off it as Table::key_type. \a Derived supplies:
 *   - \c in_ports() and \c out_ports(), as in every other core here;
 *   - \c resize_vectors(), which reads its own rate representation out
 *     of scenario_rates() and calls resize_from() -- the arity variants
 *     store the input rates as a scalar or a std::array and the output
 *     rate as a scalar, while kernelMN stores both as arrays, so this is
 *     the one thing the core cannot do generically;
 *   - \c exec(), the call to the user function.
 *
 * \a ClearsAfterFiring is the one behavioural difference between the
 * existing classes: kernel and kernel2 empty their token vectors after
 * writing, kernelMN does not. Since prep() resizes them again next
 * firing, this is only observable through a user function that leaves
 * some of its output vector unwritten -- which would see zeroes under
 * kernel and last firing's values under kernelMN. That is an
 * inconsistency rather than a designed distinction, but it is a
 * behavioural one, so it is preserved verbatim and marked here rather
 * than quietly unified.
 */
template <typename Derived, typename OVals, typename IVals, typename Table,
          bool ClearsAfterFiring>
class kernel_core : public SADF_process
{
public:
    typedef typename Table::key_type TC;    ///< the scenario type

    SADF_in<TC> cport1;     ///< port for the control channel

protected:
    OVals ovals;    ///< output tokens, one vector per output port
    IVals ivals;    ///< input tokens, one vector per input port
    TC cval1;       ///< the scenario read from cport1 this firing

    //! The table of the kernel's scenarios, passed to the process constructor
    Table scenario_table;

    kernel_core(const sc_module_name& _name,    ///< process name
                const Table& scenario_table     ///< the kernel scenario table
                ) : SADF_process(_name), cport1("cport1"), cval1(),
                    scenario_table(scenario_table)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_func",func_name+std::string("_func")));
        std::stringstream ss;
        ss << scenario_table;
        arg_vec.push_back(std::make_tuple("scenario_table",ss.str()));
#endif
    }

    //! This firing's rates, or a raised error if the scenario is unknown (D8)
    const typename Table::mapped_type& scenario_rates()
    {
        return scenario_entry(scenario_table, cval1, name(),
                                      "kernel scenario table");
    }

    //! Size every token vector to this firing's rates
    template <typename InRates, typename OutRates>
    void resize_from(const InRates& in_rates, const OutRates& out_rates)
    {
        SDF::detail::resize_all(ivals, in_rates);
        SDF::detail::resize_all(ovals, out_rates);
    }

private:
    Derived& self() {return static_cast<Derived&>(*this);}

    //Implementing the abstract semantics

    // cval1 used to be a heap-allocated TC* in all three classes,
    // new'd here and deleted in clean(); it is a plain member now.
    void init() {}

    void clean() {}

    void prep()
    {
        // Read the control port, which is connected to the detector that
        // determines this firing's scenario for the kernel
        cval1 = cport1.read();
        self().resize_vectors();
        SDF::detail::read_all(self().in_ports(), ivals);
    }

    void prod()
    {
        SDF::detail::write_all(self().out_ports(), ovals);
        if constexpr (ClearsAfterFiring)
        {
            std::apply([](auto&... val){(val.clear(), ...);}, ovals);
            std::apply([](auto&... val){(val.clear(), ...);}, ivals);
        }
    }

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        // The control port is the kernel's first input, ahead of the
        // data inputs -- which is the order the XML lists them in.
        SDF::detail::bind_all(boundInChans,
                              std::tuple_cat(std::tie(cport1), self().in_ports()));
        SDF::detail::bind_all(boundOutChans, self().out_ports());
    }
#endif
};

}

//! Process constructor for a kernel process (actor) with one input and one output
/*! This class is used to build kernel processes with one input
 * and one output. The class is parameterized for input and output
 * data-types.
 */
template <typename T0, typename TC, typename T1>
class kernel : public detail::kernel_core<kernel<T0,TC,T1>,
                                          std::tuple<std::vector<T0>>,
                                          std::tuple<std::vector<T1>>,
                                          std::map<TC,std::tuple<size_t,size_t>>,
                                          true>
{
    typedef detail::kernel_core<kernel<T0,TC,T1>,
                                std::tuple<std::vector<T0>>,
                                std::tuple<std::vector<T1>>,
                                std::map<TC,std::tuple<size_t,size_t>>,
                                true> base;
    friend base;
public:
    SADF_in<T1>  iport1;       ///< port for the input channel
    SADF_out<T0> oport1;       ///< port for the output channel

    //! Type of the table of kernel's scennarios to be passed to the process constructor
    /*! The table of kernel's scennarios is a map from the scenario ID to
     * a tuple of the input consumtion and output production rates.
     */
    typedef std::map<TC,std::tuple<size_t,size_t>> scenario_table_type;

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(
                                std::vector<T0>&,
                                const TC&,
                                const std::vector<T1>&
                            )> functype;

    //! The constructor requires the module name, the kernel function, and the scenario table
    /*! It creates an SC_THREAD which according to the current scenario,reads data from its input port,
     * applies the user-imlpemented function to it and writes the results using the output port
     */
    kernel(sc_module_name _name,            ///< process name
         const functype& _func,             ///< function to be passed
         const scenario_table_type& scenario_table///< the kernel scenario table
         ) : base(_name,scenario_table), iport1("iport1"), oport1("oport1"),
            _func(_func) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SADF::kernel";}

private:
    //! The function passed to the process constructor
    functype _func;

    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}

    // (consumption rate, production rate), both scalars for this arity
    void resize_vectors()
    {
        const auto& scen_rates = this->scenario_rates();
        this->resize_from(std::array<size_t,1>{std::get<0>(scen_rates)},
                          std::array<size_t,1>{std::get<1>(scen_rates)});
    }

    void exec()
    {
        // Call the user-imlpemented kernel function with input and output vectors and the control value
        _func(std::get<0>(this->ovals), this->cval1, std::get<0>(this->ivals));
    }
};

//! Process constructor for a kernel process (actor) with two inputs and one output
/*! This class is used to build kernel processes with two inputs
 * and one output. The class is parameterized for input and output
 * data-types.
 */
template <typename T0, typename TC, typename T1, typename T2>
class kernel2 : public detail::kernel_core<kernel2<T0,TC,T1,T2>,
                                           std::tuple<std::vector<T0>>,
                                           std::tuple<std::vector<T1>,std::vector<T2>>,
                                           std::map<TC,std::tuple<std::array<size_t,2>,size_t>>,
                                           true>
{
    typedef detail::kernel_core<kernel2<T0,TC,T1,T2>,
                                std::tuple<std::vector<T0>>,
                                std::tuple<std::vector<T1>,std::vector<T2>>,
                                std::map<TC,std::tuple<std::array<size_t,2>,size_t>>,
                                true> base;
    friend base;
public:
    SADF_in<T1>  iport1;       ///< port for the input channel 1
    SADF_in<T2>  iport2;       ///< port for the input channel 2
    SADF_out<T0> oport1;       ///< port for the output channel

    //! Type of the table of kernel's scennarios to be passed to the process constructor
    /*! The table of kernel's scennarios is a map from the scenario ID to
     * a tuple of the input consumtion and output production rates.
     */
    typedef std::map<TC,std::tuple<std::array<size_t,2>,size_t>> scenario_table_type;

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(
                                std::vector<T0>&,
                                const TC&,
                                const std::vector<T1>&,
                                const std::vector<T2>&
                            )> functype;

    //! The constructor requires the module name, the kernel function, and the scenario table
    /*! It creates an SC_THREAD which according to the current scenario,reads data from its input port,
     * applies the user-imlpemented function to it and writes the results using the output port
     */
    kernel2(sc_module_name _name,           ///< process name
         const functype& _func,             ///< function to be passed
         const scenario_table_type& scenario_table///< the kernel scenario table
         ) : base(_name,scenario_table), iport1("iport1"), iport2("iport2"),
            oport1("oport1"), _func(_func) {}

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SADF::kernel2";}

private:
    //! The function passed to the process constructor
    functype _func;

    auto in_ports()  {return std::tie(iport1,iport2);}
    auto out_ports() {return std::tie(oport1);}

    // (consumption rates as a 2-array, production rate as a scalar)
    void resize_vectors()
    {
        const auto& scen_rates = this->scenario_rates();
        this->resize_from(std::get<0>(scen_rates),
                          std::array<size_t,1>{std::get<1>(scen_rates)});
    }

    void exec()
    {
        // Call the user-imlpemented kernel function with input and output vectors and the control value
        _func(std::get<0>(this->ovals), this->cval1,
              std::get<0>(this->ivals), std::get<1>(this->ivals));
    }
};

//! Process constructor for a kernel process with M inputs and N outputs
/*! similar to kernel with M inputs and unzipN
 */
template<typename TO_tuple, typename TC, typename TI_tuple> class kernelMN;

template <typename... TOs, typename TC, typename... TIs>
class kernelMN<std::tuple<TOs...>,TC,std::tuple<TIs...>>
    : public detail::kernel_core<kernelMN<std::tuple<TOs...>,TC,std::tuple<TIs...>>,
                                 std::tuple<std::vector<TOs>...>,
                                 std::tuple<std::vector<TIs>...>,
                                 std::map<TC,std::tuple<std::array<size_t,sizeof...(TIs)>,
                                                        std::array<size_t,sizeof...(TOs)>>>,
                                 false>
{
    typedef detail::kernel_core<kernelMN<std::tuple<TOs...>,TC,std::tuple<TIs...>>,
                                std::tuple<std::vector<TOs>...>,
                                std::tuple<std::vector<TIs>...>,
                                std::map<TC,std::tuple<std::array<size_t,sizeof...(TIs)>,
                                                       std::array<size_t,sizeof...(TOs)>>>,
                                false> base;
    friend base;
public:
    std::tuple<SADF_in<TIs>...>  iport;///< tuple of ports for the input channels
    std::tuple<SADF_out<TOs>...> oport;///< tuple of ports for the output channels

    //! Type of the table of kernel's scennarios to be passed to the process constructor
    /*! The table of kernel's scennarios is a map from the scenario ID to
     * a tuple of the input consumtion and output production rates, each represented as an array.
     */
    typedef std::map<TC,std::tuple<
                        std::array<size_t,sizeof...(TIs)>,
                        std::array<size_t,sizeof...(TOs)>
                    >> scenario_table_type;

    //! Type of the function to be passed to the process constructor
    typedef std::function<void(
                                std::tuple<std::vector<TOs>...>&,
                                const TC&,
                                const std::tuple<std::vector<TIs>...>&
                            )> functype;

    //! The constructor requires the module name, the kernel function, and the scenario table
    /*! It creates an SC_THREAD which according to the current scenario, reads data from its input ports,
     * applies the user-imlpemented function to them and writes the
     * results using the output ports
     */
    //  D10: _report_pipe used to be appended to this one constructor only
    //  #ifdef FORSYDE_SELF_REPORTING, so kernelMN had two mutually
    //  incompatible signatures depending on a build macro -- a model
    //  written against one could not be rebuilt against the other without
    //  editing its call sites, which is exactly what enabling
    //  self-reporting used to require in practice. There are now two
    //  overloads instead, both always present: passing a pipe is a
    //  deliberate choice at the call site rather than a macro-dependent
    //  change of shape. The macro still decides whether the reporting
    //  code is compiled at all, and passing a pipe without it is a
    //  compile-time error rather than a silently-ignored argument.
    kernelMN(sc_module_name _name,      ///< process name
          const functype& _func,        ///< function to be passed
          const scenario_table_type& scenario_table ///< the kernel scenario table
          ) : base(_name,scenario_table), _func(_func), report_pipe(nullptr) {}

    //! As above, additionally reporting each firing to a self-report pipe.
    /*! Requires FORSYDE_SELF_REPORTING; without it this is a compile-time
     * error rather than an argument that quietly does nothing.
     */
    kernelMN(sc_module_name _name,      ///< process name
          const functype& _func,        ///< function to be passed
          const scenario_table_type& scenario_table,///< the kernel scenario table
          FILE** _report_pipe           ///< the report named pipe
          ) : base(_name,scenario_table), _func(_func), report_pipe(_report_pipe)
    {
        FORSYDE_REQUIRE_SELF_REPORTING(TC, "SADF::kernelMN");
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SADF::kernelMN";}

private:
    //! The function passed to the process constructor
    functype _func;

    //! Self-report string, built only when report_pipe is non-null
    std::ostringstream report_str;

    //! Optional self-report pipe; null unless the constructor was given one
    FILE** report_pipe;

    auto in_ports()  {return std::apply([](auto&... p){return std::tie(p...);}, iport);}
    auto out_ports() {return std::apply([](auto&... p){return std::tie(p...);}, oport);}

    // (consumption rates, production rates), both arrays for this arity
    void resize_vectors()
    {
        const auto& scen_rates = this->scenario_rates();
        this->resize_from(std::get<0>(scen_rates), std::get<1>(scen_rates));
    }

    void exec()
    {
        // Call the user-imlpemented kernel function with input and output vectors and the control value
        _func(this->ovals, this->cval1, this->ivals);
#ifdef FORSYDE_SELF_REPORTING
        if (report_pipe)
        {
            // Write the report to the pipe
            const auto& scen_rates = this->scenario_rates();
            report_str << "kernelMN" << "  " << this->basename()
                                    << "  " << this->cval1
                                    << "  " << std::get<0>(scen_rates)
                                    << "  " << std::get<1>(scen_rates) << std::endl;
            fputs(report_str.str().c_str(), *report_pipe);
            fflush(*report_pipe);
            report_str.str("");
        }
#endif
    }
};

//! Process constructor for a detector process (actor) with one data input and one control output
/*! This class is used to build  a simple detector. Given an initial detector scenario, a detector scenario table,
 * a current scenario detection function, and a kernel scenario selection function, it creates a detector process.
 */
template <typename T0, typename T1, typename TS>
class detector : public detail::detector_core<detector<T0,T1,TS>,
                                        std::tuple<std::vector<T0>>,
                                        std::tuple<std::vector<T1>>,
                                        std::map<TS,size_t>>
{
    typedef detail::detector_core<detector<T0,T1,TS>,
                                        std::tuple<std::vector<T0>>,
                                        std::tuple<std::vector<T1>>,
                                        std::map<TS,size_t>> base;
    friend base;
public:
    SADF_in<T1> iport1;     ///< port for the input channel
    SADF_out<T0> oport1;    ///< port for the output channel
    
    //! Type of the table of kernel's scennarios to be passed to the process constructor
    /*! The table of kernel's scennarios is a map from the scenario ID to
     * the output production rates.
     */
    typedef std::map<TS,size_t> scenario_table_type;

    //! Type of the current detector scenario function to be passed to the process constructor
    typedef std::function<void(TS&,
                                const TS&,
                                const std::vector<T1>&)> cds_functype;
    
    //! Type of the kernel scenario selection function to be passed to the process constructor
    typedef std::function<void(std::vector<T0>&,
                                const TS&,
                                const std::vector<T1>&)> kss_functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input ports,
     * applies the user-imlpemented function to them and writes the
     * results using the output port
     */
    detector(sc_module_name _name,                  ///< process name
          const cds_functype& _cds_func,            ///< current detector scenario function to be passed
          const kss_functype& _kss_func,            ///< kernel scenario function to be passed
          const scenario_table_type& scenario_table,///< the detector scenario table
          const TS& init_sc,                        ///< Initial scenario
          const size_t& i1toks                      ///< consumption rate for the first input
          ) : base(_name, scenario_table, init_sc), iport1("iport1"), oport1("oport1"), i1toks(i1toks), _cds_func(_cds_func), _kss_func(_kss_func)
    {
#ifdef FORSYDE_INTROSPECTION
        this->arg_vec.push_back(std::make_tuple("i1toks",std::to_string(i1toks)));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SADF::detector";}
private:
    // consumption rate, fixed at construction
    size_t i1toks;

    //! The functions passed to the process constructor
    cds_functype _cds_func;
    kss_functype _kss_func;

    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}

    void resize_inputs() {std::get<0>(this->ivals).resize(i1toks);}

    void exec()
    {
        auto& i1vals = std::get<0>(this->ivals);
        // Applying the current detector scenario function to the previous scenario and input tokens to get the operating scenario
        _cds_func(this->sc_val, this->sc_val, i1vals);

        // The scenario table gives this firing's output production rate
        std::get<0>(this->ovals).resize(this->scenario_rates());

        /*  Applying the kernel scenario selection function to the current scenario and the input tokens
        *   to determine scenario for each output port (control token for sending to the kernel)
        */
        _kss_func(std::get<0>(this->ovals), this->sc_val, i1vals);
    }
};

//! Process constructor for a detector process (actor) with M data inputs and N control outputs
/*! similar to detector with M inputs and unzipN
 */
template<typename TO_tuple, typename TI_tuple, typename TS> class detectorMN;

template <typename... TOs, typename... TIs, typename TS>
class detectorMN<std::tuple<TOs...>,std::tuple<TIs...>,TS> : public detail::detector_core<detectorMN<std::tuple<TOs...>,std::tuple<TIs...>,TS>,
                                          std::tuple<std::vector<TOs>...>,
                                          std::tuple<std::vector<TIs>...>,
                                          std::map<TS,std::array<size_t,sizeof...(TOs)>>>
{
    typedef detail::detector_core<detectorMN<std::tuple<TOs...>,std::tuple<TIs...>,TS>,
                                          std::tuple<std::vector<TOs>...>,
                                          std::tuple<std::vector<TIs>...>,
                                          std::map<TS,std::array<size_t,sizeof...(TOs)>>> base;
    friend base;
public:
    std::tuple<SADF_in<TIs>...>  iport;///< tuple of ports for the input channels
    std::tuple<SADF_out<TOs>...> oport;///< tuple of ports for the output channels
    
    //! Type of the table of kernel's scennarios to be passed to the process constructor
    /*! The table of kernel's scennarios is a map from the scenario ID to
     * an array of the output production rates.
     */
    typedef std::map<TS,std::array<size_t,sizeof...(TOs)>> scenario_table_type;

    //! Type of the current detector scenario function to be passed to the process constructor
    typedef std::function<void(TS&,
                                const TS&,
                                const std::tuple<std::vector<TIs>...>&)> cds_functype;

    //! Type of the kernel scenario selection function to be passed to the process constructor
    typedef std::function<void(std::tuple<std::vector<TOs>...>&,
                                const TS&,
                                const std::tuple<std::vector<TIs>...>&)> kss_functype;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which according to the current scenario, reads data from its input ports,
     * applies the user-imlpemented function to them and writes the
     * results using the output ports
     */
    //  D10: see the note on kernelMN's constructors above. Two always-
    //  present overloads rather than one whose shape a build macro
    //  changes; the reporting one requires FORSYDE_SELF_REPORTING.
    detectorMN(sc_module_name _name,                ///< process name
          const cds_functype& _cds_func,            ///< current detector scenario function to be passed
          const kss_functype& _kss_func,            ///< kernel scenario function to be passed
          const scenario_table_type& scenario_table,///< the detector scenario table
          const TS& init_sc,                        ///< Initial scenario
          const std::array<size_t,sizeof...(TIs)>& itoks    ///< consumption rate for the first input
          ) : base(_name, scenario_table, init_sc), itoks(itoks),
          _cds_func(_cds_func), _kss_func(_kss_func),
          report_pipe(nullptr)
    {
        register_rate_args(itoks);
    }

    //! As above, additionally reporting each firing to a self-report pipe.
    /*! Requires FORSYDE_SELF_REPORTING; without it this is a compile-time
     * error rather than an argument that quietly does nothing.
     */
    detectorMN(sc_module_name _name,                ///< process name
          const cds_functype& _cds_func,            ///< current detector scenario function to be passed
          const kss_functype& _kss_func,            ///< kernel scenario function to be passed
          const scenario_table_type& scenario_table,///< the detector scenario table
          const TS& init_sc,                        ///< Initial scenario
          const std::array<size_t,sizeof...(TIs)>& itoks,   ///< consumption rate for the first input
          FILE** _report_pipe                       ///< the report named pipe
          ) : base(_name, scenario_table, init_sc), itoks(itoks),
          _cds_func(_cds_func), _kss_func(_kss_func),
          report_pipe(_report_pipe)
    {
        FORSYDE_REQUIRE_SELF_REPORTING(TS, "SADF::detectorMN");
        register_rate_args(itoks);
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SADF::detectorMN";}
private:
    //! Shared by both constructors above, which cannot delegate to one
    //! another: the reporting one carries a static_assert that the
    //! non-reporting one must not trip. Everything else about this
    //! process's introspection arguments is registered by the core.
    void register_rate_args(const std::array<size_t,sizeof...(TIs)>& itoks)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << itoks;
        this->arg_vec.push_back(std::make_tuple("itoks",ss.str()));
#else
        (void)itoks;
#endif
    }

    // consumption rates, fixed at construction
    std::array<size_t,sizeof...(TIs)> itoks;

    //! The functions passed to the process constructor
    cds_functype _cds_func;
    kss_functype _kss_func;

    //! Self-report string, built only when report_pipe is non-null
    std::ostringstream report_str;

    //! Optional self-report pipe; null unless the constructor was given one
    FILE** report_pipe;

    auto in_ports()  {return std::apply([](auto&... p){return std::tie(p...);}, iport);}
    auto out_ports() {return std::apply([](auto&... p){return std::tie(p...);}, oport);}

    void resize_inputs() {SDF::detail::resize_all(this->ivals, itoks);}

    void exec()
    {
        // Applying the current detector scenario function to the previous scenario and input tokens to get the operating scenario
        _cds_func(this->sc_val, this->sc_val, this->ivals);

        // The scenario table gives this firing's output production rates
        SDF::detail::resize_all(this->ovals, this->scenario_rates());

        /*  Applying the kernel scenario selection function to the current scenario and the input tokens
        *   to determine scenario for each output port (control token for sending to the kernel)
        */
        _kss_func(this->ovals, this->sc_val, this->ivals);
#ifdef FORSYDE_SELF_REPORTING
        if (report_pipe)
        {
            // Write the report to the pipe
            report_str << "detectorMN" << "  " << this->basename()
                                       << "  " << this->sc_val
                                       << "  " << this->scenario_rates() << std::endl;
            fputs(report_str.str().c_str(), *report_pipe);
            fflush(*report_pipe);
            report_str.str("");
        }
#endif
    }
};

//! Process constructor for a combinational process with M inputs and N outputs
/*! similar to comb with M inputs and unzipN, re-exported from the SDF MoC
 */
template <typename TO_tuple, typename TI_tuple>
using combMN = SDF::combMN<TO_tuple,TI_tuple>;

//! Process constructor for a source process
/*! This class is used to build a souce process which only has an output.
 * Given an initial state and a function, the process repeatedly applies
 * the function to the current state to produce next state, which is
 * also the process output. It can be used in test-benches.
 */
template <class T>
using source = SDF::source<T>;

//! Process constructor for a sink process
/*! This class is used to build a sink process which only has an input.
 * Its main purpose is to be used in test-benches. The process repeatedly
 * applies a given function to the current input.
 */
template <class T>
using sink = SDF::sink<T>;

//! Process constructor for a n-delay element
/*! This class is used to build a sequential process similar to dalay
 * but with an extra initial variable which sets the number of delay
 * elements (initial tokens). Given an initial value, it inserts the
 * initial value n times at the the beginning of output stream and
 * passes the rest of the inputs to its output untouched. The class is
 * parameterized for its input/output data-type.
 */
template <class T>
using delayn = SDF::delayn<T>;

}
}

#endif
