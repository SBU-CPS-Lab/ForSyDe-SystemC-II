/**********************************************************************           
    * compAvg.hpp -- A composite process which includes an averager   *
    *          with a delay.                                          *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a simple program.                     *
    *                                                                 *
    * Usage:   Toy SDF example                                        *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/


#ifndef COMPAVG_HPP
#define COMPAVG_HPP

#include <forsyde.hpp>
#include "averager.hpp"

using namespace ForSyDe;

FORSYDE_COMPOSITE(compAvg)
{
    SDF::in_port<double>  iport1;
    SDF::out_port<double> oport1;

    SDF::signal<double> din, dout;

    SC_CTOR(compAvg)
    {
        add_combMN(*this, "averager1", averager_func, {2}, {3,2},
            outs(readers(oport1, din)), ins(iport1, dout));

        add(new SDF::delayn("avginit1",0.0,2))(dout, din);
    }
};

#endif
