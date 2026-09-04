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

struct compAvg : ForSyDe::composite
{
    SDF::in_port<double>  iport1;
    SDF::out_port<double> oport1;
    
    SDF::signal<double> din, dout;
    
    SC_CTOR(compAvg)
    {
        auto& averager1 = add(new SDF::comb2("averager1", averager_func, 2,3,2));
        averager1(oport1, iport1, dout);
        averager1.oport1(din);
        
        add(new SDF::delayn("avginit1",0.0,2))(dout, din);
    }
};

#endif
