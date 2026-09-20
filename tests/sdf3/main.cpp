// tests/sdf3 -- flattening a real model into an SDF3 graph, checked
// against the real tool.
//
// sdf3::flatten (src/forsyde/sdf3.hpp) is the second view over
// ForSyDe::ir::model, alongside the XML backend: where XML preserves
// hierarchy because that is the format's own shape, SDF3's "sdf"
// application graph has no hierarchy concept at all, so flatten() has
// two real jobs XML's rewrite never had to do -- eliminate every
// composite boundary, and turn any readers(...) fan-out into an
// explicit broadcast actor, since a port in SDF3's model is one end of
// exactly one channel.
//
// This model is built to exercise both, plus the two virtual hooks
// that had to be added to reach them at all (ForSyDe::process::rates()
// and ::initial_tokens(), abssemantics.hpp): a composite whose one
// SDF::comb both leaves the composite and feeds back into an
// SDF::delayn closing a loop (fan-out through a boundary, and a
// non-trivial initial-token count); a plain SDF::comb with different
// input and output rates; and an SDF::source/SDF::sink pair, which
// declare no rate at all and so exercise flatten()'s documented default
// of 1 for the eight SDF classes that are always exactly one token per
// port per firing.
//
// The strongest claim this test makes is not "the XML looks like the
// SDF3 examples" -- it is that the file this writes was handed to the
// real sdf3analysis-sdf binary and came back consistent, deadlock-free,
// with a repetition vector, which is recorded in the golden precisely
// so a future run's output can be handed to the same binary again. See
// README.md for exactly how that was done and how to redo it.
#include <forsyde.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <utility>

using namespace ForSyDe;

void up(std::vector<int>& o, const std::vector<int>& i) {o[0] = i[0]; o[1] = i[0]+1;}
void avg(std::tuple<std::vector<int>>& out, const std::tuple<std::vector<int>,std::vector<int>>& in)
{
    // Both inputs at rate 1: fb_out is SDF::delayn's own output, and a
    // delayn -- any n -- writes exactly one token per firing in
    // steady state, whatever n says about how many it prepends before
    // the first one (ForSyDe::process::initial_tokens()). Declaring
    // fb_out at any other rate here is not a modelling choice, it is
    // a wrong number, and sdf3analysis-sdf's "not consistent" is
    // exactly how a wrong number in this specific spot gets caught --
    // confirmed by deliberately writing 2 here first and watching the
    // real tool reject the result.
    const auto& i1 = std::get<0>(in);
    const auto& i2 = std::get<1>(in);
    std::get<0>(out)[0] = (i1[0]+i2[0])/2;
}

//! A composite whose one actor both leaves it and feeds its own loop
/*! outs(readers(oport1, fb_in)) is exactly toysdfMN's compAvg shape:
 * one SDF::combMN output read by two downstream signals, one of them
 * the composite's own boundary port. Flattened, averager1's output has
 * to become the source of exactly one channel (into a synthesized
 * fanout actor), not two.
 */
FORSYDE_COMPOSITE(loopy)
{
    SDF::in_port<int> iport1;
    SDF::out_port<int> oport1;
    SDF::signal<int> fb_in, fb_out;

    SC_CTOR(loopy)
    {
        add_combMN(*this, "averager1", avg, {1}, {1,1},
            outs(readers(oport1, fb_in)), ins(iport1, fb_out));
        add(new SDF::delayn("state", 0, 2))(fb_out, fb_in);
    }
};

FORSYDE_COMPOSITE(top)
{
    SDF::signal<int> src, up1, res;

    SC_CTOR(top)
    {
        add(new SDF::source("src", [](int& o, const int& i){o = i+1;}, 1, 20))(src);
        add(new SDF::comb("upsample", up, 2, 1))(up1, src);  // out rate 2, in rate 1
        auto& lp = add(new loopy("lp1"));
        lp.iport1(up1);
        lp.oport1(res);
        add(new SDF::sink("snk", [](const int& v){std::cout << "res " << v << "\n";}))(res);
    }
};

//! A model with one non-SDF leaf, to check flatten()'s refusal path
FORSYDE_COMPOSITE(mixed)
{
    SY::signal<int> a;
    SC_CTOR(mixed)
    {
        add(new SY::sconstant("c", 1, 1))(a);
        add(new SY::ssink("s", [](const abst_ext<int>&){}))(a);
    }
};

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "ok   " : "FAIL ") << what << "\n";
    if (!ok) ++failures;
}

int sc_main(int, char*[])
{
    // Both top-level modules are constructed before the one sc_start()
    // -- a new sc_module cannot be constructed once simulation is under
    // way (SystemC rejects it outright), so the refusal-path model
    // below has to exist from the start like any other, even though
    // nothing about it is exercised until after sc_start() returns.
    top t("top1");
    mixed bad("bad1");
    sc_core::sc_start();

    const auto m = ir::build(&t);
    const auto g = sdf3::flatten(m);

    // Every output port is the source of at most one channel -- the
    // property insert_fanouts exists to establish.
    std::map<std::pair<std::string,std::string>, int> out_degree;
    for (const auto& c : g.channels) out_degree[{c.src_actor, c.src_port}]++;
    bool single_source = true;
    for (const auto& [key, n] : out_degree) if (n != 1) single_source = false;
    check(single_source, "every channel source is used by exactly one channel");

    bool fanout_present = false;
    for (const auto& a : g.actors) if (a.type == "fanout") fanout_present = true;
    check(fanout_present, "the composite's readers(...) fan-out became an explicit actor");

    bool composite_flattened = true;
    for (const auto& a : g.actors) if (a.name.find("lp1__") == std::string::npos && a.type != "fanout"
                                        && a.name != "src" && a.name != "upsample" && a.name != "snk")
        composite_flattened = false;
    check(composite_flattened, "every actor is a top-level leaf or qualified lp1__<leaf>");

    bool default_rate_applied = false;
    for (const auto& a : g.actors)
        if (a.name == "src")
            for (const auto& p : a.ports)
                if (p.rate == 1) default_rate_applied = true;
    check(default_rate_applied, "SDF::source's undeclared rate defaulted to 1");

    bool initial_tokens_found = false;
    for (const auto& c : g.channels)
        if (c.initial_tokens == 2) initial_tokens_found = true;
    check(initial_tokens_found, "SDF::delayn(0,2)'s initial_tokens() reached the flattened channel");

    // Refusal path: a model with a non-SDF leaf must not silently
    // produce a graph that looks like it means something.
    bool threw = false;
    std::string what;
    try
    {
        const auto bm = ir::build(&bad);
        (void)sdf3::flatten(bm);
    }
    catch (const std::runtime_error& e)
    {
        threw = true;
        what = e.what();
    }
    check(threw && what.find("not SDF") != std::string::npos,
          "flatten() refuses a non-SDF leaf and names it");

    std::ostringstream xml;
    sdf3::write(g, xml);
    std::cout << "-- " << g.actors.size() << " actors, " << g.channels.size() << " channels --\n";
    std::cout << xml.str();

    std::filesystem::create_directories("gen");
    std::ofstream out("gen/top.sdf3.xml");
    out << xml.str();

    std::cout << (failures == 0 ? "all sdf3 checks hold\n" : "SDF3 CHECKS BROKEN\n");
    return failures == 0 ? 0 : 1;
}
