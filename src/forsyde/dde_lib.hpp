/**********************************************************************
    * dde_lib.hpp -- a library of components in the DDE MoC           *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Providing a library of useful DDE components           *
    *                                                                 *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef DDE_LIB_HPP
#define DDE_LIB_HPP

/*! \file dde_lib.hpp
 * \brief Implements a library of components in the DDE MoC
 *
 *  These are *components*, not process constructors. A process
 * constructor is parameterised by a user function and says nothing about
 * what that function computes; a component is a specific, named piece of
 * behaviour. filter and filterf solve a Laplace transfer function given
 * by its numerator and denominator constants, which is one fixed
 * computation, so they belong here beside the other libraries --
 * ct_lib.hpp's scale, add and mul, sy_lib.hpp's -- rather than among the
 * MoC's primitives in dde_process_constructors.hpp, where they were
 * about a third of the file.
 *
 *  They stay in the DDE MoC. A numerical solver takes discrete steps, so
 * expressing one over time-tagged events is the natural fit; CT::filter
 * is a primitive of the CT MoC that is built on this one, between a
 * CT2DDE and a DDE2CT interface.
 */

#include <vector>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/lu.hpp>
#include <boost/numeric/ublas/io.hpp>

#include "abssemantics.hpp"
#include "dde_process.hpp"
#include "dde_process_constructors.hpp"

namespace ForSyDe
{

namespace DDE
{

using namespace sc_core;
using namespace boost::numeric::ublas;

//! A linear filter with an adaptive time step
/*! Solves the transfer function given by its numerator and denominator
 * constants, adapting the step size to hold the local error under
 * tol_error between min_step and max_step.
 */
template <class T>
class filter : public dde_process,
               public ForSyDe::detail::bindable<filter<T>>
{
public:
    DDE_in<T>  iport1;           ///< port for the input channel
    DDE_out<T> oport1;           ///< port for the output channel
    DDE_out<unsigned int> oport2;///< port for the sampling signal

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<filter<T>>::operator();
    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1,oport2);}

    typedef matrix<T> MatrixDouble;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which inserts the initial element, reads
     * data from its input port, and writes the results using the output
     * port.
     */
    filter(sc_module_name _name,             ///< process name
            std::vector<T> numerators,       ///< Numerator constants
            std::vector<T> denominators,     ///< Denominator constants
            sc_time max_step,                ///< Maximum time step
            sc_time min_step=sc_time(0.05,SC_NS),///< Minimum time step
            T tol_error=1e-5                 ///< Tolerated error
          ) : dde_process(_name), iport1("iport1"), oport1("oport1"),
              numerators(numerators), denominators(denominators),
              max_step(max_step), min_step(min_step), tol_error(tol_error)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << numerators;
        arg_vec.push_back(std::make_tuple("numerators", ss.str()));
        ss.str("");
        ss << denominators;
        arg_vec.push_back(std::make_tuple("denominators", ss.str()));
        ss.str("");
        ss << max_step;
        arg_vec.push_back(std::make_tuple("max_step", ss.str()));
        ss.str("");
        ss << min_step;
        arg_vec.push_back(std::make_tuple("min_step", ss.str()));
        ss.str("");
        ss << tol_error;
        arg_vec.push_back(std::make_tuple("tol_error", ss.str()));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DDE::filter";}

private:
    // Constructor parameters
    std::vector<T> numerators, denominators;
    sc_time max_step, min_step;
    T tol_error;

    // Internal variables
    sc_time step;
    sc_time samplingTimeTag;
    MatrixDouble a, b, c, d;
    // states
    MatrixDouble x, x0, x1, x2;
    // current and previous input/time.
    MatrixDouble u, u0, u1, u_1;
    sc_time t, t_1, t2, h;
    // output
    MatrixDouble y0, y1, y2;
    // Some helper matrices used in RK solver
    MatrixDouble k1,k2,k3,k4;
    // to prevent rounding error
    double roundingFactor;

    // Output event
    ttn_event<T>* out_ev;

    //Implementing the abstract semantics
    void init()
    {
        out_ev = new ttn_event<T>;

        step = max_step;
        int /*nn = nums.size(),*/ nd = denominators.size();
        a = MatrixDouble(nd-1,nd-1);
        b = MatrixDouble(nd-1,1);
        c = MatrixDouble(1,nd-1);
        d = MatrixDouble(1,1);

        tf2ss(numerators,denominators,a,b,c,d);

        // State number
        int numState = a.size1();
        assert(a.size1() == a.size2());
        samplingTimeTag = SC_ZERO_TIME;
        x = zero_matrix<T>(numState,1);
        u = MatrixDouble(1,1), u_1 = MatrixDouble(1,1);
        u0 = u1 = MatrixDouble(1,1);
        y1 = MatrixDouble(1,1);
        k1 = MatrixDouble(numState,1);
        k2 = MatrixDouble(numState,1);
        k3 = MatrixDouble(numState,1);
        k4 = MatrixDouble(numState,1);

        // initial sampling time tag
        write_multiport(oport2,ttn_event<unsigned int>(0, samplingTimeTag));
        // read initial input
        auto in_ev = iport1.read();
        u(0,0) = unsafe_from_abst_ext(get_value(in_ev)); // FIXME: assumes non-null inputs
        t = get_time(in_ev);
        // calculate and write initial output
        y1 = boost::numeric::ublas::prod(c,x) + boost::numeric::ublas::prod(d,u);
        *out_ev = ttn_event<T>(y1(0,0), t);
        write_multiport(oport1, *out_ev);
        // step signal
        write_multiport(oport2, ttn_event<unsigned int>(0, samplingTimeTag+step/2));
        write_multiport(oport2, ttn_event<unsigned int>(0, samplingTimeTag+step));
        u_1(0,0) = u(0,0);
        t_1 = t;
        roundingFactor = 1.0001;
    }

    void prep()
    {
        auto in_ev = iport1.read();
        u1(0,0) = unsafe_from_abst_ext(get_value(in_ev)); // FIXME: assumes non-null inputs
        t = get_time(in_ev);

        in_ev = iport1.read();
        u0(0,0) = unsafe_from_abst_ext(get_value(in_ev)); // FIXME: assumes non-null inputs
        t2 = get_time(in_ev);
    }

    void exec()
    {
        // 1st step error estimation
        h = t - t_1;
        rkSolver(a, b, c, d, u1, u_1, x, h.to_seconds(), x1, y1);

        // regular RK
        h = t2 - t_1;
        rkSolver(a, b, c, d, u0, u_1, x, h.to_seconds(), x0, y0);

        // 2nd step error estimation
        rkSolver(a, b, c, d, u0, u1, x1, (h/2).to_seconds(), x2, y2);

        // error estimation
        double err_est = (double) std::abs(y2(0,0)-y0(0,0))/(h.to_seconds());
        if( (err_est < tol_error) || (h<=roundingFactor*min_step)) {
          x = x0;
          samplingTimeTag = t;
          // TODO: move the following line to the prod stage
          write_multiport(oport2, ttn_event<unsigned int>(1, samplingTimeTag)); // commitment
          *out_ev = ttn_event<T>(y0(0,0), t);
          write_multiport(oport1, *out_ev);
          u(0,0) = u0(0,0);
          u_1(0,0) = u(0,0);
          t_1 = t;
          if(h==min_step)
            std::cout << "Step accepted due to minimum step size. "
             << "However, err_tol is not met." << std::endl;
          // Recover the full step for the next interval: a reduction
          // below is a response to one hard interval, not a permanent
          // downgrade of the solver.
          step = max_step;
        } else {
          // The step was rejected -- and until this branch existed,
          // that meant the solver hung. Nothing here updated
          // samplingTimeTag, t_1 or step, so prod() below re-requested
          // the *same* two sample times, prep() read back the same two
          // inputs, and exec() recomputed the same failing error
          // estimate, forever, at 100% CPU and a standing simulated
          // time. The step size this whole routine is written around --
          // note min_step, max_step, and the h<=roundingFactor*min_step
          // guard above, which can only ever be reached once the step
          // actually shrinks -- was assigned once in init() and never
          // touched again, so "adaptive" step control did not adapt.
          //
          // Halve it, floored at min_step, and let prod() re-request the
          // interval at the finer spacing. Termination is guaranteed by
          // the guard above: once h reaches min_step the step is
          // force-accepted with the warning already written for it.
          step = step / 2;
          if (step < min_step) step = min_step;
        }
    }

    // NOTE -- this is the one DDE process that does not advance its local
    // clock. Every other one now ends its firing with wait_until() on the
    // latest time it has complete input information for, which for a
    // one-input process is the tag it just consumed. This one consumes
    // *two* events per firing, at t and t2, and also keeps its own
    // sampling clock in samplingTimeTag, which the adaptive step moves
    // independently of either. So there are three defensible answers
    // here -- t, t2, or samplingTimeTag -- and they are not the same
    // number once the step adapts. Picking one is a decision about what
    // a solver's local time means, not a consistency fix, and it changes
    // the timing of every model that filters, so it is left alone and
    // recorded instead.
    void prod()
    {
        write_multiport(oport2, ttn_event<unsigned int>(0, samplingTimeTag+step/2));
        write_multiport(oport2, ttn_event<unsigned int>(0, samplingTimeTag+step));
    }

    void clean()
    {
        delete out_ev;
    }

    // To obtain state space matrices from transfer function.
    // We assume there are non-zero leading coefficients in num and denom.
    int tf2ss(std::vector<T> & num_, std::vector<T> & den_, MatrixDouble & a,
          MatrixDouble & b, MatrixDouble & c, MatrixDouble & d)
    {
        std::vector<T> num, den;
        // sizes checking
        int nn = num_.size(), nd = den_.size();
        if(nn >= nd)
        {
            std::cerr << "ERROR: " << "degree(num) = " << nn
            << " >= degree(denom) = " << nd << std::endl;
            abort();
        }
        T dCoef1 = den_.at(0);
        if(nd==1)
        {
            //~ a = NULL, b = NULL, c = NULL;
            d = MatrixDouble(1,1);
            d(0,0) = num_.at(0)/dCoef1;
        }
        else
        {
            if ((nd - nn) > 0)
            {
                // Pad num so that degree(num) == degree(denom)
                for(int i=0; i<nd; i++)
                {
                    if(i<(nd-nn))
                        num.push_back(0.0);
                    else
                        num.push_back(num_.at(i-nd+nn));
                }
            }

            // Normalizing w.r.t the leading coefficient of denominator
            for(unsigned int i=0; i<num.size(); i++)
                num.at(i) /= dCoef1;
            for(unsigned int i=0; i<den_.size(); i++)
                den_.at(i) /= dCoef1;
            for(unsigned int i=0; i<(den_.size()-1); i++)
                den.push_back(den_.at(i+1));

            // Form A (nd-1)*(nd-1)
            a = zero_matrix<T> (a.size1(), a.size2());
            if(nd > 2)
            {
                // The eyes (up-right corner) are set to '1'
                for(int i=0; i<(nd-2); i++)
                    for(int j=0; j<(nd-1); j++)
                        if((j-i)==1) a(i,j) = 1.0;
                // The lower row(s)
                for(int j=0; j<(nd-1); j++)
                    a(nd-2,j) = 0-den.at(nd-2-j);
            }
            else
                a(0,0) = 0-den.at(0);
            //
            // Form B (nd-1)*1
            b = zero_matrix<T> (b.size1(), b.size2());
            b(nd-2,0) = 1.0;
            //
            // Form C 1*(nd-1)
            for(int j=0; j< nd-1; j++)
                c(0,nd-2-j) = num.at(j+1) - num.at(0)*den.at(j);
            //
            // Form D 1*1
            d(0,0) = num.at(0);
        }

        return 0;
    }

    void rkSolver(MatrixDouble a, MatrixDouble b, MatrixDouble c,
                  MatrixDouble d, MatrixDouble u_k, MatrixDouble u_k_1,
                  MatrixDouble x, T h, MatrixDouble &x_, MatrixDouble &y)
    {
        // Some helper matrices used in RK solver
        MatrixDouble k1,k2,k3,k4;
        k1 = boost::numeric::ublas::prod(a,x) + boost::numeric::ublas::prod(b,u_k_1);
        k2 = boost::numeric::ublas::prod(a,(x+k1*(h/2.0))) + boost::numeric::ublas::prod(b,(u_k_1 + u_k)) * 0.5;
        k3 = boost::numeric::ublas::prod(a,(x+k2*(h/2.0))) + boost::numeric::ublas::prod(b,(u_k_1 + u_k)) * 0.5;
        k4 = boost::numeric::ublas::prod(a,(x+k3*h)) + boost::numeric::ublas::prod(b,u_k);
        x_ = x + (k1 + 2.0*k2 + 2.0*k3 + k4) * (h/6.0);
        y = boost::numeric::ublas::prod(c,x_) + boost::numeric::ublas::prod(d,u_k);
    }

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

//! A linear filter with a fixed time step
/*! As filter, but takes one step of a fixed size rather than adapting.
 */
template <class T>
class filterf : public dde_process,
                public ForSyDe::detail::bindable<filterf<T>>
{
public:
    DDE_in<T>  iport1;           ///< port for the input channel
    DDE_out<T> oport1;           ///< port for the output channel

    //! Bind signals positionally: outputs first, then inputs
    using ForSyDe::detail::bindable<filterf<T>>::operator();
    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}

    typedef matrix<T> MatrixDouble;

    //! The constructor requires the module name
    /*! It creates an SC_THREAD which inserts the initial element, reads
     * data from its input port, and writes the results using the output
     * port.
     */
    filterf(sc_module_name _name,           ///< process name
            std::vector<T> numerators,      ///< Numerator constants
            std::vector<T> denominators     ///< Denominator constants
          ) : dde_process(_name), iport1("iport1"), oport1("oport1"),
              numerators(numerators), denominators(denominators)
    {
#ifdef FORSYDE_INTROSPECTION
        std::stringstream ss;
        ss << numerators;
        arg_vec.push_back(std::make_tuple("numerators", ss.str()));
        ss.str("");
        ss << denominators;
        arg_vec.push_back(std::make_tuple("denominators", ss.str()));
#endif
    }

    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const {return "DDE::filterf";}

private:
    // Constructor parameters
    std::vector<T> numerators, denominators;

    // Internal variables
    MatrixDouble a, b, c, d;
    // states
    MatrixDouble x, x_1;
    // current and previous input/time.
    MatrixDouble u, u_1;
    sc_time t, t_1, h;
    // output
    MatrixDouble y;
    // Some helper matrices used in RK solver
    MatrixDouble k1,k2,k3,k4;

    // Output event
    ttn_event<T>* out_ev;

    //Implementing the abstract semantics
    void init()
    {
        out_ev = new ttn_event<T>;

        int /*nn = nums.size(),*/ nd = denominators.size();
        a = MatrixDouble(nd-1,nd-1);
        b = MatrixDouble(nd-1,1);
        c = MatrixDouble(1,nd-1);
        d = MatrixDouble(1,1);

        tf2ss(numerators,denominators,a,b,c,d);

        // State number
        int numState = a.size1();
        assert(a.size1() == a.size2());
        x = zero_matrix<T>(numState,1), x_1 = zero_matrix<T>(numState,1);
        u = MatrixDouble(1,1), u_1 = MatrixDouble(1,1);
        y = MatrixDouble(1,1);
        k1 = MatrixDouble(numState,1);
        k2 = MatrixDouble(numState,1);
        k3 = MatrixDouble(numState,1);
        k4 = MatrixDouble(numState,1);

        // read initial input
        auto in_ev = iport1.read();
        u(0,0) = unsafe_from_abst_ext(get_value(in_ev)); // FIXME: assumes non-absent inputs
        t = get_time(in_ev);
        // calculate and write initial output
        y = boost::numeric::ublas::prod(c,x) + boost::numeric::ublas::prod(d,u);
        *out_ev = ttn_event<T>(y(0,0), t);
        write_multiport(oport1, *out_ev);
        wait_until(t, name());
        u_1(0,0) = u(0,0);
        t_1 = t;
    }

    void prep()
    {
        auto in_ev = iport1.read();
        u(0,0) = unsafe_from_abst_ext(get_value(in_ev)); // FIXME: assumes non-absent inputs
        t = get_time(in_ev);
    }

    void exec()
    {
        // 1st step error estimation
        h = t - t_1;
        rkSolver(a, b, c, d, u, u_1, x_1, h.to_seconds(), x, y);
        *out_ev = ttn_event<T>(y(0,0), t);
    }

    void prod()
    {
        write_multiport(oport1, *out_ev);
        wait_until(t, name());
        x_1 = x;
        u_1(0,0) = u(0,0);
        t_1 = t;
    }

    void clean()
    {
        delete out_ev;
    }

    // To obtain state space matrices from transfer function.
    // We assume there are non-zero leading coefficients in num and denom.
    int tf2ss(std::vector<T> & num_, std::vector<T> & den_, MatrixDouble & a,
          MatrixDouble & b, MatrixDouble & c, MatrixDouble & d)
    {
        std::vector<T> num, den;
        // sizes checking
        int nn = num_.size(), nd = den_.size();
        if(nn >= nd)
        {
            std::cerr << "ERROR: " << "degree(num) = " << nn
            << " >= degree(denom) = " << nd << std::endl;
            abort();
        }
        T dCoef1 = den_.at(0);
        if(nd==1)
        {
            //~ a = NULL, b = NULL, c = NULL;
            d = MatrixDouble(1,1);
            d(0,0) = num_.at(0)/dCoef1;
        }
        else
        {
            if ((nd - nn) > 0)
            {
                // Pad num so that degree(num) == degree(denom)
                for(int i=0; i<nd; i++)
                {
                    if(i<(nd-nn))
                        num.push_back(0.0);
                    else
                        num.push_back(num_.at(i-nd+nn));
                }
            }

            // Normalizing w.r.t the leading coefficient of denominator
            for(unsigned int i=0; i<num.size(); i++)
                num.at(i) /= dCoef1;
            for(unsigned int i=0; i<den_.size(); i++)
                den_.at(i) /= dCoef1;
            for(unsigned int i=0; i<(den_.size()-1); i++)
                den.push_back(den_.at(i+1));

            // Form A (nd-1)*(nd-1)
            a = zero_matrix<T> (a.size1(), a.size2());
            if(nd > 2)
            {
                // The eyes (up-right corner) are set to '1'
                for(int i=0; i<(nd-2); i++)
                    for(int j=0; j<(nd-1); j++)
                        if((j-i)==1) a(i,j) = 1.0;
                // The lower row(s)
                for(int j=0; j<(nd-1); j++)
                    a(nd-2,j) = 0-den.at(nd-2-j);
            }
            else
                a(0,0) = 0-den.at(0);
            //
            // Form B (nd-1)*1
            b = zero_matrix<T> (b.size1(), b.size2());
            b(nd-2,0) = 1.0;
            //
            // Form C 1*(nd-1)
            for(int j=0; j< nd-1; j++)
                c(0,nd-2-j) = num.at(j+1) - num.at(0)*den.at(j);
            //
            // Form D 1*1
            d(0,0) = num.at(0);
        }

        return 0;
    }

    void rkSolver(MatrixDouble a, MatrixDouble b, MatrixDouble c,
                  MatrixDouble d, MatrixDouble u_k, MatrixDouble u_k_1,
                  MatrixDouble x, T h, MatrixDouble &x_, MatrixDouble &y)
    {
        // Some helper matrices used in RK solver
        MatrixDouble k1,k2,k3,k4;
        k1 = boost::numeric::ublas::prod(a,x) + boost::numeric::ublas::prod(b,u_k_1);
        k2 = boost::numeric::ublas::prod(a,(x+k1*(h/2.0))) + boost::numeric::ublas::prod(b,(u_k_1 + u_k)) * 0.5;
        k3 = boost::numeric::ublas::prod(a,(x+k2*(h/2.0))) + boost::numeric::ublas::prod(b,(u_k_1 + u_k)) * 0.5;
        k4 = boost::numeric::ublas::prod(a,(x+k3*h)) + boost::numeric::ublas::prod(b,u_k);
        x_ = x + (k1 + 2.0*k2 + 2.0*k3 + k4) * (h/6.0);
        y = boost::numeric::ublas::prod(c,x_) + boost::numeric::ublas::prod(d,u_k);
    }

#ifdef FORSYDE_INTROSPECTION
    void bindInfo()
    {
        ForSyDe::detail::record_ports(boundInChans, in_ports());
        ForSyDe::detail::record_ports(boundOutChans, out_ports());
    }
#endif
};

}
}

#endif
