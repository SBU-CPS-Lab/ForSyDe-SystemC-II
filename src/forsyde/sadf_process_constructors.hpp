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

}

using namespace sc_core;

//! Process constructor for a kernel process (actor) with one input and one output
/*! This class is used to build kernel processes with one input
 * and one output. The class is parameterized for input and output
 * data-types.
 */
template <typename T0, typename TC, typename T1>
class kernel : public SADF_process
{
public:
    SADF_in<TC>  cport1;       ///< port for the control channel
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
         ) : SADF_process(_name), cport1("cport1"), iport1("iport1"), oport1("oport1"),
            _func(_func), scenario_table(scenario_table)
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
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SADF::kernel";}

private:    
    // Control, input, and output variables
    std::vector<T0> o1vals;
    std::vector<T1> i1vals;
    TC* cval1;
    
    //! The function passed to the process constructor
    functype _func;

    //! The table of kernel's scenarios to be passed to the process constructor
    scenario_table_type scenario_table;
    
    //Implementing the abstract semantics
    void init()
    {
        cval1 = new TC;
    }
    
    void prep()
    {
        // Read the control port which is connected to the detector to determine the scenario for the kernel
        *cval1 = cport1.read();
        
        // Set the consumption and production rates from the kernel's scenario table
        // (consumption rate, production rate)
        auto cons_rate = std::get<0>(scenario_table[*cval1]);
        auto prod_rate = std::get<1>(scenario_table[*cval1]);

        // Resizing the input and output vectors according to the consumption and production rates
        i1vals.resize(cons_rate);
        o1vals.resize(prod_rate);

        // Reading the input port
        for (auto it=i1vals.begin();it!=i1vals.end();it++)
            *it = iport1.read();
    }
    
    void exec()
    {
        // Call the user-imlpemented kernel function with input and output vectors and the control value
        _func(o1vals, *cval1, i1vals);
    }
    
    void prod()
    {
        write_vec_multiport(oport1, o1vals);
        o1vals.clear();
        i1vals.clear();
    }
    
    void clean()
    {
        delete cval1;
    }
    
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        boundInChans.resize(2);     // only one input port
        boundInChans[0].port = &cport1;
        boundInChans[1].port = &iport1;
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
    }
#endif
};

//! Process constructor for a kernel process (actor) with two inputs and one output
/*! This class is used to build kernel processes with two inputs
 * and one output. The class is parameterized for input and output
 * data-types.
 */
template <typename T0, typename TC, typename T1, typename T2>
class kernel2 : public SADF_process
{
public:
    SADF_in<TC>  cport1;       ///< port for the control channel
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
         ) : SADF_process(_name), cport1("cport1"), iport1("iport1"), iport2("iport2"),
            oport1("oport1"), _func(_func), scenario_table(scenario_table)
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
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SADF::kernel2";}

private:    
    // Control, input, and output variables
    std::vector<T0> o1vals;
    std::vector<T1> i1vals;
    std::vector<T2> i2vals;
    TC* cval1;
    
    //! The function passed to the process constructor
    functype _func;

    //! The table of kernel's scenarios to be passed to the process constructor
    scenario_table_type scenario_table;
    
    //Implementing the abstract semantics
    void init()
    {
        cval1 = new TC;
    }
    
    void prep()
    {
        // Read the control port which is connected to the detector to determine the scenario for the kernel
        *cval1 = cport1.read();
        
        // Set the consumption and production rates from the kernel's scenario table
        // (consumption rate, production rate)
        auto cons_rate1 = std::get<0>(scenario_table[*cval1])[0];
        auto cons_rate2 = std::get<0>(scenario_table[*cval1])[1];
        auto prod_rate = std::get<1>(scenario_table[*cval1]);

        // Resizing the input and output vectors according to the consumption and production rates
        i1vals.resize(cons_rate1);
        i2vals.resize(cons_rate2);
        o1vals.resize(prod_rate);

        // Reading the input ports
        for (auto it=i1vals.begin();it!=i1vals.end();it++)
            *it = iport1.read();
        for (auto it=i2vals.begin();it!=i2vals.end();it++)
            *it = iport2.read();
    }
    
    void exec()
    {
        // Call the user-imlpemented kernel function with input and output vectors and the control value
        _func(o1vals, *cval1, i1vals, i2vals);
    }
    
    void prod()
    {
        write_vec_multiport(oport1, o1vals);
        o1vals.clear();
        i1vals.clear();
        i2vals.clear();
    }
    
    void clean()
    {
        delete cval1;
    }
    
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        boundInChans.resize(3);     // only one input port
        boundInChans[0].port = &cport1;
        boundInChans[1].port = &iport1;
        boundInChans[2].port = &iport2;
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
    }
#endif
};

//! Process constructor for a kernel process with M inputs and N outputs
/*! similar to kernel with M inputs and unzipN
 */
template<typename TO_tuple, typename TC, typename TI_tuple> class kernelMN;

template <typename... TOs, typename TC, typename... TIs>
class kernelMN<std::tuple<TOs...>,TC,std::tuple<TIs...>> : public SADF_process
{
public:
    SADF_in<TC>                 cport1;///< port for the control channel
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
          ) : SADF_process(_name), _func(_func), scenario_table(scenario_table),
              report_pipe(nullptr)
    {
        register_introspection_args(scenario_table);
    }

    //! As above, additionally reporting each firing to a self-report pipe.
    /*! Requires FORSYDE_SELF_REPORTING; without it this is a compile-time
     * error rather than an argument that quietly does nothing.
     */
    kernelMN(sc_module_name _name,      ///< process name
          const functype& _func,        ///< function to be passed
          const scenario_table_type& scenario_table,///< the kernel scenario table
          FILE** _report_pipe           ///< the report named pipe
          ) : SADF_process(_name), _func(_func), scenario_table(scenario_table),
              report_pipe(_report_pipe)
    {
        FORSYDE_REQUIRE_SELF_REPORTING(TC, "SADF::kernelMN");
        register_introspection_args(scenario_table);
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SADF::kernelMN";}
private:
    //! Shared by both constructors above, which cannot delegate to one
    //! another: the reporting one carries a static_assert that the
    //! non-reporting one must not trip.
    void register_introspection_args(const scenario_table_type& scenario_table)
    {
#ifdef FORSYDE_INTROSPECTION
        std::string func_name = std::string(basename());
        func_name = func_name.substr(0, func_name.find_last_not_of("0123456789")+1);
        arg_vec.push_back(std::make_tuple("_func",func_name+std::string("_func")));
        std::stringstream ss;
        ss << scenario_table;
        arg_vec.push_back(std::make_tuple("scenario_table",ss.str()));
#else
        (void)scenario_table;
#endif
    }

    // Control, input and output variables
    std::tuple<std::vector<TOs>...> ovals;
    std::tuple<std::vector<TIs>...> ivals;
    TC* cval1;
    
    //! The function passed to the process constructor
    functype _func;

    //! The table of kernel's scenarios to be passed to the process constructor
    scenario_table_type scenario_table;

    //! Self-report string, built only when report_pipe is non-null
    std::ostringstream report_str;

    //! Optional self-report pipe; null unless the constructor was given one
    FILE** report_pipe;

    //Implementing the abstract semantics
    void init()
    {
        cval1 = new TC;
    }
    
    void prep()
    {
        // Read the control port which is connected to the detector to determine the scenario for the kernel
        *cval1 = cport1.read();

        // Resize the input and output vectors according to 
        // the consumption and production rates from the kernel's scenario table
        // (consumption rate, production rate)
        std::apply([&](auto&... oval) {
            std::apply([&](auto&... otok) {
                (oval.resize(otok), ...);
            }, std::get<1>(scenario_table[*cval1]));
        }, ovals);

        std::apply([&](auto&... ival) {
            std::apply([&](auto&... itok) {
                (ival.resize(itok), ...);
            }, std::get<0>(scenario_table[*cval1]));
        }, ivals);

        // Reading the input ports        
        std::apply([&](auto&... inport) {
            std::apply([&](auto&... ival) {
                (
                    [&ival,&inport](){
                        for (auto it=ival.begin();it!=ival.end();it++)
                            *it = inport.read();
                    }()
                , ...);
            }, ivals);
        }, iport);
    }
    
    void exec()
    {
        // Call the user-imlpemented kernel function with input and output vectors and the control value
        _func(ovals, *cval1, ivals);
#ifdef FORSYDE_SELF_REPORTING
        if (report_pipe)
        {
            // Write the report to the pipe
            report_str << "kernelMN" << "  " << basename()
                                    << "  " << *cval1
                                    << "  " << std::get<0>(scenario_table[*cval1])
                                    << "  " << std::get<1>(scenario_table[*cval1]) << std::endl;
            fputs(report_str.str().c_str(), *report_pipe);
            fflush(*report_pipe);
            report_str.str("");
        }
#endif
    }
    
    void prod()
    {
        std::apply([&](auto&&... port){
            std::apply([&](auto&&... val){
                (write_vec_multiport(port, val), ...);
            }, ovals);
        }, oport);
    }
    
    void clean()
    {
        delete cval1;
    }
    
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        boundInChans.resize(1+sizeof...(TIs));     // control and input ports
        boundInChans[0].port = &cport1;
        std::apply
        (
            [&](auto&... ports)
            {
                std::size_t n{1};
                ((boundInChans[n++].port = &ports),...);
            }, iport
        );
        boundOutChans.resize(sizeof...(TOs));    // output ports
        std::apply
        (
            [&](auto&... ports)
            {
                std::size_t n{0};
                ((boundOutChans[n++].port = &ports),...);
            }, oport
        );
    }
#endif
};

//! Process constructor for a detector process (actor) with one data input and one control output
/*! This class is used to build  a simple detector. Given an initial detector scenario, a detector scenario table,
 * a current scenario detection function, and a kernel scenario selection function, it creates a detector process.
 */
template <typename T0, typename T1, typename TS>
class detector : public SADF_process
{
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
          ) : SADF_process(_name), iport1("iport1"), oport1("oport1"), i1toks(i1toks),
               init_sc(init_sc), _cds_func(_cds_func), _kss_func(_kss_func), scenario_table(scenario_table)
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
        arg_vec.push_back(std::make_tuple("i1toks",std::to_string(i1toks)));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SADF::detector";}
private:
    // consumption and production rates
    size_t i1toks;
    size_t o1toks;
    
    // Input, output, current scenario, and previous scenario variables
    std::vector<T0> o1vals;
    std::vector<T1> i1vals;
    TS* sc_val;
    TS init_sc;
    
    //! The function passed to the process constructor
    cds_functype _cds_func;
    kss_functype _kss_func;

    //! The table of kernel's scenarios to be passed to the process constructor
    scenario_table_type scenario_table;

    //Implementing the abstract semantics
    void init()
    {
        i1vals.resize(i1toks);

        sc_val = new TS;
        *sc_val = init_sc;
    }
    
    void prep()
    {
        // Reading the input port according to the input tokens consumption rate which is passed to the constructor
        for (auto it=i1vals.begin();it!=i1vals.end();it++)
            *it = iport1.read();
    }
    
    void exec()
    {
        // Applying the current detector scenario function to the previous scenario and input tokens to get the operating scenario
        _cds_func(*sc_val, *sc_val, i1vals);
        
        // Look up the scenario table to get the output tokens production rate
        o1toks = scenario_table[*sc_val];

        // Resize the output buffers
        o1vals.resize(o1toks);

        /*  Applying the kernel scenario selection function to the current scenario and the input tokens
        *   to determine scenario for each output port (control token for sending to the kernel)
        */
        _kss_func(o1vals, *sc_val, i1vals);
    }
    
    void prod()
    {
        write_vec_multiport(oport1, o1vals);
    }
    
    void clean()
    {
        delete sc_val;
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

//! Process constructor for a detector process (actor) with M data inputs and N control outputs
/*! similar to detector with M inputs and unzipN
 */
template<typename TO_tuple, typename TI_tuple, typename TS> class detectorMN;

template <typename... TOs, typename... TIs, typename TS>
class detectorMN<std::tuple<TOs...>,std::tuple<TIs...>,TS> : public SADF_process
{
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
          ) : SADF_process(_name), itoks(itoks), init_sc(init_sc),
          _cds_func(_cds_func), _kss_func(_kss_func), scenario_table(scenario_table),
          report_pipe(nullptr)
    {
        register_introspection_args(scenario_table, init_sc, itoks);
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
          ) : SADF_process(_name), itoks(itoks), init_sc(init_sc),
          _cds_func(_cds_func), _kss_func(_kss_func), scenario_table(scenario_table),
          report_pipe(_report_pipe)
    {
        FORSYDE_REQUIRE_SELF_REPORTING(TS, "SADF::detectorMN");
        register_introspection_args(scenario_table, init_sc, itoks);
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "SADF::detectorMN";}
private:
    //! Shared by both constructors above, which cannot delegate to one
    //! another: the reporting one carries a static_assert that the
    //! non-reporting one must not trip.
    void register_introspection_args(const scenario_table_type& scenario_table,
                                     const TS& init_sc,
                                     const std::array<size_t,sizeof...(TIs)>& itoks)
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
        ss.clear();
        ss.str(std::string());
        ss << itoks;
        arg_vec.push_back(std::make_tuple("itoks",ss.str()));
#else
        (void)scenario_table; (void)init_sc; (void)itoks;
#endif
    }

    // consumption and production rates
    std::array<size_t,sizeof...(TIs)> itoks;
    std::array<size_t,sizeof...(TOs)> otoks;

    // Input, output, current scenario, and previous scenario variables
    std::tuple<std::vector<TOs>...> ovals;
    std::tuple<std::vector<TIs>...> ivals;
    TS* sc_val;
    TS init_sc;
    
    //! The function passed to the process constructor
    cds_functype _cds_func;
    kss_functype _kss_func;

    //! The table of kernel's scenarios to be passed to the process constructor
    scenario_table_type scenario_table;

    //! Self-report string, built only when report_pipe is non-null
    std::ostringstream report_str;

    //! Optional self-report pipe; null unless the constructor was given one
    FILE** report_pipe;

    //Implementing the abstract semantics
    void init()
    {
        std::apply([&](auto&... ival) {
            std::apply([&](auto&... itok) {
                (ival.resize(itok), ...);
            }, itoks);
        }, ivals);

        sc_val = new TS;
        *sc_val = init_sc;
    }
    
    void prep()
    {
        // Reading the input ports        
        std::apply([&](auto&... inport) {
            std::apply([&](auto&... ival) {
                (
                    [&ival,&inport](){
                        for (auto it=ival.begin();it!=ival.end();it++)
                            *it = inport.read();
                    }()
                , ...);
            }, ivals);
        }, iport);
    }
    
    void exec()
    {
        // Applying the current detector scenario function to the previous scenario and input tokens to get the operating scenario
        _cds_func(*sc_val, *sc_val, ivals);

        // Look up the scenario table to get the output tokens production rate
        otoks = scenario_table[*sc_val];

        // Resize the output buffers
        std::apply([&](auto&... oval) {
            std::apply([&](auto&... otok) {
                (oval.resize(otok), ...);
            }, otoks);
        }, ovals);

        /*  Applying the kernel scenario selection function to the current scenario and the input tokens
        *   to determine scenario for each output port (control token for sending to the kernel)
        */
        _kss_func(ovals, *sc_val, ivals);
    }
    
    void prod()
    {
        std::apply([&](auto&&... port){
            std::apply([&](auto&&... val){
                (write_vec_multiport(port, val), ...);
            }, ovals);
        }, oport);
#ifdef FORSYDE_SELF_REPORTING
        if (report_pipe)
        {
            // Write the report to the pipe
            report_str << "detectorMN" << "  " << basename() << "  " << *sc_val << "  " << scenario_table[*sc_val] << std::endl;
            fputs(report_str.str().c_str(), *report_pipe);
            fflush(*report_pipe);
            report_str.str("");
        }
#endif
    }
    
    void clean()
    {
        delete sc_val;
    }
    
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        boundInChans.resize(sizeof...(TIs));     // input ports
        std::apply
        (
            [&](auto&... ports)
            {
                std::size_t n{0};
                ((boundInChans[n++].port = &ports),...);
            }, iport
        );
        boundOutChans.resize(sizeof...(TOs));    // output ports
        std::apply
        (
            [&](auto&... ports)
            {
                std::size_t n{0};
                ((boundOutChans[n++].port = &ports),...);
            }, oport
        );
    }
#endif
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
