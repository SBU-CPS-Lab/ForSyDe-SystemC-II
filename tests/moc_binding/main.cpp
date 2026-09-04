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
// the actor decide from its own state.
//
// SDF and SADF are mutually compatible: an SADF process is an SDF
// process within any one scenario, which is why SADF re-exports several
// of SDF's constructors rather than repeating them, and why SADF models
// call SDF::make_unzip and SDF::make_zip directly on SADF signals. Both
// widen to UT, which assumes least.
//
// UT to either of them is a genuine narrowing -- a UT actor's rate is
// chosen per firing from its own state and can break a static schedule.
// That is reported under FORSYDE_STRICT_MOC rather than rejected, since
// it is a design question about a model, not a broken binding.
//
// SY and DT share a carrier but neither refines the other, because an
// absent event does not mean the same thing in both -- "no value this
// tick" against "a tick elapsed" -- so they are incomparable.
//
// Incomparable and cross-carrier bindings are rejected always. Narrowing
// is reported only under FORSYDE_STRICT_MOC, which this test defines.
// The predicates themselves are checked unconditionally.
#include <forsyde.hpp>
#include <iostream>
#include <iomanip>

using namespace ForSyDe;

// ---- the lattice itself, independent of any binding ------------------
static_assert( widens_to(moc_id::SDF,  moc_id::UT),   "SDF refines UT");
static_assert( widens_to(moc_id::SADF, moc_id::UT),   "SADF refines UT");
static_assert( widens_to(moc_id::SY,   moc_id::SY),   "reflexive");

// SDF and SADF are mutually compatible rather than ordered: an SADF
// process is an SDF process within any one scenario. This is why SADF
// re-exports several of SDF's constructors instead of repeating them,
// and why SADF models call SDF::make_unzip directly.
static_assert( widens_to(moc_id::SDF,  moc_id::SADF), "SADF is SDF per scenario");
static_assert( widens_to(moc_id::SADF, moc_id::SDF),  "...and the other way");
static_assert(!narrows_to(moc_id::SADF, moc_id::SDF), "so neither narrows");

// A UT actor picks its rate per firing from its own state, which a
// static SDF schedule cannot assume. That one is a real narrowing --
// reported under FORSYDE_STRICT_MOC, not rejected outright.
static_assert( narrows_to(moc_id::UT,   moc_id::SDF),  "UT -> SDF narrows");
static_assert( narrows_to(moc_id::UT,   moc_id::SADF), "UT -> SADF narrows");

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
    // One signal per binding: an sc_fifo admits a single reader, and
    // these are only ever elaborated, never run.
    SDF::SDF2SDF<int>    sdf_a, sdf_b, sdf_c;
    SADF::SADF2SADF<int> sadf_a, sadf_b;
    SY::SY2SY<int>       sy_sig;
    DDE::DDE2DDE<int>    dde_sig;

    UT::UT_in<int>      ut_from_sdf, ut_from_sadf;
    SADF::SADF_in<int>  sadf_from_sdf;
    SDF::SDF_in<int>    sdf_from_sdf, sdf_from_sadf;
    SY::SY_in<int>      sy_from_sy;
    DDE::DDE_in<int>    dde_from_dde;

    SC_CTOR(legal_binds)
    {
        ut_from_sdf(sdf_a);         // SDF  -> UT    widening
        ut_from_sadf(sadf_a);       // SADF -> UT    widening
        sadf_from_sdf(sdf_b);       // SDF  -> SADF  compatible
        sdf_from_sadf(sadf_b);      // SADF -> SDF   compatible, and the
                                    //   binding four SADF models make
        sdf_from_sdf(sdf_c);        // SDF  -> SDF   identity
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

// ---- and what needs an interface, once bound through one ------------
// Jantsch chapter 6 organises MoC interfaces by what they do to timing
// information. Table 6-1 has six entries; the definitions then collapse
// them, since stripS2U is defined as being stripT2U (6.2) and both
// insertU2T and insertS2T as being insertU2S (6.5, 6.6). Two operations
// are left, and both are checked here:
//
//   strip   drop the absent events, keep the rest in order  (6.1, 6.2)
//   insert  emit the event, then lambda-1 absent ones       (6.4-6.6)
//   group   lambda events into one, keeping the last present (6.3)
//
// group is the third, and the one operation of the three that changes
// the events rather than only what is said about when they happen. It is
// DT to SY: lambda DT ticks are one SY clock cycle, and the cycle's
// value is the last event present in it.
//
// None of these MoC pairs had an interface before: only SY <-> SDF was
// written by hand, and these are written over a pair of MoCs. DT in
// particular had no interface of any kind -- it could not be entered or
// left -- which is checked here as a round trip, since SY2DT and DT2SY
// at the same lambda have to compose to the identity.
SC_MODULE(through_an_interface)
{
    SY::signal<int> sy_src, sy_out, sy_src2, sy_back, sy_last;
    UT::signal<int> ut_mid, ut_src;
    DT::signal<int> dt_mid, dt_tap, dt_many;

    SC_CTOR(through_an_interface)
    {
        // 1 _ 2 _ _ 3  ->  1 2 3
        SY::make_vsource("s", {abst_ext<int>(1), abst_ext<int>(),
                               abst_ext<int>(2), abst_ext<int>(),
                               abst_ext<int>(), abst_ext<int>(3)}, sy_src);
        auto st = new MI::strip<moc_id::SY, moc_id::UT, int>("strip1");
        st->iport1(sy_src); st->oport1(ut_mid);
        UT::make_sink("ru", [](const int& v)
            {std::cout << "strip  " << v << "\n";}, ut_mid);

        // 7 8  at lambda = 3  ->  7 _ _ 8 _ _
        UT::make_vsource("t", {7,8}, ut_src);
        auto ins = new MI::insert<moc_id::UT, moc_id::SY, int>("insert1", 3);
        ins->iport1(ut_src); ins->oport1(sy_out);
        SY::make_sink("rs", [](const abst_ext<int>& v)
            {std::cout << "insert " << v << "\n";}, sy_out);

        // 5 _ 6  at lambda = 3  ->  DT: 5 _ _ _ _ _ 6 _ _  ->  SY: 5 _ 6
        SY::make_vsource("u", {abst_ext<int>(5), abst_ext<int>(),
                               abst_ext<int>(6)}, sy_src2);
        auto s2d = new SY2DT<int>("s2d", 3);
        s2d->iport1(sy_src2);
        s2d->oport1(dt_mid);            // multiport: both the sink and the
        s2d->oport1(dt_tap);            // return leg see the DT stream
        DT::make_sink("rd", [](const abst_ext<int>& v)
            {std::cout << "dt     " << v << "\n";}, dt_tap);
        auto d2s = new DT2SY<int>("d2s", 3);
        d2s->iport1(dt_mid); d2s->oport1(sy_back);
        SY::make_sink("rb", [](const abst_ext<int>& v)
            {std::cout << "round  " << v << "\n";}, sy_back);

        // lastt, which the round trip above cannot show: a clock cycle
        // holding more than one present event keeps the *last* of them,
        // and one holding none is absent.
        // DT::vsource is given (tick, value) pairs and is absent between
        // them, which is the DT tick doing exactly what SY's cannot.
        //   1 2 _ | _ 9 _ | _ _ 7   ->   2 9 7
        DT::make_vsource("v", {{0,1}, {1,2}, {4,9}, {8,7}}, dt_many);
        auto lastt = new DT2SY<int>("lastt", 3);
        lastt->iport1(dt_many); lastt->oport1(sy_last);
        SY::make_sink("rl", [](const abst_ext<int>& v)
            {std::cout << "lastt  " << v << "\n";}, sy_last);
    }
};

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

    through_an_interface ti("ti");
    sc_core::sc_start();
    return 0;
}
