/**********************************************************************
    * Top.hpp -- the top module and testbench for the datapar example *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a data parallel model.                *
    *                                                                 *
    * Usage:   datapar example                                        *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include "inc.hpp"
#include "add.hpp"
#include "report.hpp"

#include <array>

using namespace ForSyDe;
using namespace std;

//std::array<int,10> inpval = {{0,1,2,3,4,5,6,7,8,9}};
std::array<int,10000> inpval;

struct top : ForSyDe::composite
{
    SY::signal<std::array<int,10000>> srca, srcb, scanned;
    SY::signal<int> result;
    
    SC_CTOR(top)
    {
        inpval.fill(1);
        add(new SY::sconstant("constant1", inpval, 10))(srca);
        
        add(new SY::sdpmap<int,int,10000>("inc1", inc_func))(srcb, srca);
        
        add(new SY::sdpscan<int,int,10000>("add1", add_func, 0))(scanned, srcb);
        
        add(new SY::sdpreduce<int,10000>("add2", add_func))(result, scanned);
        
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
