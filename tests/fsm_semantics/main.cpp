// tests/fsm_semantics -- pins the emitted sequence of every state-machine
// process constructor against Jantsch's definitions.
//
// tests/instantiate proves these compile. Nothing proved they *compute*
// the right thing: not one example in the repository uses SY::moore,
// UT::moore, UT::mooreMN, UT::scan or UT::scand, so their semantics were
// unpinned, and SY's Moore and UT's Moore had drifted apart -- UT emitted
// its first output twice. Neither golden file nor compiler could see it.
//
// Every machine below is the same counter -- state w, g(w,a) = w+1,
// starting at 0 -- so the emitted sequence reads directly as the states
// it passed through, and an off-by-one is visible by eye rather than
// having to be derived.
//
// The expected sequences, from the book:
//
//   scanU (3.2)    emits w_{i+1}            ->  1 2 3 4 ...
//   scandU (3.8)   = <w_0> + scanU          ->  0 1 2 3 ...
//   mooreU (3.4)   emits f(w_i)             ->  0 10 20 30 ...
//   mealyU (3.3)   emits f(w_i, a_i)        ->  see below
//
// f is deliberately not the identity, and the input is a ramp rather
// than a constant. With f = id and a constant input a Moore machine and
// a scand emit the same sequence, and a Mealy machine cannot be told
// from a Moore one -- so the test would agree with three different wrong
// implementations. A scand emits the state itself; a Moore machine emits
// f of it. That is the difference this encodes.
//
// and (4.3)-(4.6) give scanS/scandS/mooreS/mealyS as the untimed
// constructors at gamma = 1, so the SY and UT rows must agree.
#include <forsyde.hpp>
#include <iostream>

using namespace ForSyDe;

// gamma for the untimed constructors: one token per evaluation cycle,
// which is what (4.3)-(4.6) fix the synchronous ones at.
static void one(unsigned int& n, const int&) {n = 1;}

struct fsm_semantics : ForSyDe::composite
{
    SY::signal<int> sy_i1, sy_o1, sy_i2, sy_o2;
    UT::signal<int> ut_i1, ut_o1, ut_i2, ut_o2, ut_i3, ut_o3, ut_i4, ut_o4;

    SC_CTOR(fsm_semantics)
    {
        std::vector<abst_ext<int>> sy_src;
        std::vector<int>           ut_src;
        for (int i=0; i<8; i++) {sy_src.push_back(abst_ext<int>(i)); ut_src.push_back(i);}

        // ---- SY::moore ------------------------------------------------
        add(new SY::vsource("sy_moore_src", sy_src))(sy_i1);
        auto& sm = add(new SY::moore<int,int,int>("sy_moore",
            [](int& ns, const int& st, const abst_ext<int>&){ns = st+1;},
            [](abst_ext<int>& o, const int& st){o = abst_ext<int>(10*st);}, 0));
        sm.iport1(sy_i1); sm.oport1(sy_o1);
        add(new SY::sink("sy_moore_sink",
            [](const abst_ext<int>& v){std::cout << "SY::moore  " << v << "\n";}))(sy_o1);

        // ---- SY::mealy ------------------------------------------------
        add(new SY::vsource("sy_mealy_src", sy_src))(sy_i2);
        auto& sy = add(new SY::mealy<int,int,int>("sy_mealy",
            [](int& ns, const int& st, const abst_ext<int>&){ns = st+1;},
            [](abst_ext<int>& o, const int& st, const abst_ext<int>& a)
                {o = abst_ext<int>(100*st + unsafe_from_abst_ext(a));}, 0));
        sy.iport1(sy_i2); sy.oport1(sy_o2);
        add(new SY::sink("sy_mealy_sink",
            [](const abst_ext<int>& v){std::cout << "SY::mealy  " << v << "\n";}))(sy_o2);

        // ---- UT::moore ------------------------------------------------
        add(new UT::vsource("ut_moore_src", ut_src))(ut_i1);
        auto& um = add(new UT::moore<int,int,int>("ut_moore", one,
            [](int& ns, const int& st, const std::vector<int>&){ns = st+1;},
            [](std::vector<int>& o, const int& st){o.push_back(10*st);}, 0));
        um.iport1(ut_i1); um.oport1(ut_o1);
        add(new UT::sink("ut_moore_sink",
            [](const int& v){std::cout << "UT::moore  " << v << "\n";}))(ut_o1);

        // ---- UT::mealy ------------------------------------------------
        add(new UT::vsource("ut_mealy_src", ut_src))(ut_i2);
        auto& uy = add(new UT::mealy<int,int,int>("ut_mealy", one,
            [](int& ns, const int& st, const std::vector<int>&){ns = st+1;},
            [](std::vector<int>& o, const int& st, const std::vector<int>& a)
                {o.push_back(100*st + a[0]);}, 0));
        uy.iport1(ut_i2); uy.oport1(ut_o2);
        add(new UT::sink("ut_mealy_sink",
            [](const int& v){std::cout << "UT::mealy  " << v << "\n";}))(ut_o2);

        // ---- UT::scan -------------------------------------------------
        add(new UT::vsource("ut_scan_src", ut_src))(ut_i3);
        auto& us = add(new UT::scan<int,int>("ut_scan", one,
            [](int& ns, const int& st, const std::vector<int>&){ns = st+1;}, 0));
        us.iport1(ut_i3); us.oport1(ut_o3);
        add(new UT::sink("ut_scan_sink",
            [](const int& v){std::cout << "UT::scan   " << v << "\n";}))(ut_o3);

        // ---- UT::scand ------------------------------------------------
        add(new UT::vsource("ut_scand_src", ut_src))(ut_i4);
        auto& ud = add(new UT::scand<int,int>("ut_scand", one,
            [](int& ns, const int& st, const std::vector<int>&){ns = st+1;}, 0));
        ud.iport1(ut_i4); ud.oport1(ut_o4);
        add(new UT::sink("ut_scand_sink",
            [](const int& v){std::cout << "UT::scand  " << v << "\n";}))(ut_o4);
    }
};

int sc_main(int, char*[])
{
    fsm_semantics t("t");
    sc_start(10, sc_core::SC_NS);
    return 0;
}
