/**********************************************************************
    * config.hpp -- what this build of the library compiles in         *
    *                                                                  *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)              *
    *                                                                  *
    * Purpose: One place for the feature switches, and the reasoning    *
    *          behind each one's default                                *
    *                                                                  *
    * Usage:   This file is included automatically                     *
    *                                                                  *
    * License: BSD3                                                    *
    *******************************************************************/

#ifndef FORSYDE_CONFIG_HPP
#define FORSYDE_CONFIG_HPP

/*! \file config.hpp
 * \brief The feature switches, and what each one costs
 */

//! FORSYDE_REFLECTION -- the record a model keeps of its own structure
/*! On unless FORSYDE_NO_REFLECTION is defined.
 *
 * This is what makes a model's structure readable from inside the
 * program: the binding operators that record which channel a port was
 * bound to, the per-process list of bound channels and constructor
 * arguments, and the token-type and MoC names a port and a signal can
 * be asked for. ir::build reads exactly these, so ForSyDe::ir::model --
 * and every backend over it, and any self-model or monitor that wants
 * to know what the network looks like -- exists precisely when this is
 * defined.
 *
 * It used to be spelled FORSYDE_INTROSPECTION and it used to be off by
 * default, which made the structure a property of *how the model was
 * built* rather than of the library. Two things followed from that, and
 * both were bad. A model built the ordinary way had no structure to
 * read at all, so anything wanting to reason about the network had to
 * demand a particular build configuration. And the same macro that
 * decided whether the XML export existed also decided whether the data
 * it exported was collected, so the two could not be separated -- you
 * could not have the graph without also writing a file, and you could
 * not stop writing the file without losing the graph.
 *
 * They are separate switches now: FORSYDE_REFLECTION collects, and
 * FORSYDE_INTROSPECTION (below) is the XML backend over what was
 * collected.
 *
 * On by default, with an opt-out rather than always-on, because the
 * cost is real if not large. Measured on this tree with every other
 * flag held equal -- g++ 15.2.0, -O0 -std=c++17, SystemC 3.0.2 --
 * comparing a build with the recording against one without:
 *
 *      tests/moc_binding    compile +7.0%    binary +17.9%
 *      tests/instantiate    compile +13.8%   binary +11.9%
 *
 * and, on a running model, no measurable change in wall time and under
 * 5% in peak RSS (sy/mlpnn, sy/sorter, sdf/vad), which is what you
 * would expect: the recording happens during elaboration, not during
 * simulation. The compile-time and code-size costs are structural --
 * the recording gives every port and signal *instantiation* a vtable
 * and two more virtual functions -- so they scale with how many
 * distinct process types a model names, which is why the figure is
 * worst for the test that deliberately names all of them.
 *
 * So: a model that wants the smallest possible object file, or the
 * fastest possible rebuild, and has no use for the structure, can say
 * so. Nothing else should need to.
 */
#ifndef FORSYDE_NO_REFLECTION
#define FORSYDE_REFLECTION
#endif

//! FORSYDE_INTROSPECTION -- the XML export of what reflection collected
/*! Off unless defined. It is a *backend*: it reads the reflection data
 * and writes the process network out as XML at the start of
 * simulation. It cannot mean anything without the data.
 */
#if defined(FORSYDE_INTROSPECTION) && !defined(FORSYDE_REFLECTION)
#error "FORSYDE_INTROSPECTION is the XML export of the reflection data, so it cannot be combined with FORSYDE_NO_REFLECTION. Drop one of the two."
#endif

#endif
