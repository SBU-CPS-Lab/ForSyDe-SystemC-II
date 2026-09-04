/**********************************************************************
    * binding.hpp -- positional signal binding and function-type       *
    *                deduction, shared by every MoC                    *
    *                                                                  *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)              *
    *                                                                  *
    * Purpose: Replacing the per-constructor make_* helper functions    *
    *          with one binder written against a process's port tuples  *
    *                                                                   *
    * Usage:   This file is included automatically                     *
    *                                                                  *
    * License: BSD3                                                    *
    *******************************************************************/

#ifndef FORSYDE_BINDING_HPP
#define FORSYDE_BINDING_HPP

/*! \file binding.hpp
 * \brief One binder and one set of function-signature traits for all MoCs
 */

#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace ForSyDe
{

//! Forward declarations, so that the traits below can see through the
//! MoCs' token wrappers without this header depending on any of them
template <typename T> class abst_ext;      // abst_ext.hpp
template <typename VT, typename TT> struct tt_event;   // tt_event.hpp

namespace detail
{

//! Bind a tuple of ports to a tuple of signals, one for one, in order
template <typename Ports, typename Sigs, std::size_t... I>
inline void bind_positional(Ports& ports, Sigs sigs, std::index_sequence<I...>)
{
    (std::get<I>(ports)(std::get<I>(sigs)), ...);
}

//! The token a MoC puts on the wire, back to the value the modeller means
/*! Every MoC wraps the modeller's type before it reaches a channel --
 * SY and DT in abst_ext, SDF and UT in a vector of them, DDE in a
 * ttn_event. A deduction guide has to undo that to recover the class's
 * own template arguments from the signature of the user's function.
 */
template <typename Tok> struct token_value {typedef Tok type;};
template <typename T> struct token_value<std::vector<T>>
    {typedef typename token_value<T>::type type;};
template <typename T> struct token_value<abst_ext<T>>
    {typedef typename token_value<T>::type type;};
// ttn_event<T> is tt_event<abst_ext<T>>, so DDE unwraps in two steps
template <typename VT, typename TT> struct token_value<tt_event<VT,TT>>
    {typedef typename token_value<VT>::type type;};

//! Gives a process constructor its positional signal binding
/*! Inherited by every process constructor, directly or through the core
 * of its family. \a Derived supplies in_ports() and out_ports() -- the
 * same two accessors the cores already use to read and write -- and
 * gets, in exchange, the operator() that replaces its make_* helper.
 *
 * That is the whole of what the 87 helper functions did. Each of them
 * was a hand-written new-expression followed by a fixed sequence of port
 * bindings, one per constructor per arity per MoC, and the sequence was
 * derivable from the port tuples all along.
 *
 * Whichever class inherits this must also re-declare the operator with
 * a using-declaration, and that is not boilerplate. sc_module already
 * has an operator() of its own -- SystemC's positional binding, which
 * binds a module's ports in declaration order -- so without the using
 * the two are found in different bases and every call is ambiguous.
 * With it, ForSyDe's hides SystemC's, which is what we want on two
 * counts: the orders differ, and SystemC's reaches the port through
 * sc_port::bind rather than through the introspective operator() that
 * in_port and out_port override. A model bound the SystemC way would
 * elaborate perfectly and then emit XML with no channels recorded in it.
 */
template <typename Derived>
class bindable
{
public:
    //! Bind this process's signals: outputs first, then inputs
    /*! The order is the one the make_* helpers used and the one the
     * introspection XML lists ports in, so a call reads the way the
     * process does -- out = f(in) -- and the rewrite of
     * make_comb(name, f, out, in) into comb(name, f) then p(out, in) is
     * mechanical.
     *
     * The count is checked here rather than at the port, so a missing or
     * an extra signal is a sentence about this process instead of a
     * template error from inside sc_port.
     */
    template <typename... Sigs>
    Derived& operator()(Sigs&... sigs)
    {
        auto& self = static_cast<Derived&>(*this);
        auto ports = std::tuple_cat(self.out_ports(), self.in_ports());
        static_assert(sizeof...(Sigs) == std::tuple_size<decltype(ports)>::value,
            "Wrong number of signals bound to this process. Give one per "
            "port: the outputs first, in order, then the inputs.");
        bind_positional(ports, std::tie(sigs...),
                        std::index_sequence_for<Sigs...>{});
        return self;
    }
};

//! The parameter list of anything callable, as a tuple
template <typename F> struct fn_traits
    : fn_traits<decltype(&std::decay_t<F>::operator())> {};
template <typename R, typename... A> struct fn_traits<R(*)(A...)>
    {typedef std::tuple<A...> args;};
template <typename R, typename... A> struct fn_traits<R(&)(A...)>
    {typedef std::tuple<A...> args;};
template <typename R, typename... A> struct fn_traits<R(A...)>
    {typedef std::tuple<A...> args;};
template <typename C, typename R, typename... A> struct fn_traits<R(C::*)(A...) const>
    {typedef std::tuple<A...> args;};
template <typename C, typename R, typename... A> struct fn_traits<R(C::*)(A...)>
    {typedef std::tuple<A...> args;};

//! The value type behind the \a N'th parameter of a user function
/*! This is what a deduction guide is written in terms of: given
 * f : (abst_ext<T0>&, const abst_ext<T1>&) -> void, arg_t<0,F> is T0 and
 * arg_t<1,F> is T1, which are exactly the class's template arguments.
 *
 * It works for a function pointer, a function reference and any callable
 * with a single non-template operator(), which covers every non-generic
 * lambda. A *generic* lambda has no one signature to read, so CTAD does
 * not apply to it and the template arguments have to be written out --
 * the same rule 2b arrived at for deducing the token policy.
 */
template <std::size_t N, typename F>
using arg_t = typename token_value<
    std::decay_t<std::tuple_element_t<N, typename fn_traits<std::decay_t<F>>::args>>
>::type;

} // namespace detail

} // namespace ForSyDe

#endif
