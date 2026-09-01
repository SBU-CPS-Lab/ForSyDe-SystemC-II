
#ifndef ProcessChanuleZeroLeft_HPP
#define ProcessChanuleZeroLeft_HPP

#include <forsyde.hpp>
#include "include/MP3Decoder.h"

using namespace std;

typedef tuple<
        vector<ChanuleSamples>,
        vector<VecType>
    > ChanuleType;

void ProcessChanuleZeroLeft_func(
    vector<ChanuleType>&              outs, // headerChanuleLeft
    const vector<FrameHeader>&        inp1, // headerGranule
    const vector<FrameSideInfo>&      inp2, // sideInfoGranule
    const vector<ChanuleData>&        inp3, // chanuleData
    const vector<VecType>&            inp4  // sync
)
{
    vector<ChanuleSamples>         out1(1);
    vector<VecType>                out2(1);
#pragma ForSyDe begin ProcessChanuleZeroLeft_func

    /* User-defined local variables */
    GranuleData processedMainData;
    
    /* Main actor code */
    out2[0] = inp4[0];
    /* The decoder's DSP routines take non-const pointers and transform
     * the frame data in place (MPG_L3_Antialias, Hybrid_Synthesis and
     * Frequency_Inversion all assign through their ChanuleData*). An SDF
     * process must not modify its inputs -- they are values delivered by
     * the channel, and its ports are const for that reason -- so work on
     * local copies rather than casting the constness away. */
    FrameHeader   hdr_copy  = inp1[0];
    FrameSideInfo side_copy = inp2[0];
    ChanuleData   data_copy = inp3[0];
    processChanule(0, 0, &out1[0], &hdr_copy, &side_copy, &data_copy, out2[0].v_vec);
    
            
#pragma ForSyDe end
    outs[0] = make_tuple(out1,out2);
}

#endif
