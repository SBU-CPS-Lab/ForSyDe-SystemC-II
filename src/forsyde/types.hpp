/**********************************************************************           
    * types.hpp -- provides a simple type reflection mechanism.       *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir) based on:   *
    * http://stackoverflow.com/questions/1055452/c-get-name-of-type-in-template *
    *                                                                 *
    * Purpose: Provide facilities to store the type names, used in    *
    *          introspection.                                         *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License:                                                        *
    *******************************************************************/

#ifndef TYPES_HPP
#define TYPES_HPP

#include <typeinfo>

/*! \file types.hpp
 * \brief Provides facilities for basic type introspection
 * 
 *  This file includes a a set of basic facilities for registering names
 * for non-POD C/C++ types to be reflected in the XML output of the
 * interospection stage.
 */

//~ namespace ForSyDe
//~ {

// The general case uses RTTI (if the type is not registered explicitly)
#pragma once
template<typename T> const char* get_type_name() {return typeid(T).name();}

// Specialization for each type
//
// `inline` is load-bearing, not decoration: an explicit specialization of
// a function template is an ordinary function, not a template, so unlike
// the primary template above it is *not* implicitly inline and does not
// get vague linkage. Without it, every translation unit that includes
// this header emits its own strong definition of all fourteen
// specializations below, and linking any two of them fails with
// "multiple definition of `get_type_name<char>()'" and so on -- which
// means the library could not be used from more than one .cpp file at
// all. Both macros are part of the public API (examples call them to
// register their own types), so they must carry it too.
#define DEFINE_TYPE(X) \
    template<> inline const char* get_type_name<X>(){return #X;}
// Another version where we explicitly provide the type name (for complex types)
#define DEFINE_TYPE_NAME(X,N) \
    template<> inline const char* get_type_name<X>(){return N;}

// Specialization for base types

DEFINE_TYPE(char);
DEFINE_TYPE(short int);
DEFINE_TYPE(unsigned short int);
DEFINE_TYPE(int);
DEFINE_TYPE(unsigned int);
DEFINE_TYPE(long int);
DEFINE_TYPE(unsigned long int);
DEFINE_TYPE(long long int);
DEFINE_TYPE(unsigned long long int);
DEFINE_TYPE(bool);
DEFINE_TYPE(float);
DEFINE_TYPE(double);
DEFINE_TYPE(long double);
DEFINE_TYPE(wchar_t);


//~ }

#endif
