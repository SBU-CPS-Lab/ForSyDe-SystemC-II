/**********************************************************************
    * mulacc.hpp -- a multiply-accumulate process                     *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a simple sequential processes.        *
    *                                                                 *
    * Usage:   MulAcc example                                         *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/


#ifndef MULACC_HPP
#define MULACC_HPP

#include <forsyde.hpp>
#include "mul.hpp"
#include "add.hpp"

using namespace ForSyDe;

FORSYDE_COMPOSITE(mulacc), public SY::mn_composite<mulacc>
{
    SY::in_port<int>  a, b;
    SY::out_port<int> result;

    SY::signal<int> addi1, addi2, acci;

    SC_CTOR(mulacc)
    {
        add_scombMN("mul1", mul_func, std::tie(addi1), std::tie(a, b));

        auto& add1 = add_scombMN("add1", add_func, std::tie(acci), std::tie(addi1, addi2));
        std::get<0>(add1.oport)(result);

        add(new SY::sdelay("accum", 0))(addi2, acci);
    }
};

#endif
