/**********************************************************************
    * mis.hpp -- MoC interfaces for the SystemC map of the ForSyDe    *
    *          library                                                *
    *                                                                 *
    * Authors: Gilmar Besera (gilmar@kth.se)                          *
    *          Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Providing MoC interfaces for ForSyDe-SystemC           *
    *                                                                 *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef MIS_HPP
#define MIS_HPP

/*! \file mis.hpp
 * \brief Implements the MoC interfaces between different MoCs
 * 
 *  This file includes the basic process constructors and other
 * facilities used for creating MoC interfaces between different MoCs.
 */

#include <systemc>
// mis.hpp defines MoC interfaces, so unlike a single-MoC header it
// genuinely needs more than one MoC's process/port/signal types --
// abssemantics.hpp (for the process base class), and each MoC pair a
// converter class below actually names (SY, CT, DDE; tt_event.hpp comes
// in transitively via dde_process.hpp for ttn_event<T>). It relied on
// forsyde.hpp having already included all of ut_moc.hpp through
// dde_moc.hpp by the time it reaches mis.hpp for every one of these.
#include "abssemantics.hpp"
#include "sy_process.hpp"
#include "sdf_process.hpp"
#include "ct_process.hpp"
#include "dde_process.hpp"
#include "ut_process.hpp"
#include "dt_process.hpp"
#include "sadf_process.hpp"
// NOTE -- the MoC interfaces below wait with a bare
//     wait(t - sc_time_stamp());
// rather than through ForSyDe::wait_until(), which is the guarded form
// the DDE and CT process constructors use and which reports an attempt
// to move local time backwards instead of letting sc_time wrap.
//
// That is not because the interfaces are exempt. It is because routing
// them through the guard immediately fires in three of the models in
// this repository:
//
//   mi/cruisecontrol   top.plant1.car.de2ct1   to 10 ms, already at 20 ms
//   ct/cttutorial      top.filter1.ct2de1      to 25 us, already at 100 us
//   mi/ir_uwb_radar    top1.radar1.adc         to 500 ps, already at 525 ps
//
// Each of those is a real backwards wait today, and sc_time is unsigned,
// so each is really a wait of about 213 days of simulated time -- after
// which the process never runs again. ct/cttutorial and mi/ir_uwb_radar
// are the suite's two registered timeouts, so this is a candidate
// explanation for both, and mi/cruisecontrol passes only because the
// process that parks had nothing left to contribute.
//
// The interfaces read ahead by design -- DDE2CT consumes events until it
// has the two that bracket an interval, then emits a sub-signal over it
// -- so the fix is not simply to clamp the wait; it needs the interval
// reconstruction and the local clock reconciled, which changes what
// these processes mean. That belongs with the MoC interface work rather
// than with adding a guard, so the guard is deliberately not applied
// here yet and this note is the record of what it found.

namespace ForSyDe
{
using namespace sc_core;

//! The MoC interfaces, written once per pair of *carriers*
/*! Jantsch's chapter 6 organises these by what they do to timing
 * information rather than by which two MoCs they sit between. Table 6-1
 * has six entries over three timing regimes, and the definitions then
 * collapse them further: stripS2U is defined as *being* stripT2U (6.2),
 * and insertU2T and insertS2T as being insertU2S (6.5, 6.6). What is
 * left is two operations -- drop the absent events, or emit an event
 * followed by lambda-1 absent ones -- and one more, stripT2S, that
 * groups lambda timed events into one synchronous event.
 *
 * The two implemented here are written against a *pair of MoCs* rather
 * than a pair of names, so every combination exists rather than the two
 * that happened to be written by hand. {SY, DT} to {UT, SDF, SADF} is
 * six interfaces from one class, and back again is six more from the
 * other -- which is the gap where UT, DT and SADF had no interfaces at
 * all. They never needed their own; they needed these.
 */
namespace MI
{

//! The port and signal types of a model of computation, by its identity
/*! Lets an interface be written once against a *pair* of MoCs instead of
 * once per pair of names. Not specialised for CT, whose ports are not
 * templated on a value type at all.
 */
template <moc_id M, typename T> struct port_of;

template <typename T> struct port_of<moc_id::UT,T>
{typedef UT::UT_in<T> in; typedef UT::UT_out<T> out;};
template <typename T> struct port_of<moc_id::SDF,T>
{typedef SDF::SDF_in<T> in; typedef SDF::SDF_out<T> out;};
template <typename T> struct port_of<moc_id::SADF,T>
{typedef SADF::SADF_in<T> in; typedef SADF::SADF_out<T> out;};
template <typename T> struct port_of<moc_id::SY,T>
{typedef SY::SY_in<T> in; typedef SY::SY_out<T> out;};
template <typename T> struct port_of<moc_id::DT,T>
{typedef DT::DT_in<T> in; typedef DT::DT_out<T> out;};

//! Remove timing information: drop the absent events (Jantsch 6.1, 6.2)
/*! The interface from a synchronous or timed domain to an untimed one.
 * A synchronous signal says something at every tick, including "nothing
 * happened"; an untimed signal has no ticks to say it at. So the absent
 * events are what has to go, and the present ones pass through in the
 * order they arrived:
 *
 *      pi(nu, s)  = <e_i>,  nu(i)  = 1
 *      pi(nu', s) = <a_i>,  nu'(i) = 0 if e_i is absent, 1 otherwise
 *
 * Jantsch defines stripT2U (6.1) and then stripS2U (6.2) as *the same
 * constructor* -- from an untimed signal's point of view a time tag and
 * a tick are both timing information it does not have, so removing
 * either is the same operation. That is why this is one class over a
 * pair of MoCs rather than one class per pair of names, and why every
 * combination of {SY, DT} to {UT, SDF, SADF} exists now rather than the
 * single SY-to-SDF that was written by hand.
 *
 * \a From must be a MoC that carries timing information -- SY or DT --
 * and \a To an untimed one. Both are checked, on the timing ladder
 * rather than on the carrier: SY and DT share a carrier and differ by
 * exactly the thing this class removes.
 */
template <moc_id From, moc_id To, typename T>
class strip : public process
{
public:
    typename port_of<From,T>::in  iport1;   ///< port for the input channel
    typename port_of<To,T>::out   oport1;   ///< port for the output channel

    //! The constructor requires the module name
    strip(sc_module_name _name      ///< process name
         ) : process(_name), iport1("iport1"), oport1("oport1")
    {
        static_assert(on_timing_ladder(From) && on_timing_ladder(To),
            "strip is one of Jantsch's chapter 6 interfaces and is defined "
            "over the untimed/synchronous/timed ladder. DDE and CT are not "
            "on it: a DDE event carries its own tag rather than sitting on "
            "a grid, and a CT signal is a function over an interval. "
            "Converting either is a change of carrier and needs a physical "
            "sample period, not a count of events.");
        static_assert(timing_rank(From) > timing_rank(To),
            "strip removes timing information, so it has to start in a "
            "domain that has more of it than the destination.");
        static_assert(timing_rank(To) == 0,
            "strip's output is an untimed signal. To land in a synchronous "
            "domain from a timed one, group events instead -- that is "
            "MI::group, Jantsch's stripT2S, and it needs a ratio.");
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const
    {
        return std::string("MI::strip<") + moc_name(From) + "," + moc_name(To) + ">";
    }

private:
    T val;
    bool have_val;

    void init() {val = T(); have_val = false;}

    void prep()
    {
        // Read until something is actually there. The value read is what
        // gets written -- the hand-written SY2SDF this replaces looped
        // here correctly and then wrote a member it had never assigned,
        // so it emitted an uninitialised value for every token of every
        // run. Nothing used it, so nothing found out.
        auto tok = iport1.read();
        while (is_absent(tok))
            tok = iport1.read();
        val = unsafe_from_abst_ext(tok);
        have_val = true;
    }

    void exec() {}

    void prod() {if (have_val) write_multiport(oport1, val);}

    void clean() {}

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        boundInChans.resize(1);     // only one input port
        boundInChans[0].port = &iport1;
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
    }
#endif
};

//! Add timing information: one event, then lambda-1 absent ones (Jantsch 6.4-6.6)
/*! The interface from an untimed domain into a synchronous one. An
 * untimed signal carries no statement about when its events happen, so
 * the interface has to invent one, and the simplest such statement is a
 * constant ratio: each input event occupies lambda ticks of the output,
 * the first carrying the value and the rest carrying nothing.
 *
 *      a_i = e_i (+) absent^(lambda-1)
 *
 * Jantsch gives this as insertU2S (6.4), then insertU2T (6.5) and
 * insertS2T (6.6) as the same construction -- inserting ticks and
 * inserting time tags differ in what the destination calls them, not in
 * what the interface does. lambda = 1, one token per tick, is what the
 * hand-written SDF2SY did without saying so or letting you change it.
 *
 * Note that 6.6 goes from *synchronous* to timed, so the condition is a
 * step up the timing ladder rather than a start on its bottom rung. SY
 * to DT is the case in this library, and it is the one that shows why
 * the ladder is not the carrier: those two MoCs put the same token on
 * the wire and this interface is exactly what separates them.
 */
template <moc_id From, moc_id To, typename T>
class insert : public process
{
public:
    typename port_of<From,T>::in  iport1;   ///< port for the input channel
    typename port_of<To,T>::out   oport1;   ///< port for the output channel

    //! The constructor requires the module name and the event ratio
    insert(sc_module_name _name,        ///< process name
           unsigned long lambda = 1     ///< output events per input event
          ) : process(_name), iport1("iport1"), oport1("oport1"), lambda(lambda)
    {
        static_assert(on_timing_ladder(From) && on_timing_ladder(To),
            "insert is one of Jantsch's chapter 6 interfaces and is defined "
            "over the untimed/synchronous/timed ladder. DDE and CT are not "
            "on it: their events are tagged, or their signals continuous, "
            "so entering them needs a physical sample period rather than a "
            "count of events.");
        static_assert(timing_rank(To) > timing_rank(From),
            "insert adds timing information, so the destination has to "
            "carry more of it than the source.");
#ifdef FORSYDE_INTROSPECTION
        arg_vec.push_back(std::make_tuple("lambda", std::to_string(lambda)));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const
    {
        return std::string("MI::insert<") + moc_name(From) + "," + moc_name(To) + ">";
    }

private:
    unsigned long lambda;

    //! The event being placed, always absent-extended
    /*! An untimed source hands over a bare T, which is lifted on the way
     * in; a synchronous source already hands over an abst_ext<T>, and an
     * absent one stays absent -- an SY clock cycle in which nothing
     * happened becomes lambda DT ticks in which nothing happened.
     */
    abst_ext<T> val;

    void init() {val = abst_ext<T>();}

    void prep()
    {
        if constexpr (timing_rank(From) == 0)
            val = abst_ext<T>(iport1.read());
        else
            val = iport1.read();
    }

    void exec() {}

    void prod()
    {
        write_multiport(oport1, val);
        for (unsigned long i=1; i<lambda; i++)
            write_multiport(oport1, abst_ext<T>());
    }

    void clean() {}

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        boundInChans.resize(1);     // only one input port
        boundInChans[0].port = &iport1;
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
    }
#endif
};

//! Group timing information: lambda timed events into one synchronous event (Jantsch 6.3)
/*! The third of chapter 6's operations, and the only one that is not a
 * relabelling. strip and insert each leave the events alone and change
 * only what is said about when they happen; this one changes the events
 * themselves, because going from a timed signal to a synchronous one
 * means deciding what a whole clock cycle's worth of timed events
 * amounts to as a single synchronous value.
 *
 * Jantsch's answer (6.3) is lastt -- the last non-absent event of the
 * group, absent if all lambda of them are absent:
 *
 *      pi(nu, s)  = <a_i>,  nu(i)  = lambda
 *      pi(nu', s) = <e_i>,  nu'(i) = 1
 *      e_i = absent          if every event of a_i is absent
 *            lastt(a_i)      otherwise
 *
 * "Filters out all events except the last in a clock cycle", as chapter
 * 7 puts it when it uses this process to move a synchronous Mealy
 * machine into the timed domain.
 *
 * This is DT to SY, and only DT to SY -- which is worth saying plainly,
 * because it would be easy to reach for it at the SY-to-DDE boundary
 * instead. DDE is not the timed MoC of this ladder. Its events carry
 * their own tags and land wherever they land, so there is no fixed
 * number of them per clock cycle to group; SY2DDE and DDE2SY take a
 * physical sample_period for exactly that reason, and are a different
 * kind of interface that chapter 6 does not describe.
 */
template <moc_id From, moc_id To, typename T>
class group : public process
{
public:
    typename port_of<From,T>::in  iport1;   ///< port for the input channel
    typename port_of<To,T>::out   oport1;   ///< port for the output channel

    //! The constructor requires the module name and the event ratio
    group(sc_module_name _name,         ///< process name
          unsigned long lambda = 1      ///< input events per output event
         ) : process(_name), iport1("iport1"), oport1("oport1"), lambda(lambda)
    {
        static_assert(on_timing_ladder(From) && on_timing_ladder(To),
            "group is one of Jantsch's chapter 6 interfaces and is defined "
            "over the untimed/synchronous/timed ladder. DDE is not on it: "
            "its events are tagged rather than sampled, so there is no "
            "fixed number of them per clock cycle to group. Use the "
            "sample_period-parameterised DDE interfaces instead.");
        static_assert(timing_rank(From) > timing_rank(To) && timing_rank(To) > 0,
            "group takes a timed signal to a synchronous one. To leave the "
            "timing behind entirely, use MI::strip.");
#ifdef FORSYDE_INTROSPECTION
        arg_vec.push_back(std::make_tuple("lambda", std::to_string(lambda)));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const
    {
        return std::string("MI::group<") + moc_name(From) + "," + moc_name(To) + ">";
    }

private:
    unsigned long lambda;
    abst_ext<T> val;

    void init() {val = abst_ext<T>();}

    void prep()
    {
        // lastt: keep overwriting, so what survives the loop is the last
        // present event of the group, or absent if there was none
        val = abst_ext<T>();
        for (unsigned long i=0; i<lambda; i++)
        {
            auto tok = iport1.read();
            if (!is_absent(tok)) val = tok;
        }
    }

    void exec() {}

    void prod() {write_multiport(oport1, val);}

    void clean() {}

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        boundInChans.resize(1);     // only one input port
        boundInChans[0].port = &iport1;
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
    }
#endif
};


}


//! Operation modes for the SY2CT converter
enum A2DMode {LINEAR, HOLD};

//! Process constructor for a SY2CT MoC interfaces
/*! This class is used to build a MoC interface which converts an SY 
 * signal to a CT one. It can be used to implement digital-to-analog
 * converters. There are two operating modes which can be configured using
 * the initial values of the constructor:
 * - sample and hold
 * - linear interpolation
 */
class SY2CT : public process
{
public:
    SY::SY_in<CTTYPE> iport1;      ///< port for the input channel
	CT::CT_out oport1;              ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented function to it and writes the
     * results using the output port
     */
    SY2CT(sc_module_name _name,      ///< process name
          sc_time sample_period,     ///< The sampling period
          A2DMode op_mode = HOLD    ///< The operation mode
          ) : process(_name), iport1("iport1"), oport1("oport1"),
              sample_period(sample_period), op_mode(op_mode)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << sample_period;
        arg_vec.push_back(std::make_tuple("sample_period", ss.str()));
        ss.str("");
        ss << op_mode;
        arg_vec.push_back(std::make_tuple("op_mode", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "MI::SY2CT";}

private:
    sc_time sample_period;
	A2DMode op_mode;
    
    // Internal variables
    CTTYPE previousVal, currentVal;
    sub_signal subsig;
    unsigned long iter;
    
    //Implementing the abstract semantics
    void init()
    {
        currentVal = previousVal = 0;
        iter = 0;
    }
    
    void prep()
    {
        currentVal = (CTTYPE)from_abst_ext(iport1.read(), previousVal);
    }
    
    void exec()
    {
        set_range(subsig, sample_period*iter, sample_period*(iter+1));
        if(op_mode==HOLD)
        {
            CTTYPE pv = previousVal;
            set_function(subsig,[pv](const sc_time& t)
                                {
                                    return pv;
                                }
            );
        }
        else 
        {
            CTTYPE dv = currentVal - previousVal;
            CTTYPE pv = previousVal;
            unsigned long itr = iter;
            sc_time sp = sample_period;
            set_function(subsig,[pv,itr,sp,dv](const sc_time& t)
            {
                return (t-itr*sp)/sp*dv + pv;
            });
        }
    }
    
    void prod()
    {
        write_multiport(oport1, subsig);
        wait(get_end_time(subsig) - sc_time_stamp());
        iter++;
        previousVal = currentVal;
    }
    
    void clean() {}
    
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        boundInChans.resize(1);     // only one input port
        boundInChans[0].port = &iport1;
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
    }
#endif
};

//! Process constructor for a CT2SY MoC interface
/*! This class is used to build a MoC interface which converts an CT 
 * signal to a SY one with fixed sampling rate. It can be used to implement 
 * analog-to-digital converters.
 */
class CT2SY : public process
{
public:
    CT::CT_in iport1;           ///< port for the input channel
    SY::SY_out<CTTYPE> oport1; ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented function to it and writes the
     * results using the output port
     */
    CT2SY(sc_module_name _name,      ///< process name
          sc_time sample_period      ///< The sampling period
          ) : process(_name), iport1("iport1"), oport1("oport1"),
              sample_period(sample_period)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << sample_period;
        arg_vec.push_back(std::make_tuple("sample_period", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "MI::CT2SY";}

private:
    sc_time sample_period;
    
    // Internal variables
    sub_signal in_ss;
    CTTYPE out_val;
    sc_time local_time, sampling_time;
    
    //Implementing the abstract semantics
    void init()
    {
        local_time = sampling_time = SC_ZERO_TIME;
    }
    
    void prep()
    {
        while (sampling_time >= local_time)
        {
            in_ss = iport1.read();
            local_time = get_end_time(in_ss);
        }
    }
    
    void exec()
    {
        out_val = in_ss(sampling_time);
    }
    
    void prod()
    {
        write_multiport(oport1, out_val);
        wait(sampling_time - sc_time_stamp());
        sampling_time += sample_period;
    }
    
    void clean() {}
    
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        boundInChans.resize(1);     // only one input port
        boundInChans[0].port = &iport1;
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
    }
#endif
};

//! Process constructor for a CT2DDE MoC interface
/*! This class is used to build a MoC interface which converts an CT 
 * signal to a DDE one with adaptive sampling rate. It can be used to
 * implement analog-to-digital converters with adaptive sampling rates.
 */
template<class T>
class CT2DDE : public process
{
public:
    CT::CT_in iport1;               ///< port for the input channel
    DDE::DDE_in<unsigned int> iport2; ///< port for the sampling channel
    DDE::DDE_out<T> oport1;           ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented function to it and writes the
     * results using the output port
     */
    CT2DDE(sc_module_name _name       ///< process name
          ) : process(_name), iport1("iport1"), oport1("oport1")
    {}
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "MI::CT2DDE";}

private:    
    // Internal variables
    sub_signal f;
    std::vector<sub_signal > vecCTsignal; // a queue to be committed
    //~ sub_signal in_val;
    abst_ext<T> out_val;
    sc_time samplingT;
    unsigned int samplingType, iter;
    
    //Implementing the abstract semantics
    void init()
    {
        iter = 0;
        //~ in_val = iport1.read();
        //~ cur_time = get_start_time(in_val);
    }
    
    void prep()
    {
        //~ while (cur_time >= get_end_time(in_val)) in_val = iport1.read();
        auto e = iport2.read();
        samplingType = unsafe_from_abst_ext(get_value(e)); // FIXME: what if absent?
        samplingT = get_time(e);
    }
    
    void exec() {}
    
    void prod()
    {
        //FIXME: this code should be split between prep, prod, and probably exec
        if(samplingType!=1)
        { 
            // just sampling (without commitment) in 
            // adaptive mode '0',  or non-adapitve mode '2'
            if(iter==0)
            { 
                f = iport1.read();
                if(samplingType==0)
                    vecCTsignal.push_back(f);
            }
            if((samplingT >= get_start_time(f)) && (samplingT < get_end_time(f)))
            {
                write_multiport(oport1,ttn_event<T>(f(samplingT), samplingT));
                wait(samplingT - sc_time_stamp());
            }
            else if(samplingT >= get_end_time(f))
            {
                f = iport1.read();
                if(samplingType==0)
                    vecCTsignal.push_back(f);
                if ((samplingT >= get_start_time(f)) && (samplingT < get_end_time(f)))
                {
                    write_multiport(oport1,ttn_event<T>(f(samplingT), samplingT));
                    wait(samplingT - sc_time_stamp());
                }
                else
                {
                    while(samplingT >= get_end_time(f))
                    {
                        f = iport1.read();
                        if(samplingType==0)
                            vecCTsignal.push_back(f);
                    }
                    write_multiport(oport1,ttn_event<T>(f(samplingT), samplingT));
                    wait(samplingT - sc_time_stamp());
                }
            }
            else
            {
                // To check the sampling from the queue
                while(!vecCTsignal.empty())
                {
                    if(samplingT >= get_end_time(vecCTsignal.front()))
                        vecCTsignal.erase(vecCTsignal.begin());
                    else
                    {
                        write_multiport(oport1,ttn_event<T>(vecCTsignal.front()(samplingT), samplingT));
                        wait(samplingT - sc_time_stamp());
                        break;
                    }
                }
                if(vecCTsignal.empty())
                    assert(0);  // if have not get the sampling
            }
        }
        else
        {
            // a commitment event in adaptive mode '1'
            while(!vecCTsignal.empty())
            {
                if(samplingT >= get_end_time(vecCTsignal.front()))
                    vecCTsignal.erase(vecCTsignal.begin());
                else
                    break;
            }
            if(vecCTsignal.empty())
                assert(0);  // if have not get the sampling
        }
        iter++;
    }
    
    void clean() {}
    
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        boundInChans.resize(2);     // only one input port
        boundInChans[0].port = &iport1;
        boundInChans[1].port = &iport2;
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
    }
#endif
};

//! Process constructor for a CT2DDEf MoC interface
/*! This class is used to build a MoC interface which converts a CT 
 * signal to a DDE one with fixed sampling rate. It can be used to
 * implement analog-to-digital converters with fixed sampling rates.
 */
template<class T>
class CT2DDEf : public process
{
public:
    CT::CT_in iport1;               ///< port for the input channel
    DDE::DDE_out<T> oport1;           ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented function to it and writes the
     * results using the output port
     */
    CT2DDEf(sc_module_name _name,    ///< process name
           sc_time samp_period      ///< sampling period
          ) : process(_name), iport1("iport1"), oport1("oport1"), samp_period(samp_period)
    {}
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "MI::CT2DDEf";}

private:    
    // Internal variables
    sc_time samp_period;
    abst_ext<T> out_val;
    sc_time local_time, sampling_time;
    sub_signal in_ss;
    
    //Implementing the abstract semantics
    void init()
    {
        local_time = sampling_time = SC_ZERO_TIME;
    }
    
    void prep()
    {
        while (sampling_time >= local_time)
        {            
            in_ss = iport1.read();
            local_time = get_end_time(in_ss);
        }
    }
    
    void exec()
    {
        out_val = abst_ext<T>(in_ss(sampling_time));
    }
    
    void prod()
    {
        write_multiport(oport1,ttn_event<T>(out_val, sampling_time));
        wait(sampling_time - sc_time_stamp());
        sampling_time += samp_period;
    }
    
    void clean() {}
    
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        boundInChans.resize(1);     // only one input port
        boundInChans[0].port = &iport1;
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
    }
#endif
};

//! Process constructor for a DDE2CT MoC interfaces
/*! This class is used to build a MoC interfaces which converts a DDE 
 * signal to a CT one. It can be used to implement digital-to-analog
 * converters. There are two operating modes which can be configured using
 * the initial values of the constructor:
 * - sample and hold
 * - linear interpolation
 */
template<class T>
class DDE2CT : public process
{
public:
    DDE::DDE_in<T> iport1;        ///< port for the input channel
	CT::CT_out oport1;          ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented function to it and writes the
     * results using the output port
     */
    DDE2CT(sc_module_name _name,      ///< process name
          A2DMode op_mode = HOLD    ///< The operation mode
          ) : process(_name), iport1("iport1"), oport1("oport1"),
              op_mode(op_mode)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << op_mode;
        arg_vec.push_back(std::make_tuple("op_mode", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "MI::DDE2CT";}

private:
	A2DMode op_mode;
    
    // Internal variables
    CTTYPE previousVal, currentVal;
    sc_time previousT, currentT;
    sub_signal subsig;
    
    //Implementing the abstract semantics
    void init()
    {
        previousVal = currentVal = 0;
        previousT = currentT = SC_ZERO_TIME;
    }
    
    void prep()
    {
        while (currentT <= previousT)
        {
            auto in_ev = iport1.read();
            currentVal = (double)from_abst_ext(get_value(in_ev), previousVal);
            currentT = get_time(in_ev);
        }
    }
    
    void exec()
    {
        set_range(subsig, previousT, currentT);
        if(op_mode==HOLD)
        {
            CTTYPE pv = previousVal;
            set_function(subsig,[=](sc_time t){
						return pv;
						});
        }
        else 
        {
            CTTYPE dv = currentVal - previousVal;
            sc_time dt = currentT - previousT;
            sc_time pt = previousT;
            CTTYPE pv = previousVal;
            set_function(subsig,[=](sc_time t)->CTTYPE{
                    return ((t-pt)/dt*dv + pv);
            });
        }
    }
    
    void prod()
    {
        write_multiport(oport1, subsig);
        wait(get_end_time(subsig) - sc_time_stamp());
        previousVal = currentVal;
        previousT = currentT;
    }
    
    void clean() {}
    
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        boundInChans.resize(1);     // only one input port
        boundInChans[0].port = &iport1;
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
    }
#endif
};

//! The synchronous-to-untimed MoC interface, as a name
/*! It is MI::strip<SY,SDF> now. The hand-written version dropped absent
 * events correctly and then wrote a member it had never assigned, so
 * every token it emitted was an uninitialised value. No model used it,
 * so nothing found out. See MI::strip.
 */
template <typename T>
using SY2SDF = MI::strip<moc_id::SY, moc_id::SDF, T>;

//! The untimed-to-synchronous MoC interface, as a name
/*! It is MI::insert<SDF,SY> now, whose lambda defaults to the one token
 * per tick this hard-coded and would not let you change.
 */
template <typename T>
using SDF2SY = MI::insert<moc_id::SDF, moc_id::SY, T>;

//! The synchronous-to-timed MoC interface (Jantsch's insertS2T, 6.6)
/*! New, and the pair below with it. DT had no MoC interface of any kind
 * before: not to SY, not to the untimed MoCs, not to CT. It could not be
 * entered or left, which is a large part of why so little of the library
 * uses it.
 *
 * Each SY event becomes one DT event followed by lambda-1 absent ones,
 * so lambda is the number of DT ticks in one SY clock cycle. That is the
 * only information the conversion needs, and it is a count rather than a
 * duration, because a DT tick is the time base rather than a sample of
 * one.
 */
template <typename T>
using SY2DT = MI::insert<moc_id::SY, moc_id::DT, T>;

//! The timed-to-synchronous MoC interface (Jantsch's stripT2S, 6.3)
/*! The inverse of SY2DT: lambda DT ticks make one SY clock cycle, and
 * the cycle's value is the last event present in it. See MI::group.
 */
template <typename T>
using DT2SY = MI::group<moc_id::DT, moc_id::SY, T>;

//! Process constructor for a SY2DDE MoC interfaces
/*! This class is used to build a MoC interface which converts an SY 
 * signal to a DDE one.
 */
template<class T>
class SY2DDE : public process
{
public:
    SY::SY_in<T> iport1;        ///< port for the input channel
	DDE::DDE_out<T> oport1;       ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented function to it and writes the
     * results using the output port
     */
    SY2DDE(sc_module_name _name,     ///< process name
          sc_time sample_period     ///< The unified period length
          ) : process(_name), iport1("iport1"), oport1("oport1"),
              sample_period(sample_period)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << sample_period;
        arg_vec.push_back(std::make_tuple("sample_period", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "MI::SY2DDE";}

private:
    sc_time sample_period;
    
    // Internal variables
    abst_ext<T>* tok;
    T* val;
    sc_time cur_time;
    
    //Implementing the abstract semantics
    void init()
    {
        tok = new abst_ext<T>();
        val = new T;
        cur_time = SC_ZERO_TIME;
    }
    
    void prep()
    {
        *tok = iport1.read();
        if (is_present(*tok))
            *val = unsafe_from_abst_ext(*tok);
    }
    
    void exec() {}
    
    void prod()
    {
        write_multiport(oport1, tt_event<T>(*val,cur_time));
        wait(cur_time - sc_time_stamp());
        cur_time += sample_period;
    }
    
    void clean()
    {
        delete val;
        delete tok;
    }
    
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        boundInChans.resize(1);     // only one input port
        boundInChans[0].port = &iport1;
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
    }
#endif
};

//! Process constructor for a DDE2SY MoC interface
/*! This class is used to build a MoC interface which converts a DDE 
 * signal to an SY one.
 */
template<class T>
class DDE2SY : public process
{
public:
    DDE::DDE_in<T> iport1;  ///< port for the input channel
    SY::SY_out<T> oport1;   ///< port for the output channel

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented function to it and writes the
     * results using the output port
     */
    DDE2SY(sc_module_name _name,     ///< process name
          sc_time sample_period     ///< The unified period length
          ) : process(_name), iport1("iport1"), oport1("oport1"),
              sample_period(sample_period)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << sample_period;
        arg_vec.push_back(std::make_tuple("sample_period", ss.str()));
#endif
    }
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "MI::DDE2SY";}

private:
    sc_time sample_period;
    
    // Internal variables
    tt_event<T>* tok;
    T* prev_val;
    sc_time cur_time;
    
    //Implementing the abstract semantics
    void init()
    {
        tok = new tt_event<T>();
        prev_val = new T;
        cur_time = SC_ZERO_TIME;
        *tok = iport1.read();
    }
    
    void prep()
    {
        while (get_time(*tok) <= cur_time)
        {
            *prev_val = get_value(*tok);
            *tok = iport1.read();
        }
    }
    
    void exec() {}
    
    void prod()
    {
        write_multiport(oport1, abst_ext<T>(*prev_val));
        cur_time += sample_period;
    }
    
    void clean() {}
    
#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        boundInChans.resize(1);     // only one input port
        boundInChans[0].port = &iport1;
        boundOutChans.resize(1);    // only one output port
        boundOutChans[0].port = &oport1;
    }
#endif
};

}

#endif
