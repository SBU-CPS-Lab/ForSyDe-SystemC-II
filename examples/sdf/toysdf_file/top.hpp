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
    SDF::signal<double> src, src2, upsrc, res, downres;
    SDF::signal<std::tuple<std::vector<double>,std::vector<double>>> zipped_res;
    
    SC_CTOR(top)
    {
        auto& stimuli1 = add(new SDF::file_source("stimuli1", stimuli_func, 
            "input.txt"
        ));
        stimuli1(src);
        stimuli1.oport1(src2);
      
        add(new SDF::comb("upSampler1", upSampler_func, 2, 1))(upsrc, src);

        auto& compAvg1 = add(new compAvg("compAvg1"));
        compAvg1.iport1(upsrc);
        compAvg1.oport1(res);

        add(new SDF::comb("downSampler1", downSampler_func, 2, 3))(downres, res);
        
        add(new SDF::zip<double,double>("zip1", 4, 3))(zipped_res, src2, downres);
        
        add(new SDF::file_sink("report1", report_func, "output.txt"))(zipped_res);
    }
#ifdef FORSYDE_INTROSPECTION
    void start_of_simulation()
    {
        ForSyDe::XMLExport dumper("gen/");
        dumper.traverse(this);
    }
#endif
};
