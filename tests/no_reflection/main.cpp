// tests/no_reflection -- the opt-out, compiled.
//
// Reflection is on by default (src/forsyde/config.hpp), and
// FORSYDE_NO_REFLECTION turns it off for a model that wants the
// smaller object file and the faster rebuild and has no use for the
// structural record. This directory is the only thing in the tree that
// compiles that configuration.
//
// It exists because of the pattern every sub-phase of Phase 2 turned
// up: a code path nothing instantiates is not "probably fine", it is
// unverified source text, and four separate classes were broken
// precisely because no example named them. An opt-out is a whole build
// configuration in that category -- it compiles away the base class's
// bindInfo(), every arg_vec push, and the recording operator()
// overloads on every port -- so without something building it, the
// first person to try it would be the one who finds out.
//
// Its Makefile defines FORSYDE_NO_REFLECTION, which makes the harness's
// "on" row the opt-out build. The harness's "off" row overrides CFLAGS
// wholesale and so drops the macro, giving an ordinary build with
// reflection on; the two goldens are the two sides.
//
// The model is deliberately a spread rather than a single process: an
// SY chain, an SDF actor with a rate, a UT process and a DT one, so
// that the ports, signals and bindInfo of four MoCs are instantiated
// without reflection rather than one.
#include <forsyde.hpp>

#include <iostream>

using namespace ForSyDe;

void sy_scale(int& out, const int& inp) {out = inp * 3;}
void sy_report(const int& inp) {std::cout << "sy " << inp << "\n";}

void sdf_sum(std::vector<int>& out, const std::vector<int>& inp)
{
    out[0] = inp[0] + inp[1];
}
void sdf_report(const int& inp) {std::cout << "sdf " << inp << "\n";}

void ut_pass(std::vector<int>& out, const std::vector<int>& inp) {out = inp;}
void ut_report(const int& inp) {std::cout << "ut " << inp << "\n";}

void dt_report(const abst_ext<int>& inp) {std::cout << "dt " << inp << "\n";}

FORSYDE_COMPOSITE(top)
{
    SY::signal<int>  sy_a, sy_b;
    SDF::signal<int> sdf_a, sdf_b;
    UT::signal<int>  ut_a, ut_b;
    DT::signal<int>  dt_a, dt_b;

    SC_CTOR(top)
    {
        add(new SY::sconstant("sy_src", 7, 3))(sy_a);
        add(new SY::scomb("sy_scale", sy_scale))(sy_b, sy_a);
        add(new SY::ssink("sy_snk", sy_report))(sy_b);

        add(new SDF::source("sdf_src", [](int& o, const int& i){o = i + 1;}, 1, 6))(sdf_a);
        add(new SDF::comb("sdf_sum", sdf_sum, 1, 2))(sdf_b, sdf_a);
        add(new SDF::sink("sdf_snk", sdf_report))(sdf_b);

        add(new UT::source("ut_src", [](int& o, const int& i){o = i + 1;}, 1, 3))(ut_a);
        add(new UT::comb("ut_pass", ut_pass, 1))(ut_b, ut_a);
        add(new UT::sink("ut_snk", ut_report))(ut_b);

        add(new DT::vsource<int>("dt_src", {{0,1}, {1,2}, {2,3}}))(dt_a);
        add(new DT::delay("dt_del", abst_ext<int>(1)))(dt_b, dt_a);
        add(new DT::sink("dt_snk", dt_report))(dt_b);
    }
};

int sc_main(int, char*[])
{
#ifdef FORSYDE_REFLECTION
    std::cout << "reflection: on\n";
#else
    std::cout << "reflection: off (FORSYDE_NO_REFLECTION)\n";
#endif

    top t("top1");
    sc_core::sc_start();
    return 0;
}
