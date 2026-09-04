/**********************************************************************
    * bask_mod.hpp -- the BASK mudulator module.                      *
    *                                                                 *
    * Author:  Gilmar Beserra (gilmar@kth.se)                         *
    *          Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: A BASK modulator system.                               *
    *                                                                 *
    * Usage:   bask_mod example                                       *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef BASKMODE_HPP
#define BASKMODE_HPP

#include <forsyde.hpp>

using namespace sc_core;
using namespace ForSyDe::CT;

struct bask_mod : ForSyDe::composite
{
	CT_in iport1;
	CT_out oport1;
	
	CT2CT carrier;
	
	SC_CTOR(bask_mod)
	{
        add(new sine("sine1", sc_time(3,SC_US), sc_time(100, SC_NS), 1.0))(carrier);
		
		add(new mul("mixer"))(oport1, iport1, carrier);
	}
};

#endif
