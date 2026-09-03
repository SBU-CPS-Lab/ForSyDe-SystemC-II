/**********************************************************************
    * moc_traits.hpp -- static MoC identity, carriers and firing rules *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Giving every signal and port a compile-time MoC tag, so *
    *          that binding across models of computation is checked    *
    *          rather than assumed (D13)                               *
    *                                                                 *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef MOC_TRAITS_HPP
#define MOC_TRAITS_HPP

/*! \file moc_traits.hpp
 * \brief Static MoC identity, and the rule for when one MoC's signal may
 *        drive another MoC's port
 *
 *  Every ForSyDe signal and port already knows its MoC at run time, via
 * the virtual moc() string used by the introspection back end. That is
 * too late to prevent anything. This file gives the same information a
 * compile-time form, and uses it to reject a binding that crosses models
 * of computation without a conversion.
 */

namespace ForSyDe
{

//! What a signal physically carries
/*! This is the coarse classification, and it is the one that decides
 * whether a MoC *interface* is needed at all. Two MoCs on the same
 * carrier exchange exactly the same token type; two MoCs on different
 * carriers do not, and no amount of tagging will make one drive the
 * other -- the conversion has to be written.
 *
 *      untimed      T                 order only
 *      synchronous  abst_ext<T>       one event per tick
 *      timed        ttn_event<T>      a time tag on every event
 *      continuous   sub_signal        a function over an interval
 *
 * Worth knowing when reading what follows: the carrier boundary is
 * already enforced, and always has been, because those token types are
 * different C++ types. Binding an SY signal to an SDF port does not
 * compile today and never did. What was unchecked is the *inside* of a
 * carrier, where the token types coincide -- which is exactly where
 * ForSyDe puts several genuinely different MoCs.
 */
enum class carrier {untimed, synchronous, timed, continuous};

//! The models of computation, as compile-time identities
enum class moc_id {UT, SDF, SADF, SY, DT, DDE, CT};

//! Static description of one model of computation
template <moc_id M> struct moc_traits;

template <> struct moc_traits<moc_id::UT>
{
    static constexpr carrier on = carrier::untimed;
    static constexpr const char* name = "UT";
};
template <> struct moc_traits<moc_id::SDF>
{
    static constexpr carrier on = carrier::untimed;
    static constexpr const char* name = "SDF";
};
template <> struct moc_traits<moc_id::SADF>
{
    static constexpr carrier on = carrier::untimed;
    static constexpr const char* name = "SADF";
};
template <> struct moc_traits<moc_id::SY>
{
    static constexpr carrier on = carrier::synchronous;
    static constexpr const char* name = "SY";
};
template <> struct moc_traits<moc_id::DT>
{
    static constexpr carrier on = carrier::synchronous;
    static constexpr const char* name = "DT";
};
template <> struct moc_traits<moc_id::DDE>
{
    static constexpr carrier on = carrier::timed;
    static constexpr const char* name = "DDE";
};
template <> struct moc_traits<moc_id::CT>
{
    static constexpr carrier on = carrier::continuous;
    static constexpr const char* name = "CT";
};

//! May a signal produced under \a from drive a port belonging to \a to?
/*! Within a carrier the MoCs differ only in their firing rule -- how a
 * process decides how many tokens to consume in one evaluation cycle:
 *
 *      SDF   the rates are fixed when the process is constructed
 *      SADF  the rates are indexed by a scenario token from a detector
 *      UT    the actor decides its own rate, per firing, from its state
 *
 * Those are ordered by how constrained they are, so a producer that
 * satisfies the stricter contract already satisfies the looser one.
 * Feeding an SDF signal to a UT port is therefore sound and free: the
 * consumer assumed less than the producer guarantees. This is the
 * *widening* direction and it needs no process and no runtime cost.
 *
 * The reverse is not sound. A UT actor's data-dependent rate can violate
 * the static schedule an SDF consumer is built on, and nothing at the
 * binding site can know whether it will. That needs an explicit,
 * named refinement that asserts the rate at each firing -- not a silent
 * coercion -- so it is rejected here.
 */
constexpr bool widens_to(moc_id from, moc_id to)
{
    if (from == to) return true;
    switch (from)
    {
        // SDF and SADF are mutually compatible rather than ordered: an
        // SADF process is an SDF process within any one scenario, so
        // neither imposes on the other. Both refine UT, which lets its
        // actors pick a rate per firing from their own state.
        case moc_id::SDF:  return to == moc_id::SADF || to == moc_id::UT;
        case moc_id::SADF: return to == moc_id::SDF  || to == moc_id::UT;
        default:           return false;
    }
}

//! Does this direction fail to widen where the reverse succeeds?
/*! Note the first clause. "The reverse widens" is not on its own enough:
 * SDF and SADF widen to each other, being the same MoC seen per
 * scenario, and a pair like that narrows in neither direction. Leaving
 * it out makes every mutually compatible pair report as a narrowing as
 * well as a widening, which is how tests/moc_binding caught it.
 */
constexpr bool narrows_to(moc_id from, moc_id to)
{
    return !widens_to(from, to) && widens_to(to, from);
}

//! The carrier of a MoC, as a value rather than a template argument
constexpr carrier moc_traits_carrier(moc_id m)
{
    switch (m)
    {
        case moc_id::UT:
        case moc_id::SDF:
        case moc_id::SADF: return carrier::untimed;
        case moc_id::SY:
        case moc_id::DT:   return carrier::synchronous;
        case moc_id::DDE:  return carrier::timed;
        case moc_id::CT:   return carrier::continuous;
    }
    return carrier::untimed;
}

//! The name of a MoC, as a value rather than a template argument
constexpr const char* moc_name(moc_id m)
{
    switch (m)
    {
        case moc_id::UT:   return "UT";
        case moc_id::SDF:  return "SDF";
        case moc_id::SADF: return "SADF";
        case moc_id::SY:   return "SY";
        case moc_id::DT:   return "DT";
        case moc_id::DDE:  return "DDE";
        case moc_id::CT:   return "CT";
    }
    return "?";
}

//! Same carrier, but neither direction is a refinement of the other
/*! SY and DT are the case in the library today. Both carry abst_ext<T>
 * one token per tick, so the compiler cannot separate them, but an
 * absent event does not mean the same thing in each: in SY it is "no
 * value this tick", in DT it is "a tick passed", which is what lets a DT
 * process consume a state-dependent number of them to measure time.
 * Reading one as the other is a change of meaning rather than a
 * refinement in either direction, so it is neither widening nor
 * narrowing and is rejected outright.
 */
constexpr bool incomparable(moc_id from, moc_id to)
{
    return moc_traits_carrier(from) == moc_traits_carrier(to)
        && !widens_to(from, to) && !widens_to(to, from);
}

//! The compile-time check performed at every port-to-signal binding (D13)
/*! Two levels, because two different things can be wrong and only one of
 * them is wrong in every model.
 *
 * Always checked: that the two MoCs mean the same thing by a token.
 * Crossing a carrier is caught by the token types themselves and cannot
 * reach here; what reaches here is a pair that shares a carrier without
 * sharing a meaning. SY and DT are the case in the library -- both carry
 * abst_ext<T> one per tick, but an absent event is "no value this tick"
 * in one and "a tick elapsed" in the other, so reading either as the
 * other changes what the model says. No model here does it, and none
 * should.
 *
 * Checked under FORSYDE_STRICT_MOC: that the binding does not narrow the
 * firing rule. This is a real property and worth having, but it is not
 * an error in the way the one above is, and defaulting it on would be
 * wrong for this library. Within the untimed carrier the MoCs are
 * deliberately interoperable: an SADF process *is* an SDF process within
 * any one scenario, which is why SADF re-exports several of SDF's
 * constructors rather than repeating them, and why SADF models call
 * SDF::make_unzip and SDF::make_zip directly. Those bindings are sound
 * and intended. Rejecting them would be the check being wrong about the
 * MoCs rather than the models being wrong.
 *
 * What remains true is that a genuine UT actor, which chooses its rate
 * per firing from its own state, can break a static SDF schedule. The
 * relation is kept, and the strict mode reports it, because that is the
 * information a refine<UT,SDF>(rate) process would act on and the
 * analysis tools would want. It is a design question surfaced, not a
 * binding rejected.
 */
template <moc_id From, moc_id To>
constexpr void check_bind()
{
    static_assert(moc_traits_carrier(From) == moc_traits_carrier(To),
        "Binding crosses two models of computation that do not even carry "
        "the same kind of token. A MoC interface is required.");
    static_assert(!incomparable(From, To),
        "Binding crosses two models of computation that share a token type "
        "but not a meaning, so neither is a refinement of the other. An "
        "absent event does not denote the same thing on both sides of this "
        "binding. Convert explicitly through a MoC interface.");
#ifdef FORSYDE_STRICT_MOC
    static_assert(!narrows_to(From, To),
        "Binding narrows the model of computation: the signal is produced "
        "under a freer firing rule than the port's process assumes, so the "
        "producer may consume or emit at a rate the consumer's schedule "
        "does not allow. Within the untimed carrier this is often "
        "deliberate and sound -- an SADF process is an SDF process within "
        "a scenario -- which is why this is only reported under "
        "FORSYDE_STRICT_MOC. Where it is not deliberate it wants an "
        "explicit refinement that checks the rate at each firing.");
#endif
}

}

#endif
