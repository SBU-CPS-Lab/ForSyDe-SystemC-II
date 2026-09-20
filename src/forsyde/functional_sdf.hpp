/**********************************************************************
    * functional_sdf.hpp -- the SDF constructors, partially applied    *
    *                                                                  *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)              *
    *                                                                  *
    * Purpose: fn::SDF::comb(f, o, i) and friends                      *
    *                                                                  *
    * Usage:   Included by functional.hpp                              *
    *                                                                  *
    * License: BSD3                                                    *
    *******************************************************************/

#ifndef FORSYDE_FUNCTIONAL_SDF_HPP
#define FORSYDE_FUNCTIONAL_SDF_HPP

/*! \file functional_sdf.hpp
 * \brief The SDF process constructors as values
 *
 * The same shape as the SY ones, with the rates carried in the
 * partially applied value alongside the function -- which is the point
 * the roadmap makes about currying: SDF::comb(f, 2, 1) is a value
 * holding f *and its rates*, and it is still a Signal a -> Signal b
 * once it has them.
 *
 * An SDF function is written over std::vector<T>, one entry per token
 * the rate declares, so the token type is a vector element here where
 * it is an abst_ext payload in SY.
 */

namespace ForSyDe
{

namespace fn
{

//! The synchronous dataflow MoC
namespace SDF
{

//! A combinational process over one input, with declared rates
template <typename F>
auto comb(F f, unsigned int o1toks, unsigned int i1toks)
{
    using T0 = detail::unwrap_vec_t<detail::arg_t<F,0>>;
    using T1 = detail::unwrap_vec_t<detail::arg_t<F,1>>;
    return make_pc("comb", [f, o1toks, i1toks](const char* n)
        {return new ForSyDe::SDF::comb<T0,T1>(n, f, o1toks, i1toks);});
}

//! A combinational process over two inputs, with declared rates
template <typename F>
auto comb2(F f, unsigned int o1toks, unsigned int i1toks, unsigned int i2toks)
{
    using T0 = detail::unwrap_vec_t<detail::arg_t<F,0>>;
    using T1 = detail::unwrap_vec_t<detail::arg_t<F,1>>;
    using T2 = detail::unwrap_vec_t<detail::arg_t<F,2>>;
    return make_pc("comb2", [f, o1toks, i1toks, i2toks](const char* n)
        {return new ForSyDe::SDF::comb2<T0,T1,T2>(n, f, o1toks, i1toks, i2toks);});
}

//! A source that iterates \a f from \a init_val
template <typename F, typename T>
auto source(F f, T init_val, unsigned long long take = 0)
{
    return make_pc("source", [f, init_val, take](const char* n)
        {return new ForSyDe::SDF::source<T>(n, f, init_val, take);});
}

//! A sink
template <typename F>
auto sink(F f)
{
    using T = detail::unwrap_vec_t<detail::arg_t<F,0>>;
    return make_pc("sink", [f](const char* n)
        {return new ForSyDe::SDF::sink<T>(n, f);});
}

//! A unit delay, which carries one initial token
template <typename T>
auto delay(T init_val)
{
    return make_pc("delay", [init_val](const char* n)
        {return new ForSyDe::SDF::delay<T>(n, init_val);});
}

} // namespace SDF

} // namespace fn

} // namespace ForSyDe

#endif
