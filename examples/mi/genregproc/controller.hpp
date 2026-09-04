/**********************************************************************
    * controller.hpp -- The controller of the geenrator               *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *          Based on the example from chapter 1 of:                *
    *          System Design, Modeling, and Simulation using PtolemyII*
    *                                                                 *
    * Purpose: Demonstration of a single cyber-physical system        *
    *                                                                 *
    * Usage:   Generator/Regulator/Protector example                  *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <forsyde.hpp>

using namespace ForSyDe;

struct controller : ForSyDe::composite
{
	DDE::in_port<double> voltage;
    DDE::out_port<double> drive;
	
	DDE::signal<double> voltage2, trigger, desired_v, err;
    CT::signal err_ct, drive_ct;
	
    SC_CTOR(controller)
	{
        auto& fanout1 = add(new DDE::fanout<double>("fanout1"));
        fanout1(trigger, voltage);
        fanout1.oport1(voltage2);
        
        add(new DDE::comb("desired_v1", 
            [](abst_ext<double>& desv, const double& trig) {desv=abst_ext<double>(110.0);}
        ))(desired_v, trigger);
        
        add(new DDE::comb2("sub1",
            [](abst_ext<double>& res, const abst_ext<double>& inp1, const abst_ext<double>& inp2)
            {
                res = abst_ext<double>(unsafe_from_abst_ext(inp1)-unsafe_from_abst_ext(inp2));
            }
        ))(err, desired_v, voltage2);
        
        add(new DDE2CT<double>("de2ct1", HOLD))(err_ct, err);
        
        auto& pi1 = add(new CT::pif("pi1", 1.1, 1.0, sc_time(100,SC_MS)));
        pi1.iport1(err_ct);
        pi1.oport1(drive_ct);
        
        add(new CT2DDEf<double>("ct2de1", sc_time(100, SC_MS)))(drive, drive_ct);
        
	}
};

#endif
