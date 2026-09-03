/**********************************************************************           
    * dde_process.hpp -- The DDE MoC process                          *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Providing the primitives for the DDE MoC                *
    *                                                                 *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef DDE_PROCESS_HPP
#define DDE_PROCESS_HPP

/*! \file dde_process.hpp
 * \brief Implements the abstract process in the DDE Model of Computation
 * 
 *  This files procides definitions for the signals, ports ans the
 * abstract base process used in the distributed discrete-event MoC.
 */

#include <systemc>
#include "tt_event.hpp"
#include "moc_traits.hpp"
#include "abssemantics.hpp"

namespace ForSyDe
{

namespace DDE
{

using namespace sc_core;

//! The DDE2DDE signal used to inter-connect DDE processes
template <typename T>
class DDE2DDE: public ForSyDe::signal<T,ttn_event<T>>
{
public:
    //! The model of computation this signal belongs to (D13)
    static constexpr ForSyDe::moc_id moc_tag = ForSyDe::moc_id::DDE;

    DDE2DDE() : ForSyDe::signal<T,ttn_event<T>>() {}
    DDE2DDE(sc_module_name name, unsigned size) : ForSyDe::signal<T,ttn_event<T>>(name, size) {}
#ifdef FORSYDE_INTROSPECTION
    
    virtual std::string moc() const
    {
        return "DDE";
    }
#endif
};

//! The DDE::signal is an alias for DDE::DDE2DDE
template <typename T>
using signal = DDE2DDE<T>;

//! The DDE_in port is used for input ports of DDE processes
template <typename T>
class DDE_in: public ForSyDe::in_port<T,ttn_event<T>,signal<T>>
{
public:
    //! The model of computation this port belongs to (D13)
    static constexpr ForSyDe::moc_id moc_tag = ForSyDe::moc_id::DDE;

    //! Bind, having first checked that the two MoCs are compatible
    /*! The carrier boundary is enforced by the token types already; this
     * is what catches a binding *within* a carrier, where the types
     * coincide but the models of computation do not.
     */
    template <typename Other>
    void operator()(Other& other)
    {
        ForSyDe::check_bind<Other::moc_tag, moc_tag>();
        ForSyDe::in_port<T,ttn_event<T>,signal<T>>::operator()(other);
    }

    DDE_in() : ForSyDe::in_port<T,ttn_event<T>,signal<T>>(){}
    DDE_in(const char* name) : ForSyDe::in_port<T,ttn_event<T>,signal<T>>(name){}
#ifdef FORSYDE_INTROSPECTION
    
    virtual std::string moc() const
    {
        return "DDE";
    }
#endif
};

//! The DDE::in_port is an alias for DDE::DDE_in
template <typename T>
using in_port = DDE_in<T>;

//! The DDE_out port is used for output ports of DDE processes
template <typename T>
class DDE_out: public ForSyDe::out_port<T,ttn_event<T>,signal<T>>
{
public:
    //! The model of computation this port belongs to (D13)
    static constexpr ForSyDe::moc_id moc_tag = ForSyDe::moc_id::DDE;

    //! Bind, having first checked that the two MoCs are compatible
    /*! The carrier boundary is enforced by the token types already; this
     * is what catches a binding *within* a carrier, where the types
     * coincide but the models of computation do not.
     */
    template <typename Other>
    void operator()(Other& other)
    {
        ForSyDe::check_bind<Other::moc_tag, moc_tag>();
        ForSyDe::out_port<T,ttn_event<T>,signal<T>>::operator()(other);
    }

    DDE_out() : ForSyDe::out_port<T,ttn_event<T>,signal<T>>(){}
    DDE_out(const char* name) : ForSyDe::out_port<T,ttn_event<T>,signal<T>>(name){}
#ifdef FORSYDE_INTROSPECTION
    
    virtual std::string moc() const
    {
        return "DDE";
    }
#endif
};

//! The DDE::out_port is an alias for DDE::DDE_out
template <typename T>
using out_port = DDE_out<T>;

//! Abstract semantics of a process in the DDE MoC
typedef ForSyDe::process dde_process;

}
}

#endif
