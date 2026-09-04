/**********************************************************************           
    * top.hpp -- the Top process and testbench for the toy sdf example*
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a simple program.                     *
    *                                                                 *
    * Usage:   Toy SDF example                                        *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include "compAvg.hpp"
#include "upSampler.hpp"
#include "downSampler.hpp"
#include "report.hpp"
#include "stimuli.hpp"
#include <iostream>

using namespace ForSyDe;

struct top : ForSyDe::composite
{
    SDF::signal<double> src, upsrc, res, downres;
    
    SC_CTOR(top)
    {
        add(new SDF::source("stimuli1", stimuli_func, 0.0, 20))(src);
      
        add(new SDF::comb("upSampler1", upSampler_func, 2, 1))(upsrc, src);

        auto& compAvg1 = add(new compAvg("compAvg1"));
        compAvg1.iport1(upsrc);
        compAvg1.oport1(res);

        add(new SDF::comb("downSampler1", downSampler_func, 2, 3))(downres, res);
        
        add(new SDF::sink("report1", report_func))(downres);
    }
#ifdef FORSYDE_INTROSPECTION
    void start_of_simulation()
    {
        ForSyDe::XMLExport dumper("gen/");
        dumper.traverse(this);
    }
#endif
};
