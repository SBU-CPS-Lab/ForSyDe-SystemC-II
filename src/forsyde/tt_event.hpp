/**********************************************************************           
    * tt_event.hpp -- Time-Tagged Event data type                     *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Define a time-tagged event used in the timed MoCs      *
    *                                                                 *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef TT_EVENT_HPP
#define TT_EVENT_HPP

/*! \file tt_event.hpp
 * \brief Implements the time-tagged events
 */

#include <systemc>
#include "abst_ext.hpp"

namespace ForSyDe
{

using namespace sc_core;

//! Time-tagged data types
/*! This template class defines a timed event as a combination of a
 * value typed T, and a time value defaulted to sc_time.
 */
template <typename VT, typename TT=sc_time>
struct tt_event
{
    //! The constructor with time and value
    tt_event(const VT& value, const TT& time) : value(value), time(time) {}
    
    //! The default constructor
    tt_event ()
    {
        value = VT();
        time = TT();
    }
    
    //! The copy constructor
    tt_event (const tt_event& ev)
    {
        value = get_value(ev);
        time = get_time(ev);
    }

    //! The copy-assignment operator
    /*! D9: declaring the copy constructor above without also declaring
     * this makes the implicitly-generated copy-assignment operator
     * deprecated (-Wdeprecated-copy), even though it would already do
     * the right thing here -- both value and time are plain data
     * members with no resource to manage. Given verbatim rather than
     * defaulted to keep the two consistent should either type ever gain
     * one.
     */
    tt_event& operator= (const tt_event& ev)
    {
        value = get_value(ev);
        time = get_time(ev);
        return *this;
    }

    //! Checks for the equivalence of two timed events
    /*! Returns true only if both the values and time tags match.
     */
    bool operator== (const tt_event& ev) const
    {
        return (value == get_value(ev)) && (time == get_time(ev));
    }
    
    //! Overload the streaming operator to enable SystemC communiation
    friend std::ostream& operator<< (std::ostream& os, const tt_event &ev)
    {
        os << "(" << ev.value << "," << ev.time << ")";
        return os;
    }
    
    inline friend VT get_value(const tt_event& ev) {return ev.value;}
    
    inline friend TT get_time(const tt_event& ev) {return ev.time;}
    
    inline friend void set_value(tt_event& ev, const VT& v) {ev.value = v;}
    
    inline friend void set_time(tt_event& ev, const TT& t) {ev.time = t;}
    
private:
    VT value;
    TT time;
};

//! The time-tagged data-type with an absent-extended value representing null values
template<typename T>
using ttn_event = tt_event<abst_ext<T>>;

}
#endif
