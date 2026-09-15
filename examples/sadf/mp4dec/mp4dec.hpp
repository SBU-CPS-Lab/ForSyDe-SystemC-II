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

FORSYDE_COMPOSITE(mp4dec)
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

        add_detectorMN(*this,
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
            {1,1},
            outs(*fd2idct,*fd2vld,*fd2mc,*fd2rc), ins(ft,*rc2fdd));

        add_kernelMN(*this,
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
            },
            outs(*vld2idct,*vld2mc), *fd2vld, ins(mb));

        add_kernelMN(*this,
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
            },
            outs(*idct2rc), *fd2idct, ins(*vld2idct));

        add_kernelMN(*this,
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
            },
            outs(*mc2rc), *fd2mc, ins(*vld2mc,*rc2mcd));

        add_kernelMN(*this,
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
            },
            outs(readers(*rc2mc, out),*rc2fd), *fd2rc, ins(*idct2rc,*mc2rc));

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
