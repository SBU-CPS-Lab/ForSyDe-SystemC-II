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

FORSYDE_COMPOSITE(mulacc)
{
    SY::in_port<int>  a, b;
    SY::out_port<int> result;

    SY::signal<int> addi1, addi2, acci;

    SC_CTOR(mulacc)
    {
        add_scombMN(*this, "mul1", mul_func, outs(addi1), ins(a, b));

        add_scombMN(*this, "add1", add_func, outs(readers(acci, result)), ins(addi1, addi2));

        add(new SY::sdelay("accum", 0))(addi2, acci);
    }
};

#endif
