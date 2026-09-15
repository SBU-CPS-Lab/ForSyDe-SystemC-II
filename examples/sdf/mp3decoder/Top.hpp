
#include "ReadBitstreamAndExtractFrames.hpp"
#include "ProcessChanuleZeroLeft.hpp"
#include "ProcessChanuleZeroRight.hpp"
#include "ProcessChanuleOneLeft.hpp"
#include "ProcessChanuleOneRight.hpp"
#include "Merge.hpp"
#include "ProcessGranuleZero.hpp"
#include "ProcessGranuleOne.hpp"

using namespace ForSyDe::SDF;
using namespace std;

ostream& operator <<(ostream &os,const ChanuleSamples &obj)
{
      //~ os<<obj.strVal;
      return os;
}
ostream& operator <<(ostream &os,const ChanuleData &obj)
{
      //~ os<<obj.strVal;
      return os;
}
ostream& operator <<(ostream &os,const GranuleData &obj)
{
      //~ os<<obj.strVal;
      return os;
}
ostream& operator <<(ostream &os,const FrameSideInfo &obj)
{
      //~ os<<obj.strVal;
      return os;
}
ostream& operator <<(ostream &os,const FrameHeader &obj)
{
      //~ os<<obj.strVal;
      return os;
}
ostream& operator <<(ostream &os,const VecType &obj)
{
      //~ os<<obj.strVal;
      return os;
}
typedef comb<InputType,float> ReadBitstreamAndExtractFrames;

typedef comb4<
    tuple<
        vector<ChanuleSamples>,
        vector<VecType>
    >,
    FrameHeader,FrameSideInfo,ChanuleData,VecType> ProcessChanule;

typedef comb3<
    tuple<
        vector<FrameHeader>,
        vector<FrameSideInfo>,
        vector<ChanuleData>,
        vector<FrameHeader>,
        vector<FrameSideInfo>,
        vector<ChanuleData>
    >,
    FrameHeader,FrameSideInfo,GranuleData> ProcessGranule;

FORSYDE_COMPOSITE(Top)
{
public:
    /* Actors */
    ReadBitstreamAndExtractFrames *a_ReadBitstreamAndExtractFrames;
    delayn<float> *a_DummyLoopDelay;
    ProcessChanule *a_ProcessChanule0Left;
    sink<MergeType> *a_Merge;
    ProcessGranule *a_ProcessGranule0, *a_ProcessGranule1;
    ProcessChanule *a_ProcessChanule0Right;
    ProcessChanule *a_ProcessChanule1Right;
    ProcessChanule *a_ProcessChanule1Left;
    delayn<VecType> *a_ch_1r_0r, *a_ch_1l_0l;

    /* Channels */
    SDF2SDF<float> *dummyloopi;
    SDF2SDF<float> *dummyloopo;
    SDF2SDF<InputType> *zippedInput;
    SDF2SDF<bool> *lastFrame;
    SDF2SDF<FrameHeader> *headerGranule0;
    SDF2SDF<FrameHeader> *headerGranule1;
    SDF2SDF<FrameSideInfo> *sideInfoGranule0;
    SDF2SDF<FrameSideInfo> *sideInfoGranule1;
    SDF2SDF<GranuleData> *granuleData0;
    SDF2SDF<GranuleData> *granuleData1;
    SDF2SDF<GranuleType> *zippedGranuel0Out;
    SDF2SDF<GranuleType> *zippedGranuel1Out;
    SDF2SDF<FrameHeader> *headerMerge;
    SDF2SDF<FrameHeader> *headerChanule0Left;
    SDF2SDF<FrameHeader> *headerChanule0Right;
    SDF2SDF<FrameSideInfo> *sideInfoChanule0Left;
    SDF2SDF<FrameSideInfo> *sideInfoChanule0Right;
    SDF2SDF<ChanuleData> *chanuleData0Left;
    SDF2SDF<ChanuleData> *chanuleData0Right;
    SDF2SDF<FrameHeader> *headerChanule1Left;
    SDF2SDF<FrameHeader> *headerChanule1Right;
    SDF2SDF<FrameSideInfo> *sideInfoChanule1Left;
    SDF2SDF<FrameSideInfo> *sideInfoChanule1Right;
    SDF2SDF<ChanuleData> *chanuleData1Left;
    SDF2SDF<ChanuleData> *chanuleData1Right;
    SDF2SDF<ChanuleType> *zippedChanule0LOut;
    SDF2SDF<ChanuleType> *zippedChanule0ROut;
    SDF2SDF<ChanuleType> *zippedChanule1LOut;
    SDF2SDF<ChanuleType> *zippedChanule1ROut;
    SDF2SDF<ChanuleSamples> *samples_0_Left;
    SDF2SDF<ChanuleSamples> *samples_0_Right;
    SDF2SDF<ChanuleSamples> *samples_1_Left;
    SDF2SDF<ChanuleSamples> *samples_1_Right;
    SDF2SDF<VecType> *sync_0l_1l;
    SDF2SDF<VecType> *sync_0r_1r;
    SDF2SDF<VecType> *sync_1r_0r_predel, *sync_1r_0r_aftdel;
    SDF2SDF<VecType> *sync_1l_0l_predel, *sync_1l_0l_aftdel;
    SDF2SDF<MergeType> *zippedMerge;

    SC_CTOR(Top)
    {
        /* Create FIFOs */
        dummyloopi = new SDF2SDF<float>("dummyloopi",1);
        dummyloopo = new SDF2SDF<float>("dummyloopo",1);
        zippedInput = new SDF2SDF<InputType>("zippedInput",1);
        lastFrame = new SDF2SDF<bool>("lastFrame",1);
        headerGranule0 = new SDF2SDF<FrameHeader>("headerGranule0",1);
        headerGranule1 = new SDF2SDF<FrameHeader>("headerGranule1",1);
        sideInfoGranule0 = new SDF2SDF<FrameSideInfo>("sideInfoGranule0",1);
        sideInfoGranule1 = new SDF2SDF<FrameSideInfo>("sideInfoGranule1",1);
        granuleData0 = new SDF2SDF<GranuleData>("granuleData0",1);
        granuleData1 = new SDF2SDF<GranuleData>("granuleData1",1);
        zippedGranuel0Out = new SDF2SDF<GranuleType>("zippedGranuel0Out",1);
        zippedGranuel1Out = new SDF2SDF<GranuleType>("zippedGranuel1Out",1);
        headerMerge = new SDF2SDF<FrameHeader>("headerMerge",1);
        headerChanule0Left = new SDF2SDF<FrameHeader>("headerChanule0Left",1);
        headerChanule0Right = new SDF2SDF<FrameHeader>("headerChanule0Right",1);
        sideInfoChanule0Left = new SDF2SDF<FrameSideInfo>("sideInfoChanule0Left",1);
        sideInfoChanule0Right = new SDF2SDF<FrameSideInfo>("sideInfoChanule0Right",1);
        chanuleData0Left = new SDF2SDF<ChanuleData>("chanuleData0Left",1);
        chanuleData0Right = new SDF2SDF<ChanuleData>("chanuleData0Right",1);
        headerChanule1Left = new SDF2SDF<FrameHeader>("headerChanule1Left",1);
        headerChanule1Right = new SDF2SDF<FrameHeader>("headerChanule1Right",1);
        sideInfoChanule1Left = new SDF2SDF<FrameSideInfo>("sideInfoChanule1Left",1);
        sideInfoChanule1Right = new SDF2SDF<FrameSideInfo>("sideInfoChanule1Right",1);
        chanuleData1Left = new SDF2SDF<ChanuleData>("chanuleData1Left",1);
        chanuleData1Right = new SDF2SDF<ChanuleData>("chanuleData1Right",1);
        zippedChanule0LOut = new SDF2SDF<ChanuleType>("zippedChanuel0LOut",1);
        zippedChanule0ROut = new SDF2SDF<ChanuleType>("zippedChanuel0ROut",1);
        zippedChanule1LOut = new SDF2SDF<ChanuleType>("zippedChanuel1LOut",1);
        zippedChanule1ROut = new SDF2SDF<ChanuleType>("zippedChanuel1ROut",1);
        samples_0_Left = new SDF2SDF<ChanuleSamples>("samples_0_Left",1);
        samples_0_Right = new SDF2SDF<ChanuleSamples>("samples_0_Right",1);
        samples_1_Left = new SDF2SDF<ChanuleSamples>("samples_1_Left",1);
        samples_1_Right = new SDF2SDF<ChanuleSamples>("samples_1_Right",1);
        sync_0l_1l = new SDF2SDF<VecType>("sync_0l_1l",1);
        sync_0r_1r = new SDF2SDF<VecType>("sync_0r_1r",1);
        sync_1r_0r_predel = new SDF2SDF<VecType>("sync_1r_0r_predel",1);
        sync_1r_0r_aftdel = new SDF2SDF<VecType>("sync_1r_0r_aftdel",1);
        sync_1l_0l_predel = new SDF2SDF<VecType>("sync_1l_0l_predel",1);
        sync_1l_0l_aftdel = new SDF2SDF<VecType>("sync_1l_0l_aftdel",1);
        zippedMerge = new SDF2SDF<MergeType>("zippedMerge",1);
        a_ReadBitstreamAndExtractFrames = &add(new ReadBitstreamAndExtractFrames("ReadBitstreamAndExtractFrames",ReadBitstreamAndExtractFrames_func,1,1));
        a_ReadBitstreamAndExtractFrames->iport1(*dummyloopo);
        a_ReadBitstreamAndExtractFrames->oport1(*zippedInput);
        //
        array<size_t,9> inputUnzipperRates = {1,1,1,1,1,1,1,1,1};
        add_unzipN(*this, "InputUnzipper", inputUnzipperRates, *zippedInput,
            *dummyloopi, *lastFrame, *headerMerge, *headerGranule0, *sideInfoGranule0,
            *granuleData0, *headerGranule1, *sideInfoGranule1, *granuleData1);
        //
        a_DummyLoopDelay = &add(new delayn<float>("DummyLoopDelay",1,1));
        a_DummyLoopDelay->iport1(*dummyloopi);
        a_DummyLoopDelay->oport1(*dummyloopo);
        
        a_ProcessChanule0Left = &add(new ProcessChanule("ProcessChanuleZeroLeft0",ProcessChanuleZeroLeft_func,1,1,1,1,1));
        a_ProcessChanule0Left->iport1(*headerChanule0Left);
        a_ProcessChanule0Left->iport2(*sideInfoChanule0Left);
        a_ProcessChanule0Left->iport3(*chanuleData0Left);
        a_ProcessChanule0Left->iport4(*sync_1l_0l_aftdel);
        a_ProcessChanule0Left->oport1(*zippedChanule0LOut);
        //
        array<size_t,2> chanuleUnzipperRates = {1,1};
        add_unzipN(*this, "ChanuleUnzipperL0", chanuleUnzipperRates, *zippedChanule0LOut,
            *samples_0_Left, *sync_0l_1l);

        array<size_t,6> mergeZipperRates = {1,1,1,1,1,1};
        add_zipN(*this, "MergeZipper", mergeZipperRates, *zippedMerge,
            *samples_1_Right, *samples_0_Left, *lastFrame, *headerMerge, *samples_1_Left, *samples_0_Right);
        //
        a_Merge = &add(new sink<MergeType>("Merge", Merge_func));
        a_Merge->iport1(*zippedMerge);
        
        a_ProcessGranule0 = &add(new ProcessGranule("ProcessGranuleZero0",ProcessGranuleZero_func,1,1,1,1));
        a_ProcessGranule0->iport1(*headerGranule0);
        a_ProcessGranule0->iport2(*sideInfoGranule0);
        a_ProcessGranule0->iport3(*granuleData0);
        a_ProcessGranule0->oport1(*zippedGranuel0Out);
        //
        array<size_t,6> granuelUnzipperRates = {1,1,1,1,1,1};
        add_unzipN(*this, "GranuelUnzipper0", granuelUnzipperRates, *zippedGranuel0Out,
            *headerChanule0Left, *sideInfoChanule0Left, *chanuleData0Left,
            *headerChanule0Right, *sideInfoChanule0Right, *chanuleData0Right);

        a_ProcessGranule1 = &add(new ProcessGranule("ProcessGranuleOne0",ProcessGranuleOne_func,1,1,1,1));
        a_ProcessGranule1->iport1(*headerGranule1);
        a_ProcessGranule1->iport2(*sideInfoGranule1);
        a_ProcessGranule1->iport3(*granuleData1);
        a_ProcessGranule1->oport1(*zippedGranuel1Out);
        //
        add_unzipN(*this, "GranuelUnzipper1", granuelUnzipperRates, *zippedGranuel1Out,
            *headerChanule1Left, *sideInfoChanule1Left, *chanuleData1Left,
            *headerChanule1Right, *sideInfoChanule1Right, *chanuleData1Right);
        
        a_ProcessChanule0Right = &add(new ProcessChanule("ProcessChanuleZeroRight0",ProcessChanuleZeroRight_func,1,1,1,1,1));
        a_ProcessChanule0Right->iport1(*headerChanule0Right);
        a_ProcessChanule0Right->iport2(*sideInfoChanule0Right);
        a_ProcessChanule0Right->iport3(*chanuleData0Right);
        a_ProcessChanule0Right->iport4(*sync_1r_0r_aftdel);
        a_ProcessChanule0Right->oport1(*zippedChanule0ROut);
        //
        add_unzipN(*this, "ChanuleUnzipperR0", chanuleUnzipperRates, *zippedChanule0ROut,
            *samples_0_Right, *sync_0r_1r);
        
        a_ProcessChanule1Right = &add(new ProcessChanule("ProcessChanuleOneRight0",ProcessChanuleOneRight_func,1,1,1,1,1));
        a_ProcessChanule1Right->iport1(*headerChanule1Right);
        a_ProcessChanule1Right->iport2(*sideInfoChanule1Right);
        a_ProcessChanule1Right->iport3(*chanuleData1Right);
        a_ProcessChanule1Right->iport4(*sync_0r_1r);
        a_ProcessChanule1Right->oport1(*zippedChanule1ROut);
        //
        add_unzipN(*this, "ChanuleUnzipperR1", chanuleUnzipperRates, *zippedChanule1ROut,
            *samples_1_Right, *sync_1r_0r_predel);
        
        a_ProcessChanule1Left = &add(new ProcessChanule("ProcessChanuleOneLeft0",ProcessChanuleOneLeft_func,1,1,1,1,1));
        a_ProcessChanule1Left->iport1(*headerChanule1Left);
        a_ProcessChanule1Left->iport2(*sideInfoChanule1Left);
        a_ProcessChanule1Left->iport3(*chanuleData1Left);
        a_ProcessChanule1Left->iport4(*sync_0l_1l);
        a_ProcessChanule1Left->oport1(*zippedChanule1LOut);
        //
        add_unzipN(*this, "ChanuleUnzipperL1", chanuleUnzipperRates, *zippedChanule1LOut,
            *samples_1_Left, *sync_1l_0l_predel);
        
        a_ch_1r_0r = &add(new delayn<VecType>("ch_1r_0r",zeroVec,1));
        a_ch_1r_0r->iport1(*sync_1r_0r_predel);
        a_ch_1r_0r->oport1(*sync_1r_0r_aftdel);
        
        a_ch_1l_0l = &add(new delayn<VecType>("ch_1l_0l",zeroVec,1));
        a_ch_1l_0l->iport1(*sync_1l_0l_predel);
        a_ch_1l_0l->oport1(*sync_1l_0l_aftdel);
    }

public:
    // This used to call ForSyDe::CoMPSoCExport, a CompSOC-platform
    // specific exporter that no longer exists anywhere in the library --
    // XMLExport is the only one left. Switched to the same
    // start_of_simulation shape every other introspective example uses,
    // and guarded, since XMLExport only exists when introspection is
    // compiled in.
#ifdef FORSYDE_INTROSPECTION
    void start_of_simulation()
    {
        ForSyDe::XMLExport dumper("gen/");
        dumper.traverse(this);
    }
#endif
};
