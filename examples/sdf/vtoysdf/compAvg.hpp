/**********************************************************************           
    * compAvg.hpp -- A composite process which includes an averager   *
    *          with a delay.                                          *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a variadic program using zip and unzip*
    *                                                                 *
    * Usage:   Toy SDF example                                        *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/


#ifndef COMPAVG_HPP
#define COMPAVG_HPP

#include <forsyde.hpp>
#include "averager.hpp"
#include <array>
#include <vector>
#include <tuple>

// SDF::zipN / SDF::unzipN take std::array<size_t, N>, where N is the
// number of zipped signals, so that the rate list cannot disagree in
// length with the port list. This example still passed the
// std::vector<uint> that an older signature accepted, which is why it
// stopped compiling: the token counts are part of the process's type
// now, not a runtime-sized argument. constexpr also keeps these out of
// the ODR trouble a mutable namespace-scope definition in a header
// would cause in a multi-file model.
constexpr std::array<size_t,2> itoks = {3,2};
constexpr std::array<size_t,2> otoks = {2,2};

using namespace ForSyDe;
using namespace std;

struct compAvg : ForSyDe::composite
{
    SDF::in_port<float>  iport;
    SDF::out_port<float> oport;
        
    SDF::signal<float> din, dout;
    SDF::signal< tuple<vector<float>,vector<float>> > zi, zo;
    
    SC_CTOR(compAvg)
    {
        auto& zip1 = add(new SDF::zipN<float,float>("zip1",itoks));
        get<0>(zip1.iport)(iport);
        get<1>(zip1.iport)(dout);
        zip1.oport1(zi);
        
        add(new SDF::comb("averager1", averager_func, 1, 1))(zo, zi);
        
        auto& unzip1 = add(new SDF::unzipN<float,float>("unzip1",otoks));
        unzip1.iport1(zo);
        get<0>(unzip1.oport)(oport);
        get<1>(unzip1.oport)(din);
        
        add(new SDF::delayn("avginit1", (float)0, 2))(dout, din);
    }
};

#endif
