/**********************************************************************
    * top.hpp -- the top module and testbench for the amplifier example*
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a simple example in the untimed MoC.  *
    *                                                                 *
    * Usage:   amplifier example                                      *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include "amplifier.hpp"
#include "ramp.hpp"
#include "report.hpp"
#include <iostream>

using namespace ForSyDe;

struct top : ForSyDe::composite
{
    UT::signal<int> src, result;

    SC_CTOR(top)
    {
        add(new UT::source("ramp1", ramp_func, 1, 20))(src);

        auto& amplifier1 = add(new amplifier("amplifier1"));
        amplifier1.iport1(src);
        amplifier1.oport1(result);

        add(new UT::sink("report1", report_func))(result);
    }
#ifdef FORSYDE_INTROSPECTION
    void start_of_simulation()
    {
        ForSyDe::XMLExport dumper("gen/");
        dumper.traverse(this);
    }
#endif

};
