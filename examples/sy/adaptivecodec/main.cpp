/**********************************************************************
    * main.cpp -- the main file and testbench for the mulacc example  *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a simple sequential processes.        *
    *                                                                 *
    * Usage:   MulAcc example                                         *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include "Top.hpp"

int sc_main(int argc, char **argv)
{
    Top top("top");

    sc_start();
        
    return 0;
}


