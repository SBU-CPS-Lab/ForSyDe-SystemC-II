/**********************************************************************
    * top.hpp -- the top module and testbench for the toyde example   *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a simple DDE system.                  *
    *                                                                 *
    * Usage:   ToyDDE example                                         *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include "report.hpp"
#include "inc.hpp"
#include "add.hpp"
#include "buf_add.hpp"
#include <iostream>

using namespace ForSyDe;

struct top : ForSyDe::composite
{
    DDE::signal<int> srca, feedback, addi1, addi2, result, addi1p, addi2p, buf_result;
    DDE::signal<std::tuple<abst_ext<int>,abst_ext<int>>> zip_result;
    
    SC_CTOR(top)
    {
        add(new DDE::delay("delay1", abst_ext<int>(0), sc_time(10, SC_NS)))(srca, feedback);
        
        auto& inc1 = add(new DDE::comb("inc1", inc_func));
        inc1(feedback, srca);
        inc1.oport1(addi1);
        inc1.oport1(addi1p);
        
        auto& const1 = add(new DDE::vsource<int>("const1",
                std::vector<int>(1,7),
                std::vector<sc_time>(1,sc_time(50,SC_NS))
        ));
        const1(addi2);
        const1.oport1(addi2p);
        
        add(new DDE::comb2("add1", add_func))(result, addi1, addi2);
        
        add(new DDE::mealy2("buf_add1", buf_add_ns_func, buf_add_od_func,
            std::make_tuple((int)0,(int)0),
            sc_time(0,SC_NS)))(buf_result, addi1p, addi2p);
        
        add(new DDE::zip<int,int>("zip1"))(zip_result, result, buf_result);
        
        add(new DDE::sink("report1", report_func))(zip_result);
    }
#ifdef FORSYDE_INTROSPECTION
    void start_of_simulation()
    {
        ForSyDe::XMLExport dumper("gen/");
        dumper.traverse(this);
    }
#endif
};
