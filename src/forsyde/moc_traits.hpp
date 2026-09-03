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
        case moc_id::SDF:  return to == moc_id::SADF || to == moc_id::UT;
        case moc_id::SADF: return to == moc_id::UT;
        default:           return false;
    }
}

//! Is the reverse direction a widening? Then this one is a narrowing.
constexpr bool narrows_to(moc_id from, moc_id to)
{
    return from != to && widens_to(to, from);
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
/*! Instantiated from each MoC's port classes. Split into three
 * static_asserts rather than one so that the message a user sees names
 * the specific thing that is wrong with their model, since the type
 * names in the surrounding diagnostic are not going to help them.
 *
 * \note Currently behind FORSYDE_STRICT_MOC rather than on by default,
 * and that is a decision waiting on an answer rather than a preference.
 * Turning it on rejects four of the models in this repository, all for
 * one reason: SADF re-exports several of SDF's components -- delayn,
 * source, sink, combMN -- as type aliases, so their ports are typed
 * SDF_in and SDF_out. An SADF model that uses SADF::make_delayn is
 * therefore binding an SADF signal to an SDF port, which is a narrowing
 * and is exactly what this check is for. The check is right; the aliases
 * are what make it fire.
 *
 * There is more than one defensible way out -- give SADF its own
 * components, retype the shared ones as carrier-U (UT_in/UT_out) so that
 * SDF and SADF both reach them by widening, or decide that these
 * particular rate-static components are polymorphic in their MoC -- and
 * they differ in what they claim about the library's structure, not just
 * in effort. So the machinery ships checked and tested, the models keep
 * building, and the choice stays open. tests/moc_binding exercises the
 * lattice either way.
 */
template <moc_id From, moc_id To>
constexpr void check_bind()
{
#ifdef FORSYDE_STRICT_MOC
    static_assert(!narrows_to(From, To),
        "Binding narrows the model of computation: the signal is produced "
        "under a freer firing rule than the port's process assumes, so the "
        "producer may consume or emit at a rate the consumer's schedule "
        "does not allow. This is not a conversion that can be applied "
        "silently -- it needs an explicit refinement that checks the rate "
        "at each firing.");
    static_assert(!incomparable(From, To),
        "Binding crosses two models of computation that share a token type "
        "but not a meaning, so neither is a refinement of the other. An "
        "absent event does not denote the same thing on both sides of this "
        "binding. Convert explicitly through a MoC interface.");
    static_assert(moc_traits_carrier(From) == moc_traits_carrier(To),
        "Binding crosses two models of computation that do not even carry "
        "the same kind of token. A MoC interface is required.");
#endif
}

}

#endif
