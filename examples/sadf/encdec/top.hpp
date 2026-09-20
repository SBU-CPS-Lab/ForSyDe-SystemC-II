/**********************************************************************
    * main.cpp -- the main file and testbench for the SADF Encoder/   *
    *             Decoder example                                     *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a simple program.                     *
    *                                                                 *
    * Usage:   SADF Encoder/Decoder                                   *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include <forsyde.hpp>

#include <cstdlib>   // getenv, for the runtime self-report opt-in
#include <fcntl.h>   // open, for the report pipe
#include <unistd.h>

using namespace sc_core;
using namespace ForSyDe;
using namespace std;

// Define an enumerated tupe for the graph scenarios with values Sp, Sm, Sc
enum scen {Sp, Sm, Sc};

FORSYDE_COMPOSITE(top)
{
    SADF::signal<int> ttot, ttotd, ttoep, ttoem, ttoec, eptod, emtod, ectod, dtor;
    SADF::signal<scen> ktot, ktoep, ktoem, ktoec, ktod;

    SC_CTOR(top)
    {
        // The detector K        
        auto k_cds_func = [](auto&& new_scen, const auto& prev_scen, const auto& inp) {
            new_scen = (scen)((prev_scen+1) % 3);
        };

        auto k_kss_func = [](auto&& out, const auto& sc, const auto& inp) {
            auto&& [outT,outEp,outEm,outEc,outD] = out;

            switch (sc) {
                case Sp:
                    outT[0] = outEp[0] = outD [0]= Sp;
                    break;
                case Sm:
                    outT[0] = outEm[0]= outD[0] = Sm;
                    break;
                case Sc:
                    outT[0] = outT[1] = outEc[0] = outD[0] = Sc;
                    break;
                default:
                    break;
            }
        };

        add_detectorMN(*this, "k", k_cds_func, k_kss_func,
            {
                {Sp,{1,1,0,0,1}},
                {Sm,{1,0,1,0,1}},
                {Sc,{2,0,0,1,1}}
            }, // k_table
            Sc, {},
            outs(ktot, ktoep, ktoem, ktoec, ktod), ins());

        // The kernel T        
        auto t_func = [&](auto&& out, const auto& sc, const auto& inp) {
            const auto& [inp1] = inp;
            auto&& [outT,outEp,outEm,outEc] = out;
            auto& cur_st = inp1[0];

            outT[0] = cur_st + 1;
            switch (sc) {
                case Sp:    outEp[0] = cur_st;  break;
                case Sm:    outEm[0] = cur_st;  break;
                case Sc:    outEc[0] = cur_st;  break;
            }
            if (cur_st > 20) wait();
        };
        
        add_kernelMN(*this, "t", t_func,
            {
                {Sp,{{1},{1,1,0,0}}},
                {Sm,{{1},{1,0,1,0}}},
                {Sc,{{1},{1,0,0,1}}}
            }, // t_table
            outs(ttot, ttoep, ttoem, ttoec), ktot, ins(ttotd));

        add(new SADF::delayn<int>("totd", 0, 1))(ttotd, ttot);

        // The kernel E+
        
        auto ep_func = [](auto&& out, const auto& sc, const auto& inp) {
            const auto& [inpT] = inp;
            auto&& [outD] = out;

            outD[0] = inpT[0] + 1;
        };
        
        add_kernelMN(*this, "ep", ep_func,
            {
                {Sp,{{1},{1}}},
                {Sm,{{0},{0}}},
                {Sc,{{0},{0}}}
            }, // e_table
            outs(eptod), ktoep, ins(ttoep));

        // The kernel E-
        
        auto em_func = [](auto&& out, const auto& sc, const auto& inp) {
            const auto& [inpT] = inp;
            auto&& [outD] = out;

            outD[0] = {inpT[0] - 1};
        };

        add_kernelMN(*this, "em", em_func,
            {
                {Sp,{{0},{0}}},
                {Sm,{{1},{1}}},
                {Sc,{{0},{0}}}
            }, // e_table
            outs(emtod), ktoem, ins(ttoem));

        // The kernel Ec

        auto ec_func = [](auto&& out, const auto& sc, const auto& inp) {
            const auto& [inpT] = inp;
            auto&& [outD] = out;

            outD[0] = inpT[0]+inpT[1];
            outD[1] = inpT[0]-inpT[1];
        };
        
        add_kernelMN(*this, "ec", ec_func,
            {
                {Sp,{{0},{0}}},
                {Sm,{{0},{0}}},
                {Sc,{{2},{2}}}
            }, // ec_table
            outs(ectod), ktoec, ins(ttoec));

        // The kernel D
        
        auto d_func = [](auto&& out, const auto& sc, const auto& inp) {
            const auto& [inpEp, inpEm, inpEc] = inp;
            auto&& [outR] = out;

            switch (sc) {
                case Sp:
                    outR[0] = inpEp[0] - 1;
                    break;
                case Sm:
                    outR[0] = inpEm[0] + 1;
                    break;
                case Sc:
                    outR[0] = (inpEc[0] + inpEc[1]) / 2;
                    outR[1] = (inpEc[0] - inpEc[1]) / 2;
                    break;
            }
        };

        add_kernelMN(*this, "d", d_func,
            {
                {Sp,{{1,0,0},{1}}},
                {Sm,{{0,1,0},{1}}},
                {Sc,{{0,0,2},{2}}}
            }, // d_table
            outs(dtor), ktod, ins(eptod, emtod, ectod));

        // The SDF sink actor r

        add(new SDF::sink(
            "r",
            [](const int& out) {
                std::cout <<"out = " <<out << std::endl;
            }
        ))(dtor);
    }
    // Self-reporting, as a subscription rather than a build (D10).
    //
    // Every kernel and detector in this model used to be constructed
    // twice -- once plain and once with &report_pipe -- behind
    // #ifdef FORSYDE_SELF_REPORTING, so the model was written twice to
    // say one thing. The processes report unconditionally now, and
    // this decides whether anything listens.
    //
    // A runtime choice, not a compile-time one, because the reason the
    // old flag had to exist is the loop below: gen/self_report is a
    // named pipe and opening the write end spins until a reader
    // attaches, so a model that always opened it would always hang. A
    // model that asks at runtime keeps that property and still gets
    // the reporting path compiled on every build, which the macro
    // never did -- it was commented out in every Makefile in the tree.
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

