/**********************************************************************           
    * Top.hpp -- the Top process and testbench for the toy sdf example*
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a variadic program using zip and unzip*
    *                                                                 *
    * Usage:   Toy SDF example                                        *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include <forsyde.hpp>
#include "compAvg.hpp"
#include "upSampler.hpp"
#include "downSampler.hpp"
#include <iostream>

using namespace ForSyDe;

void stimuli_func(float& out1, const float& inp1)
{
  out1 = inp1+1;
}

void report_func(float inp1)
{
    std::cout << "output value: " << inp1 << std::endl;
}

struct Top : ForSyDe::composite
{
    SDF::signal<float> src, upsrc, res, downres;
        
    SC_CTOR(Top)
    {
        add(new SDF::source("stimuli1", stimuli_func, (float)0, 100))(src);
      
        add(new SDF::comb("upSampler1", upSampler_func, 2, 1))(upsrc, src);

        auto& compAvg1 = add(new compAvg("compAvg1"));
        compAvg1.iport(upsrc);
        compAvg1.oport(res);

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
