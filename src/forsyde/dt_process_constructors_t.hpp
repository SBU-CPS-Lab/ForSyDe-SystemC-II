/**********************************************************************           
    * dt_process_constructors_t.hpp -- Process constructors in the DT *
    *                    MOC with event count and timout              *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Providing basic process constructors for modeling      *
    *          discrete-time systems in ForSyDe-SystemC               *
    *                                                                 *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef DT_PROCESS_CONSTRUCTORS_T_HPP
#define DT_PROCESS_CONSTRUCTORS_T_HPP

/*! \file dt_process_constructors.hpp
 * \brief Implements the basic process constructors in the DT MoC with event count and timeout
 * 
 *  This file includes the basic process constructors used for modeling
 * in the discrete-time model of computation.
 */

#include <systemc>
#include <functional>
#include <tuple>

#include "abst_ext.hpp"
#include "dt_process.hpp"

namespace ForSyDe
{

namespace DT
{

// Was mistakenly "namespace S" -- a copy-paste of
// dt_process_constructors_s.hpp that was never corrected, which combined
// with the identical include guard both files used to carry (see the
// D2 fix above) to make DT::T::mealy unreachable: dt_moc.hpp includes
// _p.hpp first, so _s.hpp and _t.hpp's *bodies* were never even
// parsed, and had they been, this would have defined its mealy inside
// DT::S right on top of _s.hpp's own, not DT::T.
namespace T
{

using namespace sc_core;

//! Process constructor for a mealy machine
/*! This class is used to build a timed Mealy state machine.
 * Given a partitioning function, a next-state function, an output decoding
 * function, and an initial state, it creates a timed Mealy process.
 */
template <class IT, class ST, class OT>
class mealy : public detail::fsm_core<mealy<IT,ST,OT>,
                                    std::tuple<std::vector<OT>>,
                                    std::tuple<std::vector<IT>>, ST>
{
    typedef detail::fsm_core<mealy<IT,ST,OT>,
                             std::tuple<std::vector<OT>>,
                             std::tuple<std::vector<IT>>, ST> base;
    friend base;
public:
    DT_in<IT>  iport1;        ///< port for the input channel
    DT_out<OT> oport1;        ///< port for the output channel
    
    //! Type of the partitioning and timeout specification function to be passed to the process constructor
    typedef std::function<void(size_t&, size_t&, const ST&)> gamma_functype;
    
    //! Type of the next-state function to be passed to the process constructor
    typedef std::function<void(ST&, 
                                const ST&,
                                const std::vector<IT>&)> ns_functype;
    
    //! Type of the output-decoding function to be passed to the process constructor
    typedef std::function<void(std::vector<OT>&, 
                                const ST&,
                                const std::vector<IT>&)> od_functype;
    
    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented functions to the input and current
     * state and writes the results using the output port
     */
    mealy(sc_module_name _name,    ///< The module name
           gamma_functype _gamma_func,    ///< The input partitioning function
           ns_functype _ns_func,    ///< The next_state function
           od_functype _od_func,    ///< The output-decoding function
           ST init_st               ///< Initial state
          ) : base(_name, init_st), iport1("iport1"), oport1("oport1"), _gamma_func(_gamma_func), _ns_func(_ns_func),
              _od_func(_od_func) {}
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "DT::T::mealy";}
    
private:
    //! The functions passed to the process constructor
    gamma_functype _gamma_func;
    ns_functype _ns_func;
    od_functype _od_func;
    std::size_t timeout;

public:
    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}
private:

    // mealyTT: as S, but gamma also gives a time period after which the
    // process fires whatever the count has reached.
    //
    // `read` is counted rather than inferred. This used to add the final
    // itoks to tin -- the number of tokens the process *would* have
    // consumed had the count been reached. When the time-out cuts the
    // cycle short those are not the same number, so tin ran ahead of what
    // was actually read, which inflates K_i and pads the output with
    // absent events that do not belong there.
    std::size_t read_inputs()
    {
        std::size_t itoks;
        _gamma_func(itoks, timeout, this->stval);
        auto& in = std::get<0>(this->ivals);
        in.clear();
        std::size_t read{0};
        for (std::size_t i=0; i<itoks && i<timeout; i++)
        {
            auto tmp = iport1.read();
            read++;
            if (is_present(tmp))
                in.push_back(unsafe_from_abst_ext(tmp));
            else
                itoks++;    // read one more token for each absent event
        }
        return read;
    }

    void exec()
    {
        _ns_func(this->nsval, this->stval, std::get<0>(this->ivals));
        _od_func(std::get<0>(this->ovals), this->stval, std::get<0>(this->ivals));
        this->stval = this->nsval;
    }
};


}
}
}

#endif
