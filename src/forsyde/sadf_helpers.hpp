/************************************************************************           
    * sadf_helpers.hpp -- Helper primitives in the SADF MoC             *
    *                                                                   *
    * Author:  Mohammad Vazirpanah (mohammad.vazirpanah@yahoo.com)      *
    *                                                                   *
    * Purpose: Providing helper primitives for modeling in the SADF MoC *
    *                                                                   *
    * Usage:   This file is included automatically                      *
    *                                                                   *
    * License:                                                          *
    *********************************************************************/

#ifndef SADF_HELPERS_HPP
#define SADF_HELPERS_HPP

/*! \file sadf_helpers.hpp
 * \brief Implements helper primitives for modeling in the SADF MoC
 * 
 *  This file includes helper functions which facilliate construction of
 * processes in the SADF MoC
 */

#include <systemc>
#include <functional>
#include <tuple>

#include "sadf_process_constructors.hpp"

namespace ForSyDe
{

namespace SADF
{

using namespace sc_core;

//! Helper function to construct a kernel process
/*! This function is used to construct a kernel (SystemC module) and
 * connect its output and output signals.
 * It provides a more functional style definition of a ForSyDe process.
 * It also removes bilerplate code by using type-inference feature of
 * C++ and automatic binding to the input FIFOs.
 */
template <typename T0, typename TC, typename T1,
           template <class> class CIf,
           template <class> class I1If,
           template <class> class OIf>
inline kernel<T0,TC,T1>* make_kernel(const std::string& pName,
    const typename kernel<T0,TC,T1>::functype& _func,
    const typename kernel<T0,TC,T1>::scenario_table_type& _scenario_table,
    OIf<T0>& outS1,
    CIf<TC>& cS1,
    I1If<T1>& inpS1
    )
    
{
    auto p = new kernel<T0,TC,T1>(pName.c_str(), _func, _scenario_table);
    
    (*p).cport1(cS1);
    (*p).iport1(inpS1);
    (*p).oport1(outS1);
    
    return p;
}

//! Helper function to construct a kernelMN process
/*! This function is used to construct a kernelMN (SystemC module) and
 * connect its input and output signals.
 * It provides a more functional style definition of a ForSyDe process.
 * It also removes boilerplate code by using type-inference feature of
 * C++ and automatic binding to the input FIFOs.
 */
template <typename... TOs, typename TC, typename... TIs,
           template <class> class CIf,
           template <class> class... IIf,
           template <class> class... OIf>
inline kernelMN<std::tuple<TOs...>,TC,std::tuple<TIs...>>* make_kernelMN(const std::string& pName,
    const typename kernelMN<std::tuple<TOs...>,TC,std::tuple<TIs...>>::functype& _func,
    const typename kernelMN<std::tuple<TOs...>,TC,std::tuple<TIs...>>::scenario_table_type& _scenario_table,
    std::tuple<OIf<TOs>&...> outS,
    CIf<TC>& cS1,
    std::tuple<IIf<TIs>&...> inpS
    )

{
    auto p = new kernelMN<std::tuple<TOs...>,TC,std::tuple<TIs...>>(
        pName.c_str(),
        _func,
        _scenario_table
    );

    (*p).cport1(cS1);

    std::apply([&](auto&... inpS){
        std::apply([&](auto&... inpP){
            (inpP(inpS), ...);
        }, p->iport);
    }, inpS);

    std::apply([&](auto&... outS){
        std::apply([&](auto&... outP){
            (outP(outS), ...);
        }, p->oport);
    }, outS);

    return p;
}

//! As above, additionally reporting each firing to a self-report pipe.
/*! D10: report_pipe used to be spliced into the parameter list only
 * #ifdef FORSYDE_SELF_REPORTING, so this helper's shape depended on a
 * build macro -- enabling self-reporting meant editing the model source,
 * not just its CFLAGS. It is a separate overload now, always present.
 * The pipe keeps its position among the process-constructor parameters,
 * before the output and then input signals, because that ordering is the
 * helper layer's convention throughout (mirroring a curried function
 * signature, as in ForSyDe-Haskell) and a self-report pipe is a
 * parameter of the process, not a signal.
 *
 * Requires FORSYDE_SELF_REPORTING; without it this is a compile-time
 * error rather than an argument that quietly does nothing.
 */
template <typename... TOs, typename TC, typename... TIs,
           template <class> class CIf,
           template <class> class... IIf,
           template <class> class... OIf>
inline kernelMN<std::tuple<TOs...>,TC,std::tuple<TIs...>>* make_kernelMN(const std::string& pName,
    const typename kernelMN<std::tuple<TOs...>,TC,std::tuple<TIs...>>::functype& _func,
    const typename kernelMN<std::tuple<TOs...>,TC,std::tuple<TIs...>>::scenario_table_type& _scenario_table,
    FILE** report_pipe,   ///< the report named pipe
    std::tuple<OIf<TOs>&...> outS,
    CIf<TC>& cS1,
    std::tuple<IIf<TIs>&...> inpS
    )

{
    FORSYDE_REQUIRE_SELF_REPORTING(TC, "SADF::make_kernelMN");

    auto p = new kernelMN<std::tuple<TOs...>,TC,std::tuple<TIs...>>(
        pName.c_str(),
        _func,
        _scenario_table,
        report_pipe
    );

    (*p).cport1(cS1);

    std::apply([&](auto&... inpS){
        std::apply([&](auto&... inpP){
            (inpP(inpS), ...);
        }, p->iport);
    }, inpS);

    std::apply([&](auto&... outS){
        std::apply([&](auto&... outP){
            (outP(outS), ...);
        }, p->oport);
    }, outS);

    return p;
}

//! Helper function to construct a detector12 process
/*! This function is used to construct a detector12 (SystemC module) and
 * connect its output and output signals.
 * It provides a more functional style definition of a ForSyDe process.
 * It also removes bilerplate code by using type-inference feature of
 * C++ and automatic binding to the input FIFOs.
 */
template <typename T0, typename T1, typename TS,
           template <class> class OIf,
           template <class> class I1If>
inline detector<T0,T1,TS>* make_detector(const std::string& pName,
    const typename detector<T0,T1,TS>::cds_functype& _cds_func,
    const typename detector<T0,T1,TS>::kss_functype& _kss_func,
    const typename detector<T0,T1,TS>::scenario_table_type& scenario_table,
    const TS& init_sc,
    const size_t& i1toks,
    OIf<T0>& outS,
    I1If<T1>& inpS1
    )
{
    auto p = new detector<T0,T1,TS>(pName.c_str(), _cds_func, _kss_func, scenario_table, init_sc, i1toks);
    
    (*p).iport1(inpS1);
    (*p).oport1(outS);
    
    return p;
}

//! Helper function to construct a detectorMN process
/*! This function is used to construct a detectorMN (SystemC module) and
 * connect its output and output signals.
 * It provides a more functional style definition of a ForSyDe process.
 * It also removes bilerplate code by using type-inference feature of
 * C++ and automatic binding to the input FIFOs.
 */
template <typename... TOs, typename... TIs, typename TS,
           template <class> class... OIf,
           template <class> class... IIf>
inline detectorMN<std::tuple<TOs...>,std::tuple<TIs...>,TS>* make_detectorMN(const std::string& pName,
    const typename detectorMN<std::tuple<TOs...>,std::tuple<TIs...>,TS>::cds_functype& _cds_func,
    const typename detectorMN<std::tuple<TOs...>,std::tuple<TIs...>,TS>::kss_functype& _kss_func,
    const typename detectorMN<std::tuple<TOs...>,std::tuple<TIs...>,TS>::scenario_table_type& scenario_table,
    const TS& init_sc,
    const std::array<size_t,sizeof...(TIs)>& itoks,
    std::tuple<OIf<TOs>&...> outS,
    std::tuple<IIf<TIs>&...> inpS
    )
{
    auto p = new detectorMN<std::tuple<TOs...>,std::tuple<TIs...>,TS>(
        pName.c_str(),
        _cds_func,
        _kss_func,
        scenario_table,
        init_sc,
        itoks
    );

    std::apply([&](auto&... inpS){
        std::apply([&](auto&... inpP){
            (inpP(inpS), ...);
        }, p->iport);
    }, inpS);

    std::apply([&](auto&... outS){
        std::apply([&](auto&... outP){
            (outP(outS), ...);
        }, p->oport);
    }, outS);

    return p;
}

//! As above, additionally reporting each firing to a self-report pipe.
/*! D10: see the note on make_kernelMN above -- same reasoning, same
 * parameter position (with the process-constructor parameters, before
 * the output and input signals).
 */
template <typename... TOs, typename... TIs, typename TS,
           template <class> class... OIf,
           template <class> class... IIf>
inline detectorMN<std::tuple<TOs...>,std::tuple<TIs...>,TS>* make_detectorMN(const std::string& pName,
    const typename detectorMN<std::tuple<TOs...>,std::tuple<TIs...>,TS>::cds_functype& _cds_func,
    const typename detectorMN<std::tuple<TOs...>,std::tuple<TIs...>,TS>::kss_functype& _kss_func,
    const typename detectorMN<std::tuple<TOs...>,std::tuple<TIs...>,TS>::scenario_table_type& scenario_table,
    const TS& init_sc,
    const std::array<size_t,sizeof...(TIs)>& itoks,
    FILE** report_pipe,   ///< the report named pipe
    std::tuple<OIf<TOs>&...> outS,
    std::tuple<IIf<TIs>&...> inpS
    )
{
    FORSYDE_REQUIRE_SELF_REPORTING(TS, "SADF::make_detectorMN");

    auto p = new detectorMN<std::tuple<TOs...>,std::tuple<TIs...>,TS>(
        pName.c_str(),
        _cds_func,
        _kss_func,
        scenario_table,
        init_sc,
        itoks,
        report_pipe
    );

    std::apply([&](auto&... inpS){
        std::apply([&](auto&... inpP){
            (inpP(inpS), ...);
        }, p->iport);
    }, inpS);

    std::apply([&](auto&... outS){
        std::apply([&](auto&... outP){
            (outP(outS), ...);
        }, p->oport);
    }, outS);

    return p;
}

// template <class T, template <class> class OIf>
// using make_source = SDF::make_source<T,OIf>;

//! Helper function to construct a source process
/*! This function is used to construct a source (SystemC module) and
 * connect its output signal.
 * It provides a more functional style definition of a ForSyDe process.
 * It also removes bilerplate code by using type-inference feature of
 * C++ and automatic binding to the output FIFOs.
 */
template <class T, template <class> class OIf>
inline source<T>* make_source(std::string pName,
    typename source<T>::functype _func,
    T initval,
    unsigned long long take,
    OIf<T>& outS
    )
{
    auto p = new source<T>(pName.c_str(), _func, initval, take);
    
    (*p).oport1(outS);
    
    return p;
}

// template <class T, template <class> class IIf>
// using make_sink = SDF::make_sink<T,IIf>;

//! Helper function to construct a sink process
/*! This function is used to construct a sink (SystemC module) and
 * connect its output and output signals.
 * It provides a more functional style definition of a ForSyDe process.
 * It also removes bilerplate code by using type-inference feature of
 * C++ and automatic binding to the input FIFOs.
 */
template <class T, template <class> class IIf>
inline sink<T>* make_sink(std::string pName,
    typename sink<T>::functype _func,
    IIf<T>& inS
    )
{
    auto p = new sink<T>(pName.c_str(), _func);
    
    (*p).iport1(inS);
    
    return p;
}

//! Helper function to construct a delayn process
/*! This function is used to construct a process (SystemC module) and
 * connect its output and output signals.
 * It provides a more functional style definition of a ForSyDe process.
 * It also removes bilerplate code by using type-inference feature of
 * C++ and automatic binding to the input and output FIFOs.
 */
template <typename T, template <class> class IIf,
                        template <class> class OIf>
inline delayn<T>* make_delayn(std::string pName,
    T initval,
    unsigned int n,
    OIf<T>& outS,
    IIf<T>& inpS
    )
{
    auto p = new delayn<T>(pName.c_str(), initval, n);
    
    (*p).iport1(inpS);
    (*p).oport1(outS);
    
    return p;
}

}
}

#endif
