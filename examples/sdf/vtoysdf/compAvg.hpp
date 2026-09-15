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

FORSYDE_COMPOSITE(compAvg)
{
    SDF::in_port<float>  iport;
    SDF::out_port<float> oport;
        
    SDF::signal<float> din, dout;
    SDF::signal< tuple<vector<float>,vector<float>> > zi, zo;
    
    SC_CTOR(compAvg)
    {
        add_zipN(*this, "zip1", itoks, zi, iport, dout);

        add(new SDF::comb("averager1", averager_func, 1, 1))(zo, zi);

        add_unzipN(*this, "unzip1", otoks, zo, oport, din);
        
        add(new SDF::delayn("avginit1", (float)0, 2))(dout, din);
    }
};

#endif
