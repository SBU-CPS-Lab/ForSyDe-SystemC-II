/**********************************************************************
    * main.cpp -- the main file for the bask_mod example.             *
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

#include "top.hpp"

int sc_main(int argc, char **argv)
{
    top top("top");

    sc_start(3,SC_US);
        
    return 0;
}
