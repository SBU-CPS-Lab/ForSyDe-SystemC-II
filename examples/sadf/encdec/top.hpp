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

using namespace sc_core;
using namespace ForSyDe;
using namespace std;

// Define an enumerated tupe for the graph scenarios with values Sp, Sm, Sc
enum scen {Sp, Sm, Sc};

struct top : ForSyDe::composite
{
    SADF::signal<int> ttot, ttotd, ttoep, ttoem, ttoec, eptod, emtod, ectod, dtor;
    SADF::signal<scen> ktot, ktoep, ktoem, ktoec, ktod;
#ifdef FORSYDE_SELF_REPORTING
    // Communication pipes
    FILE* report_pipe;      // Report pipe
    int report_pipe_fd = 0;     // Report pipe file descriptor
#endif

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

        using k_t = SADF::detectorMN<std::tuple<scen,scen,scen,scen,scen>,std::tuple<>,scen>;
        #ifdef FORSYDE_SELF_REPORTING
        auto k_ptr = new k_t(
            "k", k_cds_func, k_kss_func,
            {
                {Sp,{1,1,0,0,1}},
                {Sm,{1,0,1,0,1}},
                {Sc,{2,0,0,1,1}}
            }, // k_table
            Sc, {}, &report_pipe
        );
        #else
        auto k_ptr = new k_t(
            "k", k_cds_func, k_kss_func,
            {
                {Sp,{1,1,0,0,1}},
                {Sm,{1,0,1,0,1}},
                {Sc,{2,0,0,1,1}}
            }, // k_table
            Sc, {}
        );
        #endif
        add(k_ptr)(ktot, ktoep, ktoem, ktoec, ktod);

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
        
        using t_t = SADF::kernelMN<std::tuple<int,int,int,int>,scen,std::tuple<int>>;
        #ifdef FORSYDE_SELF_REPORTING
        auto t_ptr = new t_t(
            "t", t_func,
            {
                {Sp,{{1},{1,1,0,0}}},
                {Sm,{{1},{1,0,1,0}}},
                {Sc,{{1},{1,0,0,1}}}
            }, // t_table
            &report_pipe
        );
        #else
        auto t_ptr = new t_t(
            "t", t_func,
            {
                {Sp,{{1},{1,1,0,0}}},
                {Sm,{{1},{1,0,1,0}}},
                {Sc,{{1},{1,0,0,1}}}
            } // t_table
        );
        #endif
        auto& t1 = add(t_ptr);
        t1.cport1(ktot);
        t1(ttot, ttoep, ttoem, ttoec, ttotd);

        add(new SADF::delayn<int>("totd", 0, 1))(ttotd, ttot);

        // The kernel E+
        
        auto ep_func = [](auto&& out, const auto& sc, const auto& inp) {
            const auto& [inpT] = inp;
            auto&& [outD] = out;

            outD[0] = inpT[0] + 1;
        };
        
        using ep_t = SADF::kernelMN<std::tuple<int>,scen,std::tuple<int>>;
        #ifdef FORSYDE_SELF_REPORTING
        auto ep_ptr = new ep_t(
            "ep", ep_func,
            {
                {Sp,{{1},{1}}},
                {Sm,{{0},{0}}},
                {Sc,{{0},{0}}}
            }, // e_table
            &report_pipe
        );
        #else
        auto ep_ptr = new ep_t(
            "ep", ep_func,
            {
                {Sp,{{1},{1}}},
                {Sm,{{0},{0}}},
                {Sc,{{0},{0}}}
            } // e_table
        );
        #endif
        auto& ep1 = add(ep_ptr);
        ep1.cport1(ktoep);
        ep1(eptod, ttoep);

        // The kernel E-
        
        auto em_func = [](auto&& out, const auto& sc, const auto& inp) {
            const auto& [inpT] = inp;
            auto&& [outD] = out;

            outD[0] = {inpT[0] - 1};
        };

        using em_t = SADF::kernelMN<std::tuple<int>,scen,std::tuple<int>>;
        #ifdef FORSYDE_SELF_REPORTING
        auto em_ptr = new em_t(
            "em", em_func,
            {
                {Sp,{{0},{0}}},
                {Sm,{{1},{1}}},
                {Sc,{{0},{0}}}
            }, // e_table
            &report_pipe
        );
        #else
        auto em_ptr = new em_t(
            "em", em_func,
            {
                {Sp,{{0},{0}}},
                {Sm,{{1},{1}}},
                {Sc,{{0},{0}}}
            } // e_table
        );
        #endif
        auto& em1 = add(em_ptr);
        em1.cport1(ktoem);
        em1(emtod, ttoem);

        // The kernel Ec

        auto ec_func = [](auto&& out, const auto& sc, const auto& inp) {
            const auto& [inpT] = inp;
            auto&& [outD] = out;

            outD[0] = inpT[0]+inpT[1];
            outD[1] = inpT[0]-inpT[1];
        };
        
        using ec_t = SADF::kernelMN<std::tuple<int>,scen,std::tuple<int>>;
        #ifdef FORSYDE_SELF_REPORTING
        auto ec_ptr = new ec_t(
            "ec", ec_func,
            {
                {Sp,{{0},{0}}},
                {Sm,{{0},{0}}},
                {Sc,{{2},{2}}}
            }, // ec_table
            &report_pipe
        );
        #else
        auto ec_ptr = new ec_t(
            "ec", ec_func,
            {
                {Sp,{{0},{0}}},
                {Sm,{{0},{0}}},
                {Sc,{{2},{2}}}
            } // ec_table
        );
        #endif
        auto& ec1 = add(ec_ptr);
        ec1.cport1(ktoec);
        ec1(ectod, ttoec);

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

        using d_t = SADF::kernelMN<std::tuple<int>,scen,std::tuple<int,int,int>>;
        #ifdef FORSYDE_SELF_REPORTING
        auto d_ptr = new d_t(
            "d", d_func,
            {
                {Sp,{{1,0,0},{1}}},
                {Sm,{{0,1,0},{1}}},
                {Sc,{{0,0,2},{2}}}
            }, // d_table
            &report_pipe
        );
        #else
        auto d_ptr = new d_t(
            "d", d_func,
            {
                {Sp,{{1,0,0},{1}}},
                {Sm,{{0,1,0},{1}}},
                {Sc,{{0,0,2},{2}}}
            } // d_table
        );
        #endif
        auto& d1 = add(d_ptr);
        d1.cport1(ktod);
        d1(dtor, eptod, emtod, ectod);

        // The SDF sink actor r

        add(new SDF::sink(
            "r",
            [](const int& out) {
                std::cout <<"out = " <<out << std::endl;
            }
        ))(dtor);
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

