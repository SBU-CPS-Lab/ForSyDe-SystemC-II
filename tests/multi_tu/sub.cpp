/**********************************************************************
    * sub.cpp -- the second translation unit                          *
    *                                                                 *
    * Purpose: Half of the multi-translation-unit regression test.    *
    *          See README.md in this directory.                       *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include "sub.hpp"

sub::sub(sc_core::sc_module_name _name) : sc_module(_name)
{
    ForSyDe::SY::make_scomb("inc", [](sample& out, const sample& in) {
        out = sample(in.value + 1);
    }, mid, iport);

    ForSyDe::SY::make_scomb("dbl", [](sample& out, const sample& in) {
        out = sample(in.value * 2);
    }, oport, mid);
}
