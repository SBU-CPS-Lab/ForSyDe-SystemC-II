/**********************************************************************           
    * top.hpp -- the Top process and testbench for the VAD example    *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *          adapted from KisTA: https://github.com/nandohca/kista  *
    *                                                                 *
    * Purpose:                                                        *
    *                                                                 *
    * Usage:   The Voice Activity Detection (VAD) example             *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include <forsyde.hpp>

#include "includes/vad_types.hpp"
#include "vad_source.hpp"
#include "ToneDetection.hpp"
#include "EnergyComputation.hpp"
#include "ACFAveraging.hpp"
#include "PredictorValues.hpp"
#include "SpectralComparison.hpp"
#include "ThresholdAdaptation.hpp"
#include "VADdecision.hpp"
#include "VADhangover.hpp"
#include "VADFilesink.hpp"

using namespace ForSyDe;

FORSYDE_COMPOSITE(top)
{
    SDF::signal<short> e5, e9, e11, e15, e16, e18, e19;
    SDF::signal<L_av_t> e1, e2;
    SDF::signal<rav1_t> e3, e4;
    SDF::signal<pvad_acf0_t> e6;
    SDF::signal<rvad_t> e7, e7d;
    SDF::signal<Pfloat> e8, e10;
    SDF::signal<r_t> e12, e13, e14;
    SDF::signal<rc_t> e17;
    SDF::signal<tuple_of_vectors<r_t,r_t,r_t,short,short,rc_t,short>> e12_13_14_15_16_17_18;
    
    SC_CTOR(top)
    {
        add(new SDF::file_source("VADFilesource1", VADFilesource_func, "source_data.txt"))
            (e12_13_14_15_16_17_18);
        add_unzipN(*this, "VADFilesource1_unzip", {1,1,1,1,1,1,1},
            e12_13_14_15_16_17_18, e12, e13, e14, e15, e16, e17, e18);

        add(new SDF::comb("ToneDetection1", ToneDetection_func, 1, 1))(e9, e17);

        add_combMN(*this,
            "EnergyComputation1",
            EnergyComputation_func,
            {1,1},
            {1,1,1},
            outs(e6, e8), ins(e7d, e13, e16));

        add_combMN(*this,
            "ACFAveraging1",
            ACFAveraging_func,
            {1,1},
            {1,1,1},
            outs(e1, e2), ins(e12, e14, e15));

        add(new SDF::comb("PredictorValues1", PredictorValues_func, 1, 1))(readers(e3, e4), e2);

        add(new SDF::comb2("SpectralComparison1", SpectralComparison_func, 1, 1, 1))(e5, e1, e3);

        add_combMN(*this,
            "ThresholdAdaptation1",
            ThresholdAdaptation_func,
            {1,1},
            {1,1,1,1,1},
            outs(e7, e10), ins(e4, e5, e6, e9, e18));
        
        std::array<short,9> rvad_init = {{0x6000,0,0,0,0,0,0,0,0}}; short scal_init = 7;
        add(new SDF::delay("e7_init", std::make_tuple(rvad_init,scal_init)))(e7d, e7);
        
        add(new SDF::comb2("VADdecision1", VADdecision_func, 1, 1, 1))(e11, e8, e10);
        
        add(new SDF::comb("VADhangover1", VADhangover_func, 1, 1))(e19, e11);
        
        add(new SDF::file_sink("VADFilesink1", VADFilesink_func, "sink_data.txt"))(e19);
        
    }
#ifdef FORSYDE_INTROSPECTION
    void start_of_simulation()
    {
        ForSyDe::XMLExport dumper("gen/");
        dumper.traverse(this);
    }
#endif
};
