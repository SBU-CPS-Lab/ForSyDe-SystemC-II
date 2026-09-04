/**********************************************************************
    * sub.cpp -- the second translation unit                          *
    *                                                                 *
    * Purpose: Half of the multi-translation-unit regression test.    *
    *          See README.md in this directory.                       *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include "sub.hpp"

sub::sub(sc_core::sc_module_name _name) : composite(_name)
{
    add(new ForSyDe::SY::scomb("inc", [](sample& out, const sample& in) {
        out = sample(in.value + 1);
    }))(mid, iport);

    add(new ForSyDe::SY::scomb("dbl", [](sample& out, const sample& in) {
        out = sample(in.value * 2);
    }))(oport, mid);
}
