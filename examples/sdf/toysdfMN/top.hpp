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

FORSYDE_COMPOSITE(top), public SDF::mn_composite<top>
{
    SDF::signal<double> src, upsrc, res, downres;
    SDF::signal<int> cnt, cnt_delay;

    SC_CTOR(top)
    {
        add_combMN("stimuli1", stimuli_func, {1,1}, {1},
            std::tie(src, cnt), std::tie(cnt_delay));
        add(new SDF::delay("src_delay1", 0))(cnt_delay, cnt);
        // add(new SDF::source("stimuli1", stimuli_func, 0.0, 20))(src);

        add_combMN("upSampler1", upSampler_func, {2}, {1},
            std::tie(upsrc), std::tie(src));

        auto& compAvg1 = add(new compAvg("compAvg1"));
        compAvg1.iport1(upsrc);
        compAvg1.oport1(res);

        add_combMN("downSampler1", downSampler_func, {2}, {3},
            std::tie(downres), std::tie(res));

        add_combMN("report1", report_func, {}, {1},
            std::tie(), std::tie(downres));
        // add(new SDF::sink("report1", report_func))(downres);
    }
#ifdef FORSYDE_INTROSPECTION
    void start_of_simulation()
    {
        ForSyDe::XMLExport dumper("gen/");
        dumper.traverse(this);
    }
#endif
};
