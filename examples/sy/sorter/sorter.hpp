/**********************************************************************           
    * sorter.hpp -- the top level module of a sorter                  *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a simple program.                     *
    *                                                                 *
    * Usage:   Sorter example                                         *
    *          inspired by material from Doulos SystemC course        *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/


#ifndef SORTER_HPP
#define SORTER_HPP

#include <forsyde.hpp>
#include "comparator.hpp"
#include "mux.hpp"
#include "decoder.hpp"

using namespace ForSyDe::SY;

FORSYDE_COMPOSITE(sorter)
{
    SY_in<int>  a, b, c;
    SY_out<int> biggest;
    
    SY2SY<int> c11, c12, c21, c22, c31, c32,
                 m1, m2, m3, m4;
    SY2SY<bool> dec1, dec2, dec3;
    
    SC_CTOR(sorter)
    {
        add(new fanout<int>("foa"))(readers(c11, c32, m2), a);

        add(new fanout<int>("fob"))(readers(c12, c21, m3), b);

        add(new fanout<int>("foc"))(readers(c22, c31, m4), c);
        
        add(new comb2("comparator1", comparator_func))(dec1, c11, c12);
        
        add(new comb2("comparator2", comparator_func))(dec2, c21, c22);
        
        add(new comb2("comparator3", comparator_func))(dec3, c31, c32);
        
        add(new comb3("decoder1", decoder_func))(m1, dec1, dec2, dec3);
        
        add(new comb4("mux1", mux_func))(biggest, m1, m2, m3, m4);
    }
};

#endif
