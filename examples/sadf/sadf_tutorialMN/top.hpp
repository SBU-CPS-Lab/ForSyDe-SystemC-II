/**********************************************************************
    * main.cpp -- the main file and testbench for the SADF tutorial   *
    *                                                                 *
    * Author:  Mohammad Vazirpanah (mohammad.vazirpanah@yahoo.com)    *
    *                                                                 *
    * Purpose: Demonstration of a simple program.                     *
    *                                                                 *
    * Usage:   SADF Tutorial                                          *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include <forsyde.hpp>
#include "kernels.hpp"
#include "detectors.hpp"
#include "globals.hpp"

using namespace sc_core;
using namespace ForSyDe;
using namespace std;

struct top : ForSyDe::composite
{
    SADF::signal<int> from_source;
    SADF::signal<int> to_kernel1, from_kernel1, to_kernel2, from_kernel2;
#ifdef FORSYDE_SELF_REPORTING
    // Communication pipes
    FILE* report_pipe;      // Report pipe
    int report_pipe_fd = 0;     // Report pipe file descriptor
#endif

    SC_CTOR(top)
    {

        auto from_detector1 = new SADF::signal<kernel1_scenario_type>("from_detector1",1);
        auto from_detector2 = new SADF::signal<kernel2_scenario_type>("from_detector2",1);

        //! < -------------------------------- Using Helper--------------------------------> //!

        using det1_t = SADF::detectorMN<std::tuple<kernel1_scenario_type,kernel2_scenario_type>,
                                        std::tuple<int>,detector_scenario_type>;
        #ifdef FORSYDE_SELF_REPORTING
        auto det1_ptr = new det1_t(
                                "detector1",
                                detector1_cds_func,
                                detector1_kss_func,
                                {
                                    {S1,{1,1}},
                                    {S2,{1,1}},
                                    {S3,{1,1}},
                                    {S4,{1,1}}
                                }, // detector1_table
                                S1,
                                {1},
                                &report_pipe
                            );
        #else
        auto det1_ptr = new det1_t(
                                "detector1",
                                detector1_cds_func,
                                detector1_kss_func,
                                {
                                    {S1,{1,1}},
                                    {S2,{1,1}},
                                    {S3,{1,1}},
                                    {S4,{1,1}}
                                }, // detector1_table
                                S1,
                                {1}
                            );
        #endif
        add(det1_ptr)(*from_detector1, *from_detector2, from_source);

        using k1_t = SADF::kernelMN<std::tuple<int>,kernel1_scenario_type,std::tuple<int>>;
        #ifdef FORSYDE_SELF_REPORTING
        auto k1_ptr = new k1_t("kernel1",
                            kernel1_func,
                            {
                                {ADD,  {{3},{1}}},
                                {MINUS,{{2},{1}}}
                            }, // kernel1_table
                            &report_pipe
        );
        #else
        auto k1_ptr = new k1_t("kernel1",
                            kernel1_func,
                            {
                                {ADD,  {{3},{1}}},
                                {MINUS,{{2},{1}}}
                            } // kernel1_table
        );
        #endif
        auto& kernel1 = add(k1_ptr);
        kernel1.cport1(*from_detector1);
        kernel1(from_kernel1, to_kernel1);

        using k2_t = SADF::kernelMN<std::tuple<int>,kernel2_scenario_type,std::tuple<int>>;
        #ifdef FORSYDE_SELF_REPORTING
        auto k2_ptr = new k2_t("kernel2",
                            kernel2_func,
                            {
                                {MUL,{{2},{1}}},
                                {DIV,{{2},{1}}}
                            }, // kernel2_table
                            &report_pipe
        );
        #else
        auto k2_ptr = new k2_t("kernel2",
                            kernel2_func,
                            {
                                {MUL,{{2},{1}}},
                                {DIV,{{2},{1}}}
                            } // kernel2_table
        );
        #endif
        auto& kernel2 = add(k2_ptr);
        kernel2.cport1(*from_detector2);
        kernel2(from_kernel2, to_kernel2);

        add(new SADF::source<int>("source1", [] (int& out1, const int& inp1) {out1 = inp1 + 1;}, 1, 0))(to_kernel1);

        add(new SADF::source<int>("source2", [] (int& out1, const int& inp1) {out1 = inp1 - 1;}, -1, 0))(to_kernel2);

        add(new SADF::sink<int>("sink1", [] (const int& out) {std::cout <<"kernel1 = " <<out << std::endl;}))(from_kernel1);

        add(new SADF::sink<int>("sink2", [] (const int& out) {std::cout <<"kernel2 = " <<out << std::endl;}))(from_kernel2);

        //! < -------------------------------- Without Using Helper--------------------------------> //!

        add(new SADF::source<int>("sourced", [] (int& out1, const int& inp1) {out1 = inp1 + 1;}, 1, 4))(from_source);

        // auto detector1 = new SADF::detectorMN<
        //                         tuple<kernel1_scenario_type,kernel2_scenario_type>,
        //                         tuple<int>,
        //                         detector_scenario_type
        //                     >
        //                     (
        //                         "detector1",
        //                         detector1_cds_func,
        //                         detector1_kss_func,
        //                         detector1_table,
        //                         S1,
        //                         {1}
        //                         #ifdef FORSYDE_SELF_REPORTING
        //                         ,&report_pipe
        //                         #endif
        //                     );
        // get<0>(detector1->iport)(from_source);
        // get<0>(detector1->oport)(*from_detector1);
        // get<1>(detector1->oport)(*from_detector2);

        // auto kernel1 = new SADF::kernelMN<tuple<int>,kernel1_scenario_type,tuple<int>>(
        //                     "kernel1",
        //                     kernel1_func,
        //                     kernel1_table
        //                     #ifdef FORSYDE_SELF_REPORTING
        //                     ,&report_pipe
        //                     #endif
        //                 );
        // kernel1->cport1(*from_detector1);
        // get<0>(kernel1->iport)(to_kernel1);
        // get<0>(kernel1->oport)(from_kernel1);

                        
        // auto kernel2 = new SADF::kernelMN<tuple<int>,kernel2_scenario_type,tuple<int>>(
        //                     "kernel2",
        //                     kernel2_func,
        //                     kernel2_table
        //                     #ifdef FORSYDE_SELF_REPORTING
        //                     ,&report_pipe
        //                     #endif
        //                 );
        // kernel2->cport1(*from_detector2);
        // get<0>(kernel2->iport)(to_kernel2);
        // get<0>(kernel2->oport)(from_kernel2);


        // auto source1 = new SDF::source<int>("source1", [] (int& out1, const int& inp1) {out1 = inp1 + 1;}, 1, 0);
        // source1->oport1(to_kernel1);

        // auto source2 = new SDF::source<int>("source2", [] (int& out1, const int& inp1) {out1 = inp1 - 1;}, -1, 0);
        // source2->oport1(to_kernel2);


        // auto sink1 = new SDF::sink<int>("sink1",[](const int& out) {cout <<"kernel1 = " <<out << endl;});
        // sink1->iport1(from_kernel1);


        // auto sink2 = new SDF::sink<int>("sink2",[](const int& out) {cout <<"kernel2 = " <<out << endl;;});
        // sink2-> iport1(from_kernel2);

    }
#ifdef FORSYDE_INTROSPECTION
    void start_of_simulation()
    {
        ForSyDe::XMLExport dumper("gen/");
        dumper.traverse(this);
#ifdef FORSYDE_SELF_REPORTING
        while (report_pipe_fd<=0) // pipe is not open
        {
            report_pipe_fd = open("gen/self_report", O_WRONLY|O_NONBLOCK);
            if (report_pipe_fd > 0)
                report_pipe = fdopen(report_pipe_fd, "w");
        }
#endif
    }
#endif
#ifdef FORSYDE_SELF_REPORTING
    void end_of_simulation()
    {
        fclose(report_pipe);
    }
#endif

};

