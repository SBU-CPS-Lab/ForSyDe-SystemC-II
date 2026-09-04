/**********************************************************************
    * mulacc.hpp -- a multiply-accumulate process                     *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a simple sequential processes.        *
    *                                                                 *
    * Usage:   Parallel MulAcc example                                *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/


#ifndef MULACC_HPP
#define MULACC_HPP

#include <forsyde.hpp>
#include "mul.hpp"
#include "add.hpp"

using namespace ForSyDe::SY;

struct mulacc : ForSyDe::composite
{
    SY_in<int>  a, b;
    SY_out<int> result;

    SY2SY<int> addi1, addi2, acci;

    SC_CTOR(mulacc)
    {
        add(new comb2("mul1", mul_func))(addi1, a, b);

        auto& add1 = add(new comb2("add1", add_func));
        add1(acci, addi1, addi2);
        add1.oport1(result);

        add(new delay("accum", abst_ext<int>(0)))(addi2, acci);
    }
};

#endif
