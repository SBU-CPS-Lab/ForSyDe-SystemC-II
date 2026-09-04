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

struct mulacc : ForSyDe::composite
{
    SY::in_port<int>  a, b;
    SY::out_port<int> result;
    
    SY::signal<int> addi1, addi2, acci;
    
    SC_CTOR(mulacc)
    {
        auto& mul1 = add(new SY::scombMN<std::tuple<int>,std::tuple<int,int>>("mul1", mul_func));
        std::get<0>(mul1.iport)(a);
        std::get<1>(mul1.iport)(b);
        std::get<0>(mul1.oport)(addi1);

        auto& add1 = add(new SY::scombMN<std::tuple<int>,std::tuple<int,int>>("add1", add_func));
        std::get<0>(add1.iport)(addi1);
        std::get<1>(add1.iport)(addi2);
        std::get<0>(add1.oport)(acci);
        std::get<0>(add1.oport)(result);
        
        add(new SY::sdelay("accum", 0))(addi2, acci);
    }
};

#endif
