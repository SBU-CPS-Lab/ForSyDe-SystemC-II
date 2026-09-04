/**********************************************************************
    * mp4dec.hpp -- an MPEG-4 decoder for the simple profile          *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of an example in the SADF MoC.           *
    *                                                                 *
    * Usage:   MPEG4-SP example                                       *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/


#ifndef MP4DEC_HPP
#define MP4DEC_HPP

#include <forsyde.hpp>
#include "globals.hpp"
#include "kernels.hpp"
#include "detectors.hpp"

using namespace ForSyDe;
using namespace std;

struct mp4dec : ForSyDe::composite
{
    SADF::in_port<frame_type>  ft;
    SADF::in_port<MacroBlock<bs>>  mb;
    SADF::out_port<Frame<fsr,fsc>> out;
    
    SC_CTOR(mp4dec)
    {
        auto fd2idct = new SADF::signal<frame_type>("fd2idct",100);
        auto fd2vld  = new SADF::signal<frame_type>("fd2vld",100);
        auto fd2mc   = new SADF::signal<frame_type>("fd2mc",1);
        auto fd2rc   = new SADF::signal<frame_type>("fd2rc",1);
        auto vld2idct= new SADF::signal<MacroBlock<bs>>("vld2idct",100);
        auto vld2mc  = new SADF::signal<MotionVec>("vld2mc",100);
        auto idct2rc = new SADF::signal<MacroBlock<bs>>("idct2rc",100);
        auto mc2rc   = new SADF::signal<Frame<fsr,fsc>>("mc2rc",100);
        auto rc2fd   = new SADF::signal<bool>("rc2fd",100);
        auto rc2fdd  = new SADF::signal<bool>("rc2fdd",100);
        auto rc2mc   = new SADF::signal<Frame<fsr,fsc>>("rc2mc",100);
        auto rc2mcd  = new SADF::signal<Frame<fsr,fsc>>("rc2mcd",100);

        using fd1_t = SADF::detectorMN<std::tuple<frame_type,frame_type,frame_type,frame_type>,
                                       std::tuple<frame_type,bool>,frame_type>;
        add(new fd1_t(
            "fd1",
            fd_cds_func,
            fd_kss_func,
            {
                {  I, {99,99,1,1}},
                { P0, { 1, 1,1,1}},
                {P30, {30,30,1,1}},
                {P40, {40,40,1,1}},
                {P50, {50,50,1,1}},
                {P60, {60,60,1,1}},
                {P70, {70,70,1,1}},
                {P80, {80,80,1,1}},
                {P99, {99,99,1,1}}
            },
            I,
            {1,1}
        ))(*fd2idct,*fd2vld,*fd2mc,*fd2rc, ft,*rc2fdd);

        using vld1_t = SADF::kernelMN<std::tuple<MacroBlock<bs>,MotionVec>,frame_type,
                                      std::tuple<MacroBlock<bs>>>;
        auto& vld1 = add(new vld1_t(
            "vld1",
            vld_func,
            {
                {  I, {{1},{1,0}}},
                { P0, {{1},{0,0}}},
                {P30, {{1},{1,1}}},
                {P40, {{1},{1,1}}},
                {P50, {{1},{1,1}}},
                {P60, {{1},{1,1}}},
                {P70, {{1},{1,1}}},
                {P80, {{1},{1,1}}},
                {P99, {{1},{1,1}}}
            }
        ));
        vld1.cport1(*fd2vld);
        vld1(*vld2idct,*vld2mc, mb);

        using idct1_t = SADF::kernelMN<std::tuple<MacroBlock<bs>>,frame_type,
                                       std::tuple<MacroBlock<bs>>>;
        auto& idct1 = add(new idct1_t(
            "idct1",
            idct_func,
            {
                {  I, {{1},{1}}},
                { P0, {{0},{0}}},
                {P30, {{1},{1}}},
                {P40, {{1},{1}}},
                {P50, {{1},{1}}},
                {P60, {{1},{1}}},
                {P70, {{1},{1}}},
                {P80, {{1},{1}}},
                {P99, {{1},{1}}}
            }
        ));
        idct1.cport1(*fd2idct);
        idct1(*idct2rc, *vld2idct);

        using mc1_t = SADF::kernelMN<std::tuple<Frame<fsr,fsc>>,frame_type,
                                     std::tuple<MotionVec,Frame<fsr,fsc>>>;
        auto& mc1 = add(new mc1_t(
            "mc1",
            mc_func,
            {
                {  I, {{ 0,1},{1}}},
                { P0, {{ 0,1},{1}}},
                {P30, {{30,1},{1}}},
                {P40, {{40,1},{1}}},
                {P50, {{50,1},{1}}},
                {P60, {{60,1},{1}}},
                {P70, {{70,1},{1}}},
                {P80, {{80,1},{1}}},
                {P99, {{99,1},{1}}}
            }
        ));
        mc1.cport1(*fd2mc);
        mc1(*mc2rc, *vld2mc,*rc2mcd);

        using rc1_t = SADF::kernelMN<std::tuple<Frame<fsr,fsc>,bool>,frame_type,
                                     std::tuple<MacroBlock<bs>,Frame<fsr,fsc>>>;
        auto& rc1 = add(new rc1_t(
            "rc1",
            rc_func,
            {
                {  I, {{99,1},{1,1}}},
                { P0, {{ 0,1},{0,1}}},
                {P30, {{30,1},{1,1}}},
                {P40, {{40,1},{1,1}}},
                {P50, {{50,1},{1,1}}},
                {P60, {{60,1},{1,1}}},
                {P70, {{70,1},{1,1}}},
                {P80, {{80,1},{1,1}}},
                {P99, {{99,1},{1,1}}}
            }
        ));
        rc1.cport1(*fd2rc);
        rc1(*rc2mc,*rc2fd, *idct2rc,*mc2rc);
        get<0>(rc1.oport)(out);

        add(new SDF::delayn(
            "rc2fddelay",
            true,
            3
        ))(*rc2fdd, *rc2fd);

        add(new SDF::delay(
            "rc2mcdelay",
            Frame<fsr,fsc>()
        ))(*rc2mcd, *rc2mc);
    }
};

#endif
