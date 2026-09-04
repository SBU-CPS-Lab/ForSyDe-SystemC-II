/**********************************************************************
    * Top.hpp -- the top module and testbench for the mulacc example  *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a simple sequential processes.        *
    *                                                                 *
    * Usage:   MulAcc example                                         *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include "mulacc.hpp"
#include "report.hpp"
#include "siggen.hpp"
#include <iostream>

using namespace ForSyDe;

struct top : ForSyDe::composite
{
    SY::signal<int> srca, srcb, result;
    
    SC_CTOR(top)
    {
        add(new SY::sconstant("constant1", 3, 10))(srca);
        
        add(new SY::ssource("siggen1", siggen_func, 1, 10))(srcb);
        
        auto& mulacc1 = add(new mulacc("mulacc1"));
        mulacc1.a(srca);
        mulacc1.b(srcb);
        mulacc1.result(result);
        
        add(new SY::ssink("report1", report_func))(result);
    }
#ifdef FORSYDE_INTROSPECTION
    void start_of_simulation()
    {
        ForSyDe::XMLExport dumper("gen/");
        dumper.traverse(this);
    }
#endif
};
