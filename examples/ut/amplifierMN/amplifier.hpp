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

FORSYDE_COMPOSITE(amplifier), public UT::mn_composite<amplifier>
{
    UT::in_port<int>  iport1;
    UT::out_port<int> oport1;

    UT::signal<int> s2, s3, s4;

    SC_CTOR(amplifier)
    {
        auto& a2p1 = add_mealyMN("A2P1", A2p_gamma_func, A2p_ns_func, A2p_od_func, std::make_tuple(),
            std::tie(s4), std::tie(s3, iport1));
        std::get<0>(a2p1.oport)(oport1);

        add_mealyMN("A3P1", A3p_gamma_func, A3p_ns_func, A3p_od_func, std::make_tuple(10),
            std::tie(s2), std::tie(s4));

        add(new UT::delay("A4p", 10))(s3, s2);
    }
};

#endif
