// tests/moc_binding -- what may be bound to what, across models of
// computation (D13).
//
// Every ForSyDe signal and port now carries its model of computation as
// a compile-time tag, and moc_traits.hpp turns those into a rule about
// which bindings are legal. This pins that rule.
//
// The rule, in one paragraph. A signal's *carrier* is what it physically
// holds: a bare T (untimed), an abst_ext<T> (synchronous), a time-tagged
// ttn_event<T> (timed), or a sub_signal over an interval (continuous).
// Crossing a carrier has always been impossible -- those are different
// C++ types, so the compiler rejected it long before any of this. What
// was unchecked is the inside of a carrier, where the token types
// coincide but the MoCs do not: UT, SDF and SADF all carry a bare T, and
// SY and DT both carry abst_ext<T>. That is where a model could bind
// across models of computation and nothing would say so.
//
// Within a carrier the MoCs differ in their firing rule -- how a process
// decides how many tokens to take per evaluation cycle. SDF fixes the
// rates at construction, SADF indexes them by a scenario token, UT lets
// the actor decide from its own state. A producer meeting the stricter
// contract already meets the looser one, so SDF -> SADF -> UT is a
// widening and is free. The reverse is not sound and is rejected: a UT
// actor's data-dependent rate can break the static schedule an SDF
// consumer is built on.
//
// SY and DT share a carrier but neither refines the other, because an
// absent event does not mean the same thing in both -- "no value this
// tick" against "a tick elapsed" -- so they are incomparable.
//
// The predicates below are checked unconditionally. The static_asserts
// inside check_bind() that act on a real binding are behind
// FORSYDE_STRICT_MOC, which this test defines; see the note there for
// why it is not yet the default.
#include <forsyde.hpp>
#include <iostream>
#include <iomanip>

using namespace ForSyDe;

// ---- the lattice itself, independent of any binding ------------------
static_assert( widens_to(moc_id::SDF,  moc_id::SADF), "SDF refines SADF");
static_assert( widens_to(moc_id::SDF,  moc_id::UT),   "SDF refines UT");
static_assert( widens_to(moc_id::SADF, moc_id::UT),   "SADF refines UT");
static_assert( widens_to(moc_id::SY,   moc_id::SY),   "reflexive");

static_assert( narrows_to(moc_id::UT,   moc_id::SDF),  "UT -> SDF narrows");
static_assert( narrows_to(moc_id::UT,   moc_id::SADF), "UT -> SADF narrows");
static_assert( narrows_to(moc_id::SADF, moc_id::SDF),  "SADF -> SDF narrows");

static_assert( incomparable(moc_id::SY, moc_id::DT), "SY and DT do not refine");
static_assert( incomparable(moc_id::DT, moc_id::SY), "...in either direction");
static_assert(!incomparable(moc_id::SY, moc_id::SDF),
              "different carriers are not 'incomparable', they are unrelated");

static_assert(moc_traits_carrier(moc_id::UT)  == carrier::untimed,     "");
static_assert(moc_traits_carrier(moc_id::SDF) == carrier::untimed,     "");
static_assert(moc_traits_carrier(moc_id::SADF)== carrier::untimed,     "");
static_assert(moc_traits_carrier(moc_id::SY)  == carrier::synchronous, "");
static_assert(moc_traits_carrier(moc_id::DT)  == carrier::synchronous, "");
static_assert(moc_traits_carrier(moc_id::DDE) == carrier::timed,       "");
static_assert(moc_traits_carrier(moc_id::CT)  == carrier::continuous,  "");

// ---- and that a legal binding really does still bind -----------------
// Only the widening and same-MoC cases can appear here: the rejected
// ones are rejected at compile time, which is the point, so they cannot
// be exercised from a program that has to compile. They are pinned by
// the narrows_to/incomparable assertions above instead.
SC_MODULE(legal_binds)
{
    SDF::SDF2SDF<int>   sdf_sig;
    SADF::SADF2SADF<int> sadf_sig;
    SY::SY2SY<int>      sy_sig;
    DDE::DDE2DDE<int>   dde_sig;

    UT::UT_in<int>      ut_from_sdf, ut_from_sadf;
    SADF::SADF_in<int>  sadf_from_sdf;
    SDF::SDF_in<int>    sdf_from_sdf;
    SY::SY_in<int>      sy_from_sy;
    DDE::DDE_in<int>    dde_from_dde;

    SC_CTOR(legal_binds)
    {
        ut_from_sdf(sdf_sig);       // SDF  -> UT    widening
        ut_from_sadf(sadf_sig);     // SADF -> UT    widening
        sadf_from_sdf(sdf_sig);     // SDF  -> SADF  widening
        sdf_from_sdf(sdf_sig);      // SDF  -> SDF   identity
        sy_from_sy(sy_sig);         // SY   -> SY    identity
        dde_from_dde(dde_sig);      // DDE  -> DDE   identity
    }
};

static const char* verdict(moc_id from, moc_id to)
{
    if (moc_traits_carrier(from) != moc_traits_carrier(to)) return "carrier";
    if (from == to)                                         return "same";
    if (widens_to(from, to))                                return "widen";
    if (narrows_to(from, to))                               return "NARROW";
    return "INCOMP";
}

int sc_main(int, char*[])
{
    const moc_id all[] = {moc_id::UT, moc_id::SDF, moc_id::SADF,
                          moc_id::SY, moc_id::DT, moc_id::DDE, moc_id::CT};

    std::cout << "producer -> consumer\n";
    std::cout << std::left << std::setw(6) << "";
    for (auto to : all) std::cout << std::setw(8) << moc_name(to);
    std::cout << "\n";
    for (auto from : all)
    {
        std::cout << std::setw(6) << moc_name(from);
        for (auto to : all) std::cout << std::setw(8) << verdict(from, to);
        std::cout << "\n";
    }

    legal_binds lb("lb");
    std::cout << "legal bindings elaborated\n";
    return 0;
}
