/**********************************************************************
    * ctadd.hpp -- an adder process for the CT MoC                    *
    *                                                                 *
    * Authors: Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a heterogeneous system.               *
    *                                                                 *
    * Usage:   The digital filter example                             *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/


#ifndef CTADD_HPP
#define CTADD_HPP

#include <forsyde.hpp>

using namespace ForSyDe;
using namespace ForSyDe::CT;

void ctadd_func(CTTYPE& out1, const CTTYPE& inp1, const CTTYPE& inp2)
{
#pragma ForSyDe begin ctadd_func
    out1 = inp1 + inp2;
#pragma ForSyDe end
}

#endif
