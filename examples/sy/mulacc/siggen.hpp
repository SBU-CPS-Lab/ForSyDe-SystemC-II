/**********************************************************************
    * siggen.hpp -- a ramp process                                    *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a simple source process.              *
    *                                                                 *
    * Usage:   MulAcc example                                         *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/


#ifndef SIGGEN_HPP
#define SIGGEN_HPP

#include <forsyde.hpp>

using namespace ForSyDe;


void siggen_func(int& out1, const int& inp1)
{
#pragma ForSyDe begin siggen_func
    out1 = inp1 + 1;
#pragma ForSyDe end
}

#endif
