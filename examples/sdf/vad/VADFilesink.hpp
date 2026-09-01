/**********************************************************************
    * VADFilesink.hpp                                                 *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *          adapted from KisTA: https://github.com/nandohca/kista  *
    *                                                                 *
    * Purpose: Collect output stimuli                                 *
    *                                                                 *
    * Usage:   The VAD example                                        *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef VADFILESINK_HPP
#define VADFILESINK_HPP

#include <forsyde.hpp>
#include <iostream>

using namespace ForSyDe::SDF;

void VADFilesink_func(std::string& line, const short& out)
{
#pragma ForSyDe begin VADFilesink_func

    std::stringstream ss(line);
    ss << out;
    line = ss.str();
    
#pragma ForSyDe end
}


#endif
