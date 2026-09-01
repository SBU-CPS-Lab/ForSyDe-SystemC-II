/**********************************************************************
    * stimuli.hpp -- a stimuli generator                              *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a simple program.                     *
    *                                                                 *
    * Usage:   Toy SDF example                                        *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/


#ifndef STIMULI_HPP
#define STIMULI_HPP

#include <forsyde.hpp>

using namespace ForSyDe::SDF;

void stimuli_func(double& out, const std::string& line)
{
    std::stringstream ss(line);
    ss >> out;
}

#endif
