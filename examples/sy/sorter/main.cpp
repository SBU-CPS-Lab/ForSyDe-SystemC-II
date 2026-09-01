/**********************************************************************           
    * main.cpp -- the main file and testbench for the sorter example  *
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

#include "Top.hpp"

int sc_main(int argc, char **argv)
{
    Top top1("top1");

    sc_start();
        
    return 0;
}

