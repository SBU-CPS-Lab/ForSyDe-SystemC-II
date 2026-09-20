/**********************************************************************
    * reflection.hpp -- what a running model says about itself         *
    *                                                                  *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)              *
    *                                                                  *
    * Purpose: A place for a process to report what it just did, and    *
    *          for anything at all to listen                            *
    *                                                                  *
    * Usage:   This file is included automatically                     *
    *                                                                  *
    * License: BSD3                                                    *
    *******************************************************************/

#ifndef FORSYDE_REFLECTION_HPP
#define FORSYDE_REFLECTION_HPP

/*! \file reflection.hpp
 * \brief Runtime reporting, as a service rather than a constructor argument
 *
 * ir.hpp answers "what does this model look like"; this answers "what is
 * it doing". The two together are what the plan calls the reflection
 * service: the structure, and the events over it.
 *
 * The shape of this file is a direct response to D10. Self-reporting
 * used to work by handing a process a FILE** at construction and
 * guarding the whole mechanism with FORSYDE_SELF_REPORTING, which went
 * wrong in three separate ways:
 *
 *   * The destination was welded into the model. A process had to be
 *     *constructed* differently to be observed, so a model that might
 *     want reporting had to be written twice -- and examples/sadf/encdec
 *     was, nine #ifdef'd call sites of it, the same processes built
 *     twice over with one extra argument.
 *   * It was SADF-only. Two classes could report; nothing else could,
 *     and nothing else could be made to without repeating the whole
 *     arrangement.
 *   * The macro was commented out in every Makefile in the tree, so the
 *     reporting path was compiled by nothing at all -- the same
 *     never-instantiated hazard that tests/instantiate and
 *     tests/no_reflection exist to close.
 *
 * Here a process reports unconditionally and whoever wants the reports
 * asks for them. Nothing is passed at construction, so a model reads the
 * same whether or not anyone is listening; nothing is MoC-specific, so
 * any process can report; and the path is always compiled, so it cannot
 * rot. When no one has subscribed, a report costs one comparison -- the
 * process checks observed() before it renders anything.
 */

#include "config.hpp"

#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <systemc>

namespace ForSyDe
{

namespace reflection
{

//! One thing a process reported while it was running
/*! scenario and rates are rendered by the process rather than carried
 * as types, which is what keeps this header free of every MoC's value
 * types -- a control token can be any user type at all, and a rate can
 * be a scalar, an array or a tuple of them depending on the
 * constructor. An observer that wants structure rather than text has
 * the IR for that (ir.hpp); this is the running commentary, and its job
 * is to say what happened in terms a reader can print.
 *
 * Both are empty for a process that has no such notion, which is most
 * of them: any process may report, and a plain comb has neither a
 * scenario nor a rate table to name.
 */
struct firing
{
    std::string kind;       //!< the reporting process's forsyde_kind()
    std::string process;    //!< its instance name
    sc_core::sc_time time;  //!< when it fired
    std::string scenario;   //!< the control value it fired under, if it has one
    std::string rates;      //!< the rates that value selected, if it has any
};

//! Anything that wants to be told
using observer = std::function<void(const firing&)>;

#ifdef FORSYDE_REFLECTION

namespace detail
{
//! The subscriber list
/*! A function-local static rather than a namespace-scope object: this
 * is a header-only library, and a namespace-scope definition here would
 * be a separate object in every translation unit that included it (D1,
 * the same defect types.hpp had). One function, one object, however
 * many TUs.
 */
inline std::vector<observer>& observers()
{
    static std::vector<observer> obs;
    return obs;
}
} // namespace detail

//! Ask to be told about every firing every process reports
inline void observe(observer obs)
{
    detail::observers().push_back(std::move(obs));
}

//! Is anyone listening?
/*! Checked by a process before it renders anything, so that reporting
 * costs one comparison in a model nobody is observing.
 */
inline bool observed()
{
    return !detail::observers().empty();
}

//! Tell whoever is listening
inline void report(const firing& f)
{
    for (const auto& obs : detail::observers()) obs(f);
}

//! Forget every subscriber
/*! For a test that installs one, and for a self-model that has finished
 * with the network it was watching.
 */
inline void forget_observers()
{
    detail::observers().clear();
}

#else // FORSYDE_NO_REFLECTION

// The opt-out leaves the whole vocabulary in place and makes it inert,
// deliberately: a process reports through the same three lines either
// way, so turning reflection off cannot change what a model *says*,
// only whether anything listens. observed() being a compile-time false
// is what lets the optimiser drop the rendering with it.
inline void observe(observer) {}
constexpr bool observed() {return false;}
inline void report(const firing&) {}
inline void forget_observers() {}

#endif

//! The process-constructor half of a kind, which is what a report names
/*! "SADF::kernelMN" -> "kernelMN". A firing carries the whole kind,
 * because that is what identifies the constructor unambiguously; the
 * line format below prints the short half, which is what it has always
 * printed.
 */
inline std::string short_kind(const std::string& kind)
{
    const std::string::size_type last = kind.rfind(':');
    return last == std::string::npos ? kind : kind.substr(last + 1);
}

//! One firing, in the format the self-report pipe has always carried
/*! kind, instance name, scenario and rates, two spaces between each.
 * Kept exactly as it was: the pipe is read by things outside this
 * repository, and changing the graph's *representation* is not what
 * this sub-phase is for.
 */
inline std::string as_report_line(const firing& f)
{
    return short_kind(f.kind) + "  " + f.process + "  " + f.scenario
         + "  " + f.rates + "\n";
}

//! An observer that writes that format to a FILE*, as the pipe did
/*! Takes FILE** rather than FILE* because a model typically opens its
 * pipe at start_of_simulation, after the network is built -- the
 * indirection that used to be forced on every *constructor* is now
 * confined to the one place that actually needs it, and only for as
 * long as a model chooses to open its pipe late.
 */
inline observer to_pipe(FILE** pipe)
{
    return [pipe](const firing& f)
    {
        if (!pipe || !*pipe) return;
        std::fputs(as_report_line(f).c_str(), *pipe);
        std::fflush(*pipe);
    };
}

} // namespace reflection

} // namespace ForSyDe

#endif
