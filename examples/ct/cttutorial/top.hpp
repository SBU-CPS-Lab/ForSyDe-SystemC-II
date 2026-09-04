/**********************************************************************           
    * Top.hpp -- the Top process and testbench for a tutorial CT exmpl*
    *                                                                 *
    * Authors:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)            *
    *           Jun Zhu (junz@kth.se)                                 *
    *                                                                 *
    * Purpose: Demonstration of a simple program.                     *
    *                                                                 *
    * Usage:   Tutorial CT example                                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include <forsyde.hpp>

#include "globals.hpp"
#include "add.hpp"

using namespace sc_core;
using namespace ForSyDe;

struct Top : ForSyDe::composite
{
    CT::signal cosSrc, NoiseSrc1, NoiseSrc2, filtInp, filtOut;
    
    SC_CTOR(Top)
    {
        
        add(new CT::cosine("cosine1", endT, CosPeriod, 1.0))(cosSrc);
        
        //~ add(new CT::cosine("cosine2", endT, CosPeriod/10, 0.1))(NoiseSrc1);
        auto& gaussian1 = add(new CT::gaussian("gaussian1", 0.01, 0, sc_time(1, SC_MS)));
        gaussian1.oport1(NoiseSrc1);
        
        auto& add1 = add(new CT::comb2("add1", add_func));
        add1(filtInp, cosSrc, NoiseSrc1);
        add1.oport1(NoiseSrc2);
        
        auto& filter1 = add(new CT::filter("filter1", nums, dens, samplingPeriod));
        filter1.iport1(filtInp);
        filter1.oport1(filtOut);
        
        add(new CT::traceSig("report1", sc_time(100,SC_US)))(filtOut);
        
        add(new CT::traceSig("report2", sc_time(100,SC_US)))(NoiseSrc2);
    }
   
};
