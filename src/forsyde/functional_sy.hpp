/**********************************************************************
    * functional_sy.hpp -- the SY constructors, partially applied      *
    *                                                                  *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)              *
    *                                                                  *
    * Purpose: fn::SY::comb(f) and friends                             *
    *                                                                  *
    * Usage:   Included by functional.hpp                              *
    *                                                                  *
    * License: BSD3                                                    *
    *******************************************************************/

#ifndef FORSYDE_FUNCTIONAL_SY_HPP
#define FORSYDE_FUNCTIONAL_SY_HPP

/*! \file functional_sy.hpp
 * \brief The SY process constructors as values
 *
 * Each of these is three lines and the same three lines: name the
 * constructor for automatic instance naming, and hand back a factory
 * that builds the real process when the network applies it. The types
 * come from the function the modeller passed, through
 * fn::detail::callable_traits, so that fn::SY::comb(f) deduces what
 * SY::comb("name", f) would -- including from a lambda, which
 * std::function's own deduction cannot do.
 *
 * Adding a constructor to this surface is adding one of these. There is
 * deliberately no registry and no macro: the list is short, and a
 * wrapper that needs to say something unusual (sdelay's initial value
 * is not a function, ssource takes both) can just say it.
 */

namespace ForSyDe
{

namespace fn
{

//! The synchronous MoC
namespace SY
{

//! A combinational process over one input
/*! f is written over abst_ext<T>, as the explicit surface's is:
 *      void f(abst_ext<T0>& out, const abst_ext<T1>& in)
 */
template <typename F>
auto comb(F f)
{
    using T0 = detail::unwrap_abst_t<detail::arg_t<F,0>>;
    using T1 = detail::unwrap_abst_t<detail::arg_t<F,1>>;
    return make_pc("comb", [f](const char* n)
        {return new ForSyDe::SY::comb<T0,T1>(n, f);});
}

//! A combinational process over two inputs
template <typename F>
auto comb2(F f)
{
    using T0 = detail::unwrap_abst_t<detail::arg_t<F,0>>;
    using T1 = detail::unwrap_abst_t<detail::arg_t<F,1>>;
    using T2 = detail::unwrap_abst_t<detail::arg_t<F,2>>;
    return make_pc("comb2", [f](const char* n)
        {return new ForSyDe::SY::comb2<T0,T1,T2>(n, f);});
}

//! A combinational process over three inputs
template <typename F>
auto comb3(F f)
{
    using T0 = detail::unwrap_abst_t<detail::arg_t<F,0>>;
    using T1 = detail::unwrap_abst_t<detail::arg_t<F,1>>;
    using T2 = detail::unwrap_abst_t<detail::arg_t<F,2>>;
    using T3 = detail::unwrap_abst_t<detail::arg_t<F,3>>;
    return make_pc("comb3", [f](const char* n)
        {return new ForSyDe::SY::comb3<T0,T1,T2,T3>(n, f);});
}

//! A constant source, optionally stopping after \a take tokens
template <typename T>
auto constant(T init_val, unsigned long long take = 0)
{
    return make_pc("sconstant", [init_val, take](const char* n)
        {return new ForSyDe::SY::sconstant<T>(n, init_val, take);});
}

//! A source that iterates \a f from \a init_val
template <typename F, typename T>
auto source(F f, T init_val, unsigned long long take = 0)
{
    return make_pc("ssource", [f, init_val, take](const char* n)
        {return new ForSyDe::SY::ssource<T>(n, f, init_val, take);});
}

//! A sink, which is where a pipeline written with | ends
template <typename F>
auto sink(F f)
{
    using T = detail::unwrap_abst_t<detail::arg_t<F,0>>;
    return make_pc("ssink", [f](const char* n)
        {return new ForSyDe::SY::ssink<T>(n, f);});
}

//! A unit delay, which is what closes a feedback loop
/*! Paired with network::declare() and pc::into(); see functional.hpp.
 */
template <typename T>
auto delay(T init_val)
{
    return make_pc("sdelay", [init_val](const char* n)
        {return new ForSyDe::SY::sdelay<T>(n, init_val);});
}

} // namespace SY

} // namespace fn

} // namespace ForSyDe

#endif
