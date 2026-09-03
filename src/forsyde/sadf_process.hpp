/**********************************************************************           
    * sadf_process.hpp -- The SADF MoC process                        *
    *                                                                 *
    * Author:  Mohammad Vazirpanah (mohammad.vazirpanah@yahoo.com)    *
    *                                                                 *
    * Purpose: Providing the primitives for the SADF MoC              *
    *                                                                 *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef SADF_PROCESS_HPP
#define SADF_PROCESS_HPP

/*! \file sadf_process.hpp
 * \brief Implements the abstract process in the SADF Model of Computation
 * 
 *  This files procides definitions for the signals, ports ans the
 * abstract base process used in the SADF MoC.
 */

#include <systemc>
#include "moc_traits.hpp"
#include "abssemantics.hpp"
// SADF is built directly on top of UT's types (SADF2SADF : UT::UT2UT<T>,
// and likewise for SADF_in/SADF_out below), the same relationship SDF
// has to UT -- see the identical note in sdf_process.hpp.
#include "ut_process.hpp"

namespace ForSyDe
{

namespace SADF
{

using namespace sc_core;

//! The SADF2SADF signal used to inter-connect SADF processes
template <typename T>
class SADF2SADF: public UT::UT2UT<T>
{
public:
    //! The model of computation this signal belongs to (D13)
    static constexpr ForSyDe::moc_id moc_tag = ForSyDe::moc_id::SADF;

    SADF2SADF() : UT::UT2UT<T>() {}
    SADF2SADF(sc_module_name name, unsigned size) : UT::UT2UT<T>(name, size) {}
#ifdef FORSYDE_INTROSPECTION
    
    virtual std::string moc() const
    {
        return "SADF";
    }
#endif
};

//! The SADF::signal is an alias for SADF::SADF2SADF
template <typename T>
using signal = SADF2SADF<T>;

//! The SY_in port is used for input ports of SY processes
template <typename T>
class SADF_in: public ForSyDe::UT::UT_in<T>
{
public:
    //! The model of computation this port belongs to (D13)
    static constexpr ForSyDe::moc_id moc_tag = ForSyDe::moc_id::SADF;

    //! Bind, having first checked that the two MoCs are compatible
    /*! The carrier boundary is enforced by the token types already; this
     * is what catches a binding *within* a carrier, where the types
     * coincide but the models of computation do not.
     */
    template <typename Other>
    void operator()(Other& other)
    {
        ForSyDe::check_bind<Other::moc_tag, moc_tag>();
        ForSyDe::UT::UT_in<T>::operator()(other);
    }

    SADF_in() : ForSyDe::UT::UT_in<T>(){}
    SADF_in(const char* name) : ForSyDe::UT::UT_in<T>(name){}
#ifdef FORSYDE_INTROSPECTION
    
    virtual std::string moc() const
    {
        return "SADF";
    }
#endif
};

//! The SADF::in_port is an alias for SADF::SADF_in
template <typename T>
using in_port = SADF_in<T>;

//! The SY_out port is used for output ports of SY processes
template <typename T>
class SADF_out: public ForSyDe::UT::UT_out<T>
{
public:
    //! The model of computation this port belongs to (D13)
    static constexpr ForSyDe::moc_id moc_tag = ForSyDe::moc_id::SADF;

    //! Bind, having first checked that the two MoCs are compatible
    /*! The carrier boundary is enforced by the token types already; this
     * is what catches a binding *within* a carrier, where the types
     * coincide but the models of computation do not.
     */
    template <typename Other>
    void operator()(Other& other)
    {
        ForSyDe::check_bind<Other::moc_tag, moc_tag>();
        ForSyDe::UT::UT_out<T>::operator()(other);
    }

    SADF_out() : ForSyDe::UT::UT_out<T>(){}
    SADF_out(const char* name) : ForSyDe::UT::UT_out<T>(name){}
#ifdef FORSYDE_INTROSPECTION
    
    virtual std::string moc() const
    {
        return "SADF";
    }
#endif
};

//! The SADF::out_port is an alias for SADF::SADF_out
template <typename T>
using out_port = SADF_out<T>;

//! Abstract semantics of a process in the SY MoC
typedef ForSyDe::process SADF_process;

}
}

#endif
