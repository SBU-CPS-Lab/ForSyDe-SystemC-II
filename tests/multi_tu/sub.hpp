/**********************************************************************
    * sub.hpp -- a subsystem whose constructor lives in its own TU    *
    *                                                                 *
    * Purpose: Half of the multi-translation-unit regression test.    *
    *          See README.md in this directory.                       *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef MULTI_TU_SUB_HPP
#define MULTI_TU_SUB_HPP

#include <forsyde.hpp>

//! A user-defined token type, registered for introspection.
/*! Registering a type is what triggered the D1 link failure, so the test
 * has to do it -- and has to do it from a header included by both
 * translation units, which is how a real multi-file model would.
 */
struct sample
{
    int value;
    sample() : value(0) {}
    sample(int v) : value(v) {}
    bool operator==(const sample& rs) const {return value == rs.value;}
};

inline std::ostream& operator<< (std::ostream& os, const sample& s)
{
    os << s.value;
    return os;
}

#ifdef FORSYDE_INTROSPECTION
DEFINE_TYPE(sample);
#endif

//! Increments, then doubles. Constructor defined in sub.cpp, not here.
SC_MODULE(sub)
{
    ForSyDe::SY::in_port<sample>  iport;
    ForSyDe::SY::out_port<sample> oport;

    ForSyDe::SY::signal<sample> mid;

    SC_CTOR(sub);
};

#endif
