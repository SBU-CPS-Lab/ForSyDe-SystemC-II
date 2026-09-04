/**********************************************************************
    * amplifier.hpp -- a an adaptive amplifier process                *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *          taken from the book by Axel Jantsch (p. 114-122)       *
    *                                                                 *
    * Purpose: Demonstration of a simple example in the untimed MoC.  *
    *                                                                 *
    * Usage:   amplifier example                                      *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/


#ifndef AMPLIFIER_HPP
#define AMPLIFIER_HPP

#include <forsyde.hpp>
#include "A2p.hpp"
#include "A3p.hpp"

using namespace ForSyDe;

struct amplifier : ForSyDe::composite
{
    UT::in_port<int>  iport1;
    UT::out_port<int> oport1;

    UT::signal<std::tuple<std::vector<int>,std::vector<int>>> s1;
    UT::signal<int> s2, s3, s4;

    SC_CTOR(amplifier)
    {
        add(new UT::zips<int,int>("A1p", 1, 5))(s1, s3, iport1);

        auto& A2p1 = add(new UT::comb("A2p1", A2p_func, 1));
        A2p1(s4, s1);
        A2p1.oport1(oport1);

        add(new UT::scan("A3p1", A3p_gamma_func, A3p_ns_func, 10))(s2, s4);

        add(new UT::delay("A4p", 10))(s3, s2);
    }
};

#endif
