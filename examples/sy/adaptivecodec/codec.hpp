/**********************************************************************
    * codec.hpp -- an adaptive encoder/decoder                        *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a simple adaptive system.             *
    *                                                                 *
    * Usage:   Adaptive Codec example                                 *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/


#ifndef CODEC_HPP
#define CODEC_HPP

#include <forsyde.hpp>
#include <tuple>

#include "keygen.hpp"

using namespace ForSyDe::SY;
using namespace std;

struct codec : ForSyDe::composite
{
    SY_in<int>  iport;
    SY_in<int>  code;
    SY_out<int> oport;
    
    SY2SY<int> coded;
    SY2SY<tuple<abst_ext<functype>,abst_ext<functype>>> keys;
    SY2SY<functype> key1, key2;
    
    SC_CTOR(codec)
    {
        auto& encoder1 = add(new ForSyDe::SY::apply<int,int>("encoder1"));
        encoder1(coded, iport);
        encoder1.fport(key1);
        
        auto& decoder1 = add(new ForSyDe::SY::apply<int,int>("decoder1"));
        decoder1(oport, coded);
        decoder1.fport(key2);
        
        add(new comb<tuple<abst_ext<functype>,abst_ext<functype>>,int>("keygen1", keygen_func))
            (keys, code);
        
        add(new unzip<functype,functype>("unzip1"))(key1, key2, keys);
    }
};

#endif
