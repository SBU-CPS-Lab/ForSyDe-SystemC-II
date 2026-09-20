/**********************************************************************
    * splitter.hpp -- a splitter composite process                    *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a simple DDE system.                  *
    *                                                                 *
    * Usage:   Packet Verifier example                                *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/


#ifndef SPLITTER_HPP
#define SPLITTER_HPP

#include <forsyde.hpp>

using namespace ForSyDe;

FORSYDE_COMPOSITE(splitter)
{
    DDE::in_port<char> iport1;
    DDE::in_port<int> iport2;
    DDE::out_port<int> oport1;
    DDE::out_port<int> oport2;
    
    DDE::signal<std::tuple<abst_ext<int>,abst_ext<int>>> zout;
    
    SC_CTOR(splitter)
    {        
        add(new DDE::mealy2("split", split_ns_func, split_od_func, 'V', SC_ZERO_TIME))
            (zout, iport1, iport2);
        
        add_unzip(*this, "unzip1", zout, oport1, oport2);
    }
    
    static void split_ns_func(char& nst, const char& st, 
        const ttn_event<char>& inp1, const ttn_event<int>& inp2)
    {
        // is_present first: an absent event carries no value, and
        // unsafe_from_abst_ext does exactly what it says -- it hands
        // back the stored value without checking. Reading it unguarded
        // was undefined behaviour, and it behaved like it: this example
        // produced its golden output on one machine and a different
        // verdict ('F' where the model means 'V') on a CI runner, from
        // the same source, because what came back was whatever happened
        // to be in that field. An absent event is not a failure marker,
        // so it must leave the verdict alone.
        nst = (st == 'F' ||
               (is_present(get_value(inp1)) &&
                unsafe_from_abst_ext(get_value(inp1)) == 'F'))
            ? 'F'
            : 'V';
    }
    
    static void split_od_func(abst_ext<std::tuple<abst_ext<int>,abst_ext<int>>>& out, const char& st, 
        const ttn_event<char>& inp1, const ttn_event<int>& inp2)
    {
        if (st == 'F' || is_absent(get_value(inp2)))
            out = std::tuple<abst_ext<int>,abst_ext<int>>();
        else
        {
            auto packet = unsafe_from_abst_ext(get_value(inp2));
            if (packet % 2 == 0)
                out = std::make_tuple(abst_ext<int>(packet),abst_ext<int>());
            else if (abs(packet % 2) == 1)
                out = std::make_tuple(abst_ext<int>(),abst_ext<int>(packet));
            else
                out = std::tuple<abst_ext<int>,abst_ext<int>>();
        }
    }

};


#endif
