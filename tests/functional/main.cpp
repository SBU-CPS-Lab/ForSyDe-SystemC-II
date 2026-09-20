// tests/functional -- the applicative surface, and the one claim that
// matters about it.
//
// ForSyDe::fn (src/forsyde/functional.hpp) lets a network be written as
// composition on signal handles rather than as declare-then-bind:
//
//     auto s3 = (fn::SY::comb(f) | fn::SY::comb(g))(s1);
//
// The roadmap's word for this is "a thin front end that calls the
// explicit one", and thin is a testable claim, not a description. So
// this test builds the *same* network twice -- once each way -- and
// checks that the two IRs are identical node for node, port for port
// and channel for channel. If the functional surface ever becomes a
// second implementation rather than a front end, that comparison is
// what says so.
//
// It also covers the two things the surface adds that the explicit one
// has no equivalent of, and which therefore have no other check:
// automatic instance naming, and the declare()/into() pair that closes
// a feedback loop -- including that forgetting to write a declared
// signal is an error at the end of the description rather than a
// deadlock at run time.
#include <forsyde.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace ForSyDe;

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "ok   " : "FAIL ") << what << "\n";
    if (!ok) ++failures;
}

// The two functions the pipeline is built from. Written as plain
// functions so both surfaces get to pass literally the same thing.
void add_one(abst_ext<int>& out, const abst_ext<int>& in)
{
    out = abst_ext<int>(unsafe_from_abst_ext(in) + 1);
}

void times_two(abst_ext<int>& out, const abst_ext<int>& in)
{
    out = abst_ext<int>(unsafe_from_abst_ext(in) * 2);
}

void report(const abst_ext<int>& v)
{
    std::cout << "  out " << unsafe_from_abst_ext(v) << "\n";
}

//! The network, written the way models are written today
FORSYDE_COMPOSITE(explicit_top)
{
    SY::signal<int> s1, s2, s3;

    SC_CTOR(explicit_top)
    {
        add(new SY::sconstant("sconstant1", 1, 4))(s1);
        add(new SY::comb("comb1", add_one))(s2, s1);
        add(new SY::comb("comb2", times_two))(s3, s2);
        add(new SY::ssink("ssink1", report))(s3);
    }
};

//! The same network, written as composition
/*! The instance names are not spelled out and come out the same,
 * because the surface names a process by its constructor and a serial
 * number -- which is the convention the composite above was written by
 * hand to follow.
 */
FORSYDE_COMPOSITE(functional_top)
{
    SC_CTOR(functional_top)
    {
        fn::network net(*this);

        auto s1 = fn::SY::constant(1, 4)(net);
        auto s3 = (fn::SY::comb(add_one) | fn::SY::comb(times_two))(s1);
                  fn::SY::sink(report)(s3);
    }
};

//! A feedback loop, which is what declare() and into() exist for
/*! An accumulator: each output is the previous output plus the input.
 * The back edge cannot be written applicatively without laziness, so
 * it is declared and then assigned, and the assignment is checked.
 */
FORSYDE_COMPOSITE(feedback_top)
{
    SC_CTOR(feedback_top)
    {
        fn::network net(*this);

        auto in = fn::SY::constant(1, 6)(net);
        auto fb = net.declare<SY::signal<int>>();

        // The accumulator prints from inside its own function rather
        // than through a sink, because `sum` already has a reader --
        // the delay -- and this surface has no fan-out yet: an sc_fifo
        // takes exactly one reader, and the explicit surface's answer
        // (readers(...)) is a decision made by the *producer*, so it
        // cannot be recovered from a handle that has been used twice.
        // See the note in functional.hpp.
        auto sum = fn::SY::comb2(
            [](abst_ext<int>& o, const abst_ext<int>& a, const abst_ext<int>& b)
            {
                const int acc = unsafe_from_abst_ext(a) + unsafe_from_abst_ext(b);
                std::cout << "  acc " << acc << "\n";
                o = abst_ext<int>(acc);
            })(in, fb);

        fn::SY::delay(0).into(fb)(sum);
    }
};

std::string unsatisfied_msg;

//! A composite that declares a back edge and then forgets to write it
/*! The failure has to happen while a composite is being *constructed*,
 * because that is when a network exists and when a signal may legally
 * be created at all -- SystemC rejects one built after elaboration.
 * check() is called here so the diagnostic can be caught and inspected
 * rather than escaping a destructor.
 */
FORSYDE_COMPOSITE(forgetful_top)
{
    SC_CTOR(forgetful_top)
    {
        fn::network net(*this);
        auto dangling = net.declare<SY::signal<int>>();
        (void)dangling;

        try {net.check();}
        catch (const std::runtime_error& ex) {unsatisfied_msg = ex.what();}

        // Give it a producer after all, so this composite is a legal
        // model and the run can continue past it.
        fn::SY::delay(0).into(dangling)(fn::SY::constant(7, 2)(net));
        fn::SY::sink([](const abst_ext<int>&){})(dangling);
    }
};

//! Renders a network as text, so two of them can be compared as one string
std::string render(const ir::model& m, std::size_t n)
{
    const ir::network& net = m.networks[n];
    std::ostringstream os;
    for (const auto& nd : net.nodes)
    {
        os << "node " << nd.name << " " << nd.pc_moc << "::" << nd.pc_name << "\n";
        for (const auto& p : nd.ports)
            os << "  port " << p.name << " " << p.moc << " " << p.type
               << (p.dir == ir::direction::in ? " in" : " out") << "\n";
        for (const auto& pa : nd.params)
            os << "  param " << pa.name << " = " << pa.value << "\n";
    }
    for (const auto& c : net.channels)
        os << "chan " << c.moc << " " << c.type << " "
           << c.source << "." << c.source_port << " -> "
           << c.target << "." << c.target_port << "\n";
    return os.str();
}

int sc_main(int, char*[])
{
    explicit_top e("top1");
    functional_top f("top1_fn");
    feedback_top fb("fbtop1");
    forgetful_top fg("fgtop1");
    sc_core::sc_start();

    const std::string a = render(ir::build(&e), 0);
    const std::string b = render(ir::build(&f), 0);

    check(!a.empty(), "the explicit model produced an IR at all");
    check(a == b, "both surfaces describe exactly the same network");
    if (a != b)
    {
        std::cout << "--- explicit ---\n" << a;
        std::cout << "--- functional ---\n" << b;
    }

    // Naming: not spelled out in the functional model, yet identical.
    check(a.find("node comb1 SY::comb") != std::string::npos &&
          a.find("node comb2 SY::comb") != std::string::npos,
          "a process constructor names its instances base1, base2, ...");

    // Feedback: the loop closed, and the delay writes the declared signal.
    const ir::model fm = ir::build(&fb);
    bool delay_present = false;
    for (const auto& nd : fm.networks[0].nodes)
        if (nd.pc_name == "sdelay") delay_present = true;
    check(delay_present, "into() placed the delay that closes the loop");
    check(fm.networks[0].channels.size() == 3,
          "the feedback network has the three signals the loop needs");

    check(unsatisfied_msg.find("nothing ever writes it") != std::string::npos,
          "a declared signal with no producer is reported, not left to deadlock");

    std::cout << (failures == 0 ? "all functional checks hold\n"
                                : "FUNCTIONAL CHECKS BROKEN\n");
    return failures == 0 ? 0 : 1;
}
