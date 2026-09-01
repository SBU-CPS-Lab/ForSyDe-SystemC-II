/**********************************************************************           
    * abst_ext.hpp -- Absent-extended values data-type                *
    *                                                                 *
    * Author:  Hosein Attarzadeh (shan2@kth.se)                       *
    *                                                                 *
    * Purpose: Extend normal values with their absent-extended        *
    *          version, required by some MoCs (e.g., synhronous)      *
    *                                                                 *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef ABST_EXT_HPP
#define ABST_EXT_HPP

/*! \file abst_ext.hpp
 * \brief Implements the Absent-extended values
 */

#include <systemc>
#include <ostream>
#include <type_traits>
#include <utility>

namespace ForSyDe
{

using namespace sc_core;

namespace detail
{

//! Detects whether `os << val` is well-formed for a given type.
template <typename T, typename = void>
struct is_ostreamable : std::false_type {};

template <typename T>
struct is_ostreamable<T,
    std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>>
    : std::true_type {};

//! Streams a value if its type supports it, and a placeholder otherwise.
/*! This has to be a function *template* rather than a plain function or a
 * branch inlined into the caller: `if constexpr` only discards the
 * untaken branch inside a template, and the caller below is a friend
 * function defined in a class template, which is not itself a template.
 */
template <typename T>
inline std::ostream& stream_or_placeholder(std::ostream& os, const T& val)
{
    if constexpr (is_ostreamable<T>::value)
        return os << val;
    else
        return os << "<unprintable>";
}

}

//! Absent-extended data types
/*! This template class extends a type T to its absent-extended version.
 * Values of this type could be either absent, or present with a specific
 * value.
 */
template <typename T>
class abst_ext
{
public:
    //! The constructor with a present value
    abst_ext(const T& val) : present(true), value(val) {}
    
    //! The constructor with an absent value
    abst_ext() : present(false) {}
    
    //! Converts a value from an extended value, returning a default value if absent
    T from_abst_ext (const T& defval) const
    {
        if (present) return value; else return defval;
    }
    
    //! Converts a value from an extended value, returning a default value if absent
    inline friend T from_abst_ext (const abst_ext& absval, const T& defval)
    {
        if (absval.present) return absval.value; else return defval;
    }
    
    //! Unsafely converts a value from an extended value assuming it is present
    T unsafe_from_abst_ext () const {return value;}
    
    //! Unsafely converts a value from an extended value assuming it is present
    inline friend T unsafe_from_abst_ext(const abst_ext& absval)
    {
        return absval.value;
    }
    
    //! Sets absent
    void set_abst() {present=false;}
    
    //! Sets absent
    inline friend void set_abst(abst_ext& absval) {absval.present=false;}
    
    //! Sets the value
    void set_val(const T& val) {present=true;value=val;}
    
    //! Sets the value
    inline friend void set_val(abst_ext& absval, const T& val)
    {
        absval.present=true;
        absval.value=val;
    }
    
    //! Checks for the absence of a value
    bool is_absent() const {return !present;}
    
    //! Checks for the absence of a value
    inline friend bool is_absent(const abst_ext& absval) {return !absval.present;}
    
    //! Checks for the presence of a value
    bool is_present() const {return present;}
    
    //! Checks for the presence of a value
    inline friend bool is_present(const abst_ext& absval) {return absval.present;}
    
    //! Checks for the equivalence of two absent-extended values
    bool operator== (const abst_ext& rs) const
    {
        if (is_absent() || rs.is_absent())
            return is_absent() && rs.is_absent();
        else
            return unsafe_from_abst_ext() == rs.unsafe_from_abst_ext();
    }
    
    //! Overload the streaming operator to enable SystemC communiation
    /*! The present value is streamed through detail::stream_or_placeholder
     * rather than directly. This operator cannot simply be dropped for a
     * non-streamable T: sc_fifo<T>::print() is a virtual member, so it is
     * instantiated for every sc_fifo<abst_ext<T>> whether or not anything
     * ever calls it, and it does `os << m_buf[i]`. Removing this overload
     * would move the error into sc_fifo rather than eliminate it. So the
     * operator stays unconditionally available and degrades to a
     * placeholder instead, which is what lets abst_ext carry types with no
     * sensible textual form -- std::function above all, i.e. every
     * adaptive process that sends a function down a signal.
     */
    friend std::ostream& operator<< (std::ostream& os, const abst_ext &abst_ext)
    {
        if (abst_ext.is_present())
            detail::stream_or_placeholder(os, abst_ext.unsafe_from_abst_ext());
        else
            os << "_";
        return os;
    }
private:
    bool present;
    T value;
};

//! Check for presence in run time
/*! This macro is used mainly in the strict version of synchronous
 * processes to ensure that the received inputs are not absent.
 */
#define CHECK_PRESENCE(VAL) \
    if (is_absent(VAL)) SC_REPORT_ERROR(this->name(),"Unexpected absent value received in");

}
#endif
