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

FORSYDE_COMPOSITE(amplifier)
{
    UT::in_port<int>  iport1;
    UT::out_port<int> oport1;

    UT::signal<int> s2, s3, s4;

    SC_CTOR(amplifier)
    {
        add_mealyMN(*this, "A2P1", A2p_gamma_func, A2p_ns_func, A2p_od_func, std::make_tuple(),
            outs(readers(s4, oport1)), ins(s3, iport1));

        add_mealyMN(*this, "A3P1", A3p_gamma_func, A3p_ns_func, A3p_od_func, std::make_tuple(10),
            outs(s2), ins(s4));

        add(new UT::delay("A4p", 10))(s3, s2);
    }
};

#endif
