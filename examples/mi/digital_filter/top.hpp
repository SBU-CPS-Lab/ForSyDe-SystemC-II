/**********************************************************************           
    * top.hpp -- the Top process and testbench for the digital filter *
    *                                                                 *
    * Authors: Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a heterogeneous system.               *
    *                                                                 *
    * Usage:   The digital filter example                             *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include <forsyde.hpp>

#include "globals.hpp"
#include "ctadd.hpp"
#include "fir.hpp"

using namespace sc_core;
using namespace ForSyDe;
using namespace ForSyDe::CT;

struct top : ForSyDe::composite
{
    CT2CT cosSrc, NoiseSrc1, NoiseSrc2, filtInp, filtOut;
    SY::SY2SY<double> dig_in, dig_out;
    
    SC_CTOR(top)    
    {
        
        add(new CT::cosine("cosine1", endT, CosPeriod, 1.0))(cosSrc);
        
        auto& gaussian1 = add(new CT::gaussian("gaussian1", 0.01, 0, sc_time(1, SC_MS)));
        gaussian1.oport1(NoiseSrc1);
        
        auto& ctadd1 = add(new CT::comb2("ctadd1", ctadd_func));
        ctadd1(filtInp, cosSrc, NoiseSrc1);
        ctadd1.oport1(NoiseSrc2);
        
        add(new CT2SY("a2d", samplingPeriod))(dig_in, filtInp);
        
        auto& fir1 = add(new fir("fir1"));
        fir1.iport1(dig_in);
        fir1.oport1(dig_out);
        
        add(new SY2CT("d2a", samplingPeriod, LINEAR))(filtOut, dig_out);
                
        add(new CT::traceSig("report1", sc_time(100,SC_US)))(filtOut);
        
        add(new CT::traceSig("report2", sc_time(100,SC_US)))(NoiseSrc2);
    }
   
};
