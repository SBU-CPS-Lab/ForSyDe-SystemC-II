/**********************************************************************           
    * dt_process.hpp -- The discrete-time MoC process                 *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Providing the primitives for the DT MoC                *
    *                                                                 *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef DT_PROCESS_HPP
#define DT_PROCESS_HPP

/*! \file dt_process.hpp
 * \brief Implements the abstract process in the DT Model of Computation
 * 
 *  This files procides definitions for the signals, ports and the
 * abstract base process used in the discrete-time MoC.
 */

#include <systemc>
#include "abst_ext.hpp"
#include "moc_traits.hpp"
#include "abssemantics.hpp"

namespace ForSyDe
{

namespace DT
{

using namespace sc_core;

//! The DT2DT signal used to inter-connect DT processes
template <typename T>
class DT2DT: public ForSyDe::signal<T,abst_ext<T>>
{
public:
    //! The model of computation this signal belongs to (D13)
    static constexpr ForSyDe::moc_id moc_tag = ForSyDe::moc_id::DT;

    DT2DT() : ForSyDe::signal<T,abst_ext<T>>() {}
    DT2DT(sc_module_name name, unsigned size) : ForSyDe::signal<T,abst_ext<T>>(name, size) {}
#ifdef FORSYDE_INTROSPECTION
    
    virtual std::string moc() const
    {
        return "DT";
    }
#endif
};

//! The DT::signal is an alias for DT::DT2DT
template <typename T>
using signal = DT2DT<T>;

//! The DT_in port is used for input ports of DT processes
template <typename T>
class DT_in: public ForSyDe::in_port<T,abst_ext<T>,signal<T>>
{
public:
    //! The model of computation this port belongs to (D13)
    static constexpr ForSyDe::moc_id moc_tag = ForSyDe::moc_id::DT;

    //! Bind, having first checked that the two MoCs are compatible
    /*! The carrier boundary is enforced by the token types already; this
     * is what catches a binding *within* a carrier, where the types
     * coincide but the models of computation do not.
     */
    template <typename Other>
    void operator()(Other& other)
    {
        ForSyDe::check_bind<Other::moc_tag, moc_tag>();
        ForSyDe::in_port<T,abst_ext<T>,signal<T>>::operator()(other);
    }

    DT_in() : ForSyDe::in_port<T,abst_ext<T>,signal<T>>(){}
    DT_in(const char* name) : ForSyDe::in_port<T,abst_ext<T>,signal<T>>(name){}
#ifdef FORSYDE_INTROSPECTION
    
    virtual std::string moc() const
    {
        return "DT";
    }
#endif
};

//! The DT::in_port is an alias for DT::DT_in
template <typename T>
using in_port = DT_in<T>;

//! The DT_out port is used for output ports of DT processes
template <typename T>
class DT_out: public ForSyDe::out_port<T,abst_ext<T>,signal<T>>
{
public:
    //! The model of computation this port belongs to (D13)
    static constexpr ForSyDe::moc_id moc_tag = ForSyDe::moc_id::DT;

    //! Bind, having first checked that the two MoCs are compatible
    /*! The carrier boundary is enforced by the token types already; this
     * is what catches a binding *within* a carrier, where the types
     * coincide but the models of computation do not.
     */
    template <typename Other>
    void operator()(Other& other)
    {
        ForSyDe::check_bind<Other::moc_tag, moc_tag>();
        ForSyDe::out_port<T,abst_ext<T>,signal<T>>::operator()(other);
    }

    DT_out() : ForSyDe::out_port<T,abst_ext<T>,signal<T>>(){}
    DT_out(const char* name) : ForSyDe::out_port<T,abst_ext<T>,signal<T>>(name){}
#ifdef FORSYDE_INTROSPECTION
    
    virtual std::string moc() const
    {
        return "DT";
    }
#endif
};

//! The DT::out_port is an alias for DT::DT_out
template <typename T>
using out_port = DT_out<T>;

//! Abstract semantics of a process in the DT MoC
typedef ForSyDe::process dt_process;

}
}

#endif
