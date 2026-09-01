/**********************************************************************           
    * forsyde.hpp -- the main header file used to import the SystemC  *
    *          map of the ForSyDe library                             *
    *                                                                 *
    * Author:  Hosein Attarzadeh (shan2@kth.se)                       *
    *                                                                 *
    * Purpose: Exporting library definitions of the ForSyDe-SystemC   *
    *                                                                 *
    * Usage:   The user only includes this header file to access the  *
    *          library definitions                                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef FORSYDE_HPP
#define FORSYDE_HPP

/*! \mainpage ForSyDe-SystemC API Documentation
 *
 * \section intro_sec Introduction
 *
 * This documentation is supplied as the API-level guideline for using the
 * ForSyDe-SystemC library.
 * It is generated automatically from the library source code and is updated
 * with the library itself.
 *
 * \subsection scope_purpose Scope and Purpose
 *
 * This document provides the designer with information about the constructs
 * provided by the library and how they can be used practically.
 * It does NOT describe the general modeling concepts and formalisms behind
 * ForSyDe.
 *
 * It is suggested to consult the ForSyDe webpage and Wiki page before starting
 * modeling with this library.
 * More information and tutorials are available there.
 *
 * \section using_doc Using the Documentation
 *
 * ForSyDe-SystemC library mainly includes constructs to build processes in
 * different Models of Computation (MoCs) and connect them using domain
 * interfaces.
 *
 * \subsection namespaces_mocs Namespaces and MoCs
 *
 * Everything provided by the ForSyDe-SystemC library is a member of the
 * ForSyDe namespace.
 * In addition, there is a separate sub-namespace dedicated to each MoC which
 * includes process constructors and other constructs related to that specific
 * MoC.
 * There are different MoCs suported in ForSyDe-SystemC:
 *
 *     - Synchronous MoC in ForSyDe::SY
 *     - Untimed MoC in ForSyDe::UT and its Synchronous Dataflow variant in ForSyDe::SDF
 *     - Two timed MoCs Distributed Discrete-Event in ForSyDe::DDE and Discrete-Time in ForSyDe::DT
 *     - Continuous-Time MoC in ForSyDe::CT
 */

/*! \file forsyde.hpp
 * \brief Exports the library definitions for the ForSyDe-SystemC
 * 
 *  The user only includes this header file. Definitions in the other
 * files of the library are re-exported from here.
 */

// include utility libraries
#include "forsyde/prettyprint.hpp"

// include the main SystemC library
#include <systemc>

#ifdef FORSYDE_INTROSPECTION
#include "forsyde/types.hpp"
#endif

// include the abstract semantics
#include "forsyde/abssemantics.hpp"

// include different MoCs
#include "forsyde/ut_moc.hpp"

#include "forsyde/sy_moc.hpp"
#include "forsyde/sy_lib.hpp"

#include "forsyde/sdf_moc.hpp"

#include "forsyde/sadf_moc.hpp"

#include "forsyde/dde_moc.hpp"

#include "forsyde/dt_moc.hpp"

#include "forsyde/ct_moc.hpp"
#include "forsyde/ct_lib.hpp"

// include MoC interfaces
#include "forsyde/mis.hpp"
#include "forsyde/mi_helpers.hpp"

#include "forsyde/adaptivity.hpp"

#ifdef FORSYDE_INTROSPECTION
#include "forsyde/xml.hpp"
#endif

// Each optional backend below has its own dependency (an MPI
// implementation; libmigdb and POSIX sockets; the FMI 2.0 headers and
// an FMU; a ROS distribution) and its own independent switch, so one
// can be requested without pulling in the others' build requirements.
// FORSYDE_PARALLEL_SIM and FORSYDE_COSIMULATION_WRAPPERS are kept as
// compatibility aliases: defining either still turns on exactly what it
// always did, for anyone building against this library from the
// command line they already have.

#if defined(FORSYDE_PARALLEL_SIM) && !defined(FORSYDE_WITH_MPI)
#define FORSYDE_WITH_MPI
#endif

#if defined(FORSYDE_COSIMULATION_WRAPPERS)
#ifndef FORSYDE_WITH_GDB
#define FORSYDE_WITH_GDB
#endif
#ifndef FORSYDE_WITH_FMI
#define FORSYDE_WITH_FMI
#endif
#endif

#ifdef FORSYDE_WITH_MPI
#include "forsyde/parallel_sim_helpers.hpp"
#endif

#ifdef FORSYDE_WITH_GDB
#include "forsyde/sy_wrappers.hpp"
#endif

#ifdef FORSYDE_WITH_FMI
// Builds the vendored FMU-description XML parser (D15) in the mode it
// offers precisely for embedding outside its origin toolchain, rather
// than pulling in that toolchain's own logging/globals headers.
#ifndef STANDALONE_XML_PARSER
#define STANDALONE_XML_PARSER
#endif
#include "forsyde/ct_wrappers.hpp"
#endif

// FORSYDE_WITH_ROS is reserved for the ROS co-simulation wrapper
// (Phase 1b); there is nothing in this tree for it to guard yet.


#endif
