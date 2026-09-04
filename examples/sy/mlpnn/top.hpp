/**********************************************************************
    * top.hpp -- the top level and test bench for the mlpnn example   *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of the vector operations.                *
    *                                                                 *
    * Usage:   mlpnn example                                          *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include "forsyde.hpp"

#include "conf.hpp"
#include "mlpnn.hpp"
#include "report.hpp"

using namespace std;
using namespace ForSyDe::SY;

struct top : ForSyDe::composite
{
    SY2SY<float> nn_input_sig[NN_NUM_INPUTS];
    SY2SY<float> nn_output_sig[NN_NUM_OUTPUTS];
    
    SC_CTOR(top)
    {

    	auto& mlpnn1 = add(new mlpnn("mlpnn1"));

    	for (int i=0;i<NN_NUM_INPUTS;i++)
    	{
    		add(new vsource<float>(("nn_inputs"+std::to_string(i)).c_str(), nn_inputs[i]))(nn_input_sig[i]);
    		mlpnn1.iport[i](nn_input_sig[i]);
    	}

    	for (int i=0;i<NN_NUM_OUTPUTS;i++)
    	{
    		add(new sink<float>(("report"+std::to_string(i)).c_str(), report_func))(nn_output_sig[i]);
    		mlpnn1.oport[i](nn_output_sig[i]);
    	}

    }
#ifdef FORSYDE_INTROSPECTION
    void start_of_simulation()
    {
        ForSyDe::XMLExport dumper("gen/");
        dumper.traverse(this);
    }
#endif
};
