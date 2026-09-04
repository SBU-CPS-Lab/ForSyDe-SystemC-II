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

//! Forward declarations, so that the helpers below can be written here
//! without this header depending on any of the MoCs, or on abssemantics

template <typename T> class abst_ext;      // abst_ext.hpp
template <typename VT, typename TT> struct tt_event;   // tt_event.hpp
#ifdef FORSYDE_INTROSPECTION
struct PortInfo;                                       // abssemantics.hpp
#endif

namespace detail
{

//! Bind a tuple of ports to a tuple of signals, one for one, in order
template <typename Ports, typename Sigs, std::size_t... I>
inline void bind_positional(Ports& ports, Sigs sigs, std::index_sequence<I...>)
{
    (std::get<I>(ports)(std::get<I>(sigs)), ...);
}

#ifdef FORSYDE_INTROSPECTION
//! Record a tuple of port references in one of a process's bound-channel vectors
/*! The other half of what the two port accessors buy. bindInfo() needs
 * exactly what the binder needs -- the input ports in order, the output
 * ports in order -- and it was written out by hand in 102 places, one of
 * which (SADF::detectorMN) was silently wrong because nothing
 * instantiated it. From here every one of them is the same line.
 */
template <typename Ports>
inline void record_ports(std::vector<PortInfo>& chans, Ports&& ports)
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

//! The token a MoC puts on the wire, back to the value the modeller means
/*! Every MoC wraps the modeller's type before it reaches a channel --
 * SY and DT in abst_ext, SDF and UT in a vector of them, DDE in a
 * ttn_event. A deduction guide has to undo that to recover the class's
 * own template arguments from the signature of the user's function.
 */
template <typename Tok> struct token_value {typedef Tok type;};
template <typename T> struct token_value<std::vector<T>> {typedef T type;};
template <typename T> struct token_value<abst_ext<T>>     {typedef T type;};
// ttn_event<T> is tt_event<abst_ext<T>>, so DDE names both layers rather
// than chaining two single-level unwraps automatically. Chaining them --
// as an earlier version of this template did, by having each
// specialization recurse into token_value<T> instead of naming T
// directly -- looked like it handled every case with one rule, but it
// is wrong for SY: a synchronous signal's own value type is free to be
// a std::vector<X> (the modeller's choice, nothing to do with a MoC's
// rate mechanism), and a recursive unwrap cannot tell that apart from
// SDF or UT's std::vector<T>, which *is* purely a rate wrapper. Found
// by SY::comb deducing T0 = complex<double> instead of
// vector<complex<double>> for a function returning
// abst_ext<vector<complex<double>>> -- the vector was real, and the
// generic rule stripped it anyway.
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
//! Does \a P declare the accessor at all?
/*! A few process constructors have only inputs (sink, printSigs) or only
 * outputs (source, constant), so the binder has to ask rather than
 * assume both are there.
 */
template <typename P, typename = void> struct has_in_ports : std::false_type {};
template <typename P> struct has_in_ports<P,
    std::void_t<decltype(std::declval<P&>().in_ports())>> : std::true_type {};
template <typename P, typename = void> struct has_out_ports : std::false_type {};
template <typename P> struct has_out_ports<P,
    std::void_t<decltype(std::declval<P&>().out_ports())>> : std::true_type {};

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
        auto ports = all_ports(self);
        static_assert(sizeof...(Sigs) == std::tuple_size<decltype(ports)>::value,
            "Wrong number of signals bound to this process. Give one per "
            "port: the outputs first, in order, then the inputs.");
        bind_positional(ports, std::tie(sigs...),
                        std::index_sequence_for<Sigs...>{});
        return self;
    }

private:
    //! Outputs then inputs, skipping whichever half this process lacks
    static auto all_ports(Derived& self)
    {
        if constexpr (has_out_ports<Derived>::value && has_in_ports<Derived>::value)
            return std::tuple_cat(self.out_ports(), self.in_ports());
        else if constexpr (has_out_ports<Derived>::value)
            return self.out_ports();
        else
            return self.in_ports();
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
