/**********************************************************************           
    * dt_process_constructors.hpp -- Process constructors in the DT   *
    *                          MOC with timer based process invocation*
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

#ifndef DT_PROCESS_CONSTRUCTORS_P_HPP
#define DT_PROCESS_CONSTRUCTORS_P_HPP

/*! \file dt_process_constructors.hpp
 * \brief Implements the basic process constructors in the DT MoC with timer invocation
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

namespace P
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
    
    //! Type of the partitioning function to be passed to the process constructor
    typedef std::function<void(size_t&, const ST&)> gamma_functype;
    
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
    std::string forsyde_kind() const{return "DT::P::mealy";}
    
private:
    //! The functions passed to the process constructor
    gamma_functype _gamma_func;
    ns_functype _ns_func;
    od_functype _od_func;

    auto in_ports()  {return std::tie(iport1);}
    auto out_ports() {return std::tie(oport1);}

    // mealyPT: gamma is a time period; the interface strips the absent
    // events out of it, so the functions only ever see present values.
    std::size_t read_inputs()
    {
        std::size_t itoks;
        _gamma_func(itoks, this->stval);
        auto& in = std::get<0>(this->ivals);
        in.clear();
        for (std::size_t i=0; i<itoks; i++)
        {
            auto tmp = iport1.read();
            if (is_present(tmp))
                in.push_back(unsafe_from_abst_ext(tmp));
        }
        return itoks;
    }

    void exec()
    {
        _ns_func(this->nsval, this->stval, std::get<0>(this->ivals));
        _od_func(std::get<0>(this->ovals), this->stval, std::get<0>(this->ivals));
        this->stval = this->nsval;
    }
};

//! Process constructor for a Mealy machine
/*! This class is used to build a finite state machine of type Mealy.
 * Given an initial state, a next-state function, and an output decoding
 * function it creates a Mealy process.
 */
template<typename TO_tuple, typename TI_tuple, typename TS> class mealyMN;

template <typename... TOs, typename... TIs, typename TS>
class mealyMN<std::tuple<TOs...>,std::tuple<TIs...>,TS> : public detail::fsm_core<mealyMN<std::tuple<TOs...>,std::tuple<TIs...>,TS>,
                                    std::tuple<std::vector<TOs>...>,
                                    std::tuple<std::vector<TIs>...>, TS>
{
    typedef detail::fsm_core<mealyMN<std::tuple<TOs...>,std::tuple<TIs...>,TS>,
                             std::tuple<std::vector<TOs>...>,
                             std::tuple<std::vector<TIs>...>, TS> base;
    friend base;
public:
    std::tuple<DT_in<TIs>...>  iport;///< tuple of ports for the input channels
    std::tuple<DT_out<TOs>...> oport;///< tuple of ports for the output channels
    
    //! Type of the partitioning function to be passed to the process constructor
    typedef std::function<void(size_t&,
                                const TS&)> gamma_functype;
    
    //! Type of the next-state function to be passed to the process constructor
    typedef std::function<void(TS&,
                                const TS&,
                                const std::tuple<std::vector<TIs>...>&)> ns_functype;
    
    //! Type of the output-decoding function to be passed to the process constructor
    typedef std::function<void(std::tuple<std::vector<TOs>...>&,
                                const TS&,
                                const std::tuple<std::vector<TIs>...>&)> od_functype;
    
    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * applies the user-imlpemented functions to the input and current
     * state and writes the results using the output port
     */
    mealyMN(sc_module_name _name,        ///< The module name
            const gamma_functype& _gamma_func,  ///< The partitioning function
            const ns_functype& _ns_func,        ///< The next_state function
            const od_functype& _od_func,        ///< The output-decoding function
            const TS& init_st                   ///< Initial state
            ) : base(_name, init_st), _gamma_func(_gamma_func), _ns_func(_ns_func),
              _od_func(_od_func) {}
    
    //! Specifying from which process constructor is the module built
    std::string forsyde_kind() const{return "DT::P::mealyMN";}
    
private:
    //! The functions passed to the process constructor
    gamma_functype _gamma_func;
    ns_functype _ns_func;
    od_functype _od_func;

    auto in_ports()  {return std::apply([](auto&... p){return std::tie(p...);}, iport);}
    auto out_ports() {return std::apply([](auto&... p){return std::tie(p...);}, oport);}

    // mealyPT, several inputs: as P, per input port.
    std::size_t read_inputs()
    {
        std::size_t itoks;
        _gamma_func(itoks, this->stval);
        std::apply([&](auto&... ival){(ival.clear(), ...);}, this->ivals);
        std::apply([&](auto&... inport){
            std::apply([&](auto&... ival){
                ([&](auto& port, auto& vals){
                    for (std::size_t i=0; i<itoks; i++)
                    {
                        auto tmp = port.read();
                        if (is_present(tmp))
                            vals.push_back(unsafe_from_abst_ext(tmp));
                    }
                }(inport, ival), ...);
            }, this->ivals);
        }, iport);
        return itoks;
    }

    void exec()
    {
        _ns_func(this->nsval, this->stval, this->ivals);
        _od_func(this->ovals, this->stval, this->ivals);
        this->stval = this->nsval;
    }
};



}
}
}

#endif
