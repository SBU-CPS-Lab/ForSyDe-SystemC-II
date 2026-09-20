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

#include <cstdlib>   // getenv, for the runtime self-report opt-in
#include <fcntl.h>   // open, for the report pipe
#include <unistd.h>

using namespace sc_core;
using namespace ForSyDe;
using namespace std;

FORSYDE_COMPOSITE(top)
{
    SADF::signal<int> from_source;
    SADF::signal<int> to_kernel1, from_kernel1, to_kernel2, from_kernel2;

    SC_CTOR(top)
    {

        auto from_detector1 = new SADF::signal<kernel1_scenario_type>("from_detector1",1);
        auto from_detector2 = new SADF::signal<kernel2_scenario_type>("from_detector2",1);

        //! < -------------------------------- Using Helper--------------------------------> //!

        add_detectorMN(*this, "detector1",
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
            outs(*from_detector1, *from_detector2), ins(from_source));

        add_kernelMN(*this, "kernel1",
            kernel1_func,
            {
                {ADD,  {{3},{1}}},
                {MINUS,{{2},{1}}}
            }, // kernel1_table
            outs(from_kernel1), *from_detector1, ins(to_kernel1));

        add_kernelMN(*this, "kernel2",
            kernel2_func,
            {
                {MUL,{{2},{1}}},
                {DIV,{{2},{1}}}
            }, // kernel2_table
            outs(from_kernel2), *from_detector2, ins(to_kernel2));

        add(new SDF::source("source1", [] (int& out1, const int& inp1) {out1 = inp1 + 1;}, 1, 0))(to_kernel1);

        add(new SDF::source("source2", [] (int& out1, const int& inp1) {out1 = inp1 - 1;}, -1, 0))(to_kernel2);

        add(new SDF::sink("sink1", [] (const int& out) {std::cout <<"kernel1 = " <<out << std::endl;}))(from_kernel1);

        add(new SDF::sink("sink2", [] (const int& out) {std::cout <<"kernel2 = " <<out << std::endl;}))(from_kernel2);

        //! < -------------------------------- Without Using Helper--------------------------------> //!

        add(new SDF::source("sourced", [] (int& out1, const int& inp1) {out1 = inp1 + 1;}, 1, 4))(from_source);

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
        //                     );
        // get<0>(detector1->iport)(from_source);
        // get<0>(detector1->oport)(*from_detector1);
        // get<1>(detector1->oport)(*from_detector2);

        // auto kernel1 = new SADF::kernelMN<tuple<int>,kernel1_scenario_type,tuple<int>>(
        //                     "kernel1",
        //                     kernel1_func,
        //                     kernel1_table
        //                 );
        // kernel1->cport1(*from_detector1);
        // get<0>(kernel1->iport)(to_kernel1);
        // get<0>(kernel1->oport)(from_kernel1);

                        
        // auto kernel2 = new SADF::kernelMN<tuple<int>,kernel2_scenario_type,tuple<int>>(
        //                     "kernel2",
        //                     kernel2_func,
        //                     kernel2_table
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
    // Self-reporting, as a runtime subscription (D10).
    //
    // Set FORSYDE_SELF_REPORT and have a reader on gen/self_report --
    // e.g. `mkfifo gen/self_report && cat gen/self_report &` before
    // running -- and every kernel and detector firing in this model is
    // written to it. Asking at runtime rather than at compile time is
    // what lets the path be built on every build; opening the write end
    // of a named pipe blocks until a reader attaches, so a model that
    // opened it unconditionally would hang with nothing listening.
    void start_of_simulation()
    {
#ifdef FORSYDE_INTROSPECTION
        ForSyDe::XMLExport dumper("gen/");
        dumper.traverse(this);
#endif
        if (!std::getenv("FORSYDE_SELF_REPORT")) return;

        while (report_pipe_fd <= 0)   // spins until a reader attaches
        {
            report_pipe_fd = open("gen/self_report", O_WRONLY|O_NONBLOCK);
            if (report_pipe_fd > 0)
                report_pipe = fdopen(report_pipe_fd, "w");
        }
        ForSyDe::reflection::observe(ForSyDe::reflection::to_pipe(&report_pipe));
    }

    void end_of_simulation()
    {
        if (report_pipe) fclose(report_pipe);
    }

private:
    FILE* report_pipe = nullptr;
    int report_pipe_fd = 0;

};

