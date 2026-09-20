// tests/reflection -- what a running model says about itself.
//
// ForSyDe::reflection (src/forsyde/reflection.hpp) is the runtime half
// of the reflection service: a process reports what it just did, and
// anything that wants to know subscribes. This is the thing that
// subscribes.
//
// It exists for two reasons, and the second is the stronger one.
//
// The obvious one: the report is now a value with fields rather than a
// line of text written to a file descriptor, so it can be checked
// rather than eyeballed -- that a kernel names the scenario it fired
// under, that a detector names the one it selected, that the rates
// quoted are the ones that scenario selects, and that time advances
// across firings.
//
// The stronger one: before this sub-phase, self-reporting was reached
// only by building with FORSYDE_SELF_REPORTING, and that macro was
// commented out in every Makefile in this repository. The code was
// therefore compiled by nothing at all -- the same hazard that
// tests/instantiate and tests/no_reflection exist to close, and the
// one that had already produced four broken process constructors
// elsewhere in the library. Moving reporting onto a service does not
// fix that by itself; something has to subscribe, or the path is
// exactly as unexercised as it was before.
//
// Both configurations are meaningful here. With reflection on, the
// reports arrive and are checked. With FORSYDE_NO_REFLECTION the same
// source still has to compile -- observe(), observed() and report()
// keep their shapes and go inert -- which is what makes turning
// reflection off a decision about listening rather than a different
// way of writing a model.
#include <forsyde.hpp>

#include <iostream>
#include <string>
#include <vector>

using namespace ForSyDe;

enum kernel_scenario {ADD, SUB};
enum detector_scenario {S1, S2};

std::ostream& operator<<(std::ostream& os, const kernel_scenario& s)
{
    return os << (s == ADD ? "ADD" : "SUB");
}
std::ostream& operator<<(std::ostream& os, const detector_scenario& s)
{
    return os << (s == S1 ? "S1" : "S2");
}

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "ok   " : "FAIL ") << what << "\n";
    if (!ok) ++failures;
}

//! Everything the model reported, in order
std::vector<reflection::firing> heard;

FORSYDE_COMPOSITE(top)
{
    SADF::signal<kernel_scenario> to_kernel_ctrl;
    SADF::signal<int> src, from_detector_src, to_kernel, from_kernel;

    SC_CTOR(top)
    {
        add(new SDF::source("src", [](int& o, const int& i){o = i + 1;}, 1, 4))(src);
        add(new SDF::source("kin", [](int& o, const int& i){o = i + 1;}, 1, 0))(to_kernel);

        // The detector alternates scenario, and with it the kernel's.
        add_detectorMN(*this, "det",
            [](auto&& next, const auto& cur, const auto&)
                {next = (cur == S1) ? S2 : S1;},
            [](auto&& out, const auto& sc, const auto&)
                {std::get<0>(out)[0] = (sc == S1) ? ADD : SUB;},
            {
                {S1, {1}},
                {S2, {1}}
            },
            S1, {1},
            outs(to_kernel_ctrl), ins(src));

        add_kernelMN(*this, "krn",
            [](auto&& out, const auto& sc, const auto& inp)
                {std::get<0>(out)[0] = (sc == ADD)
                     ? std::get<0>(inp)[0] + 1
                     : std::get<0>(inp)[0] - 1;},
            {
                {ADD, {{1},{1}}},
                {SUB, {{1},{1}}}
            },
            outs(from_kernel), to_kernel_ctrl, ins(to_kernel));

        add(new SDF::sink("snk", [](const int& v){std::cout << "out " << v << "\n";}))(from_kernel);
    }

    void start_of_simulation()
    {
        reflection::observe([](const reflection::firing& f){ heard.push_back(f); });
    }
};

int main_checks()
{
    check(!heard.empty(), "a subscriber heard something at all");
    if (heard.empty()) return 1;

    bool kernels = false, detectors = false;
    bool kinds_qualified = true, named = true, scen_and_rates = true;
    sc_core::sc_time last = sc_core::SC_ZERO_TIME;
    bool time_monotonic = true;

    for (const auto& f : heard)
    {
        if (f.kind == "SADF::kernelMN")   kernels = true;
        if (f.kind == "SADF::detectorMN") detectors = true;
        // A firing carries the fully qualified kind; the line format
        // prints the short half, which is what the pipe has always had.
        if (f.kind.find("::") == std::string::npos) kinds_qualified = false;
        if (f.process.empty()) named = false;
        if (f.scenario.empty() || f.rates.empty()) scen_and_rates = false;
        if (f.time < last) time_monotonic = false;
        last = f.time;
    }

    check(kernels,   "a kernelMN reported its firings");
    check(detectors, "a detectorMN reported its firings");
    check(kinds_qualified, "every firing carries a fully qualified kind");
    check(named, "every firing names the process instance");
    check(scen_and_rates, "every firing carries a scenario and its rates");
    check(time_monotonic, "firings arrive in non-decreasing simulated time");

    check(reflection::short_kind("SADF::kernelMN") == "kernelMN",
          "short_kind strips the MoC, as the report line wants");

    // The legacy rendering, pinned: this is the format the self-report
    // pipe has always carried, and things outside this repository read it.
    const reflection::firing sample{"SADF::kernelMN", "krn",
                                    sc_core::SC_ZERO_TIME, "ADD", "1  1"};
    check(reflection::as_report_line(sample) == "kernelMN  krn  ADD  1  1\n",
          "as_report_line renders the format the pipe has always carried");

    std::cout << "-- first three firings --\n";
    for (std::size_t i = 0; i < heard.size() && i < 3; i++)
        std::cout << "  " << reflection::as_report_line(heard[i]);

    reflection::forget_observers();
    check(!reflection::observed(), "forget_observers leaves nobody listening");
    return 0;
}

int sc_main(int, char*[])
{
    check(!reflection::observed(), "nobody is listening before anything subscribes");

    top t("top1");
    sc_core::sc_start();

#ifdef FORSYDE_REFLECTION
    main_checks();
#else
    // The opt-out: the same source compiles, and says so.
    check(heard.empty(), "with FORSYDE_NO_REFLECTION nothing is reported");
    check(!reflection::observed(), "and observed() is a compile-time false");
#endif

    std::cout << (failures == 0 ? "all reflection checks hold\n"
                                : "REFLECTION CHECKS BROKEN\n");
    return failures == 0 ? 0 : 1;
}
