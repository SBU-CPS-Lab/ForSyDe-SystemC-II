/**********************************************************************
    * generator.hpp -- simplified model of a gas-powered generator    *
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

#ifndef GENERATOR_HPP
#define GENERATOR_HPP

#include <forsyde.hpp>

using namespace ForSyDe;

struct generator : ForSyDe::composite
{
	CT::in_port drive;
	CT::in_port load_impedance;
    CT::out_port voltage;
	
	CT::signal limiter2sub, sub2scale, scale2int, int2sub, int2sub2, int2expr;
	
    generator(sc_module_name name_, double time_constant,
        double output_impedance, float drive_limit): composite(name_)
	{
        add(new CT::comb("limiter1", [drive_limit](CTTYPE& vout, const CTTYPE& vin)
            {
                if (vin>drive_limit) vout = drive_limit;
                else if (vin<0) vout = 0;
                else vout = vin;
            }))(limiter2sub, drive);
        
        add(new CT::sub("sub1"))(sub2scale, limiter2sub, int2sub);
        
        add(new CT::scale("scale1", 1.0/time_constant))(scale2int, sub2scale);
        
        // CT::make_integratorf built a filterf with the fixed
        // numerators/denominators of an integrator ({1.0}, {1.0,0.0}),
        // rather than a class of its own -- filterf is a composite now,
        // so its ports are bound by name like any other.
        auto& int1 = add(new CT::filterf("integrator1",
            std::vector<CTTYPE>{1.0}, std::vector<CTTYPE>{1.0,0.0}, sc_time(100 ,SC_MS)));
        int1.iport1(scale2int);
        int1.oport1(int2sub2);
        int1.oport1(int2expr);
        
        add(new CT::shift("delay1", sc_time(200,SC_MS)))(int2sub, int2sub2);
                
        add(new CT::comb2("expression1", [=](CTTYPE& vout, const CTTYPE& vin, const CTTYPE& imp)
            {
                vout = (imp == INFINITY) ? vin : vin*imp/(output_impedance+imp);
            }))(voltage, int2expr, load_impedance);
	}
};

#endif
