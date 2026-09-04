// Force every process constructor this phase touches to be instantiated,
// whether or not any example uses it. A class template that nothing names
// is never really compiled -- which is how UT::zipsN kept a constructor
// that named its *output* port "iport1" and a bindInfo() that wrote that
// output port into boundInChans[0], clobbering the first input, and how
// SY::sunzip kept a prod() that called std::get<0>() on an abst_ext.
#include <forsyde.hpp>

using namespace ForSyDe;

template <typename P, typename... Args>
static void inst(const char* n, Args&&... args)
{
    new P(n, std::forward<Args>(args)...);
}

SC_MODULE(all)
{
    SC_CTOR(all)
    {
        // ---- SY -------------------------------------------------------
        inst<SY::comb <int,int>>            ("sy_comb",  [](abst_ext<int>&,const abst_ext<int>&){});
        inst<SY::comb2<int,int,int>>        ("sy_comb2", [](abst_ext<int>&,const abst_ext<int>&,const abst_ext<int>&){});
        inst<SY::comb3<int,int,int,int>>    ("sy_comb3", [](abst_ext<int>&,const abst_ext<int>&,const abst_ext<int>&,const abst_ext<int>&){});
        inst<SY::comb4<int,int,int,int,int>>("sy_comb4", [](abst_ext<int>&,const abst_ext<int>&,const abst_ext<int>&,const abst_ext<int>&,const abst_ext<int>&){});
        inst<SY::combX<int,int,3>>          ("sy_combX", [](abst_ext<int>&,const std::array<abst_ext<int>,3>&){});
        inst<SY::combN<int,int,int>>        ("sy_combN", [](abst_ext<int>&,const std::tuple<abst_ext<int>,abst_ext<int>>&){});
        inst<SY::combMN<std::tuple<int,int>,std::tuple<int,int,int>>>
                                            ("sy_combMN",[](std::tuple<abst_ext<int>,abst_ext<int>>&,
                                                            const std::tuple<abst_ext<int>,abst_ext<int>,abst_ext<int>>&){});
        inst<SY::zip   <int,char>>          ("sy_zip");
        inst<SY::zipX  <int,3>>             ("sy_zipX");
        inst<SY::zipN  <int,char,bool>>     ("sy_zipN");
        inst<SY::unzip <int,char>>          ("sy_unzip");
        inst<SY::unzipX<int,3>>             ("sy_unzipX");
        inst<SY::unzipN<int,char,bool>>     ("sy_unzipN");
        inst<SY::moore <int,int,int>>       ("sy_moore",
                                             [](int&,const int&,const abst_ext<int>&){},
                                             [](abst_ext<int>&,const int&){}, 0);
        inst<SY::mealy <int,int,int>>       ("sy_mealy",
                                             [](int&,const int&,const abst_ext<int>&){},
                                             [](abst_ext<int>&,const int&,const abst_ext<int>&){}, 0);

        // ---- SY, strict ------------------------------------------------
        // Same cores as above under detail::token_policy::strict. Several
        // of these had never been named by anything: SY::sunzip's prod()
        // called std::get<0>() on an abst_ext<std::tuple<...>>, which has
        // no tuple interface at all, so it could not have compiled.
        inst<SY::scomb <int,int>>            ("sy_scomb",  [](int&,const int&){});
        inst<SY::scomb2<int,int,int>>        ("sy_scomb2", [](int&,const int&,const int&){});
        inst<SY::scomb3<int,int,int,int>>    ("sy_scomb3", [](int&,const int&,const int&,const int&){});
        inst<SY::scomb4<int,int,int,int,int>>("sy_scomb4", [](int&,const int&,const int&,const int&,const int&){});
        inst<SY::scombX<int,int,3>>          ("sy_scombX", [](int&,const std::array<int,3>&){});
        inst<SY::scombN<int,int,int>>        ("sy_scombN", [](int&,const std::tuple<int,int>&){});
        inst<SY::scombMN<std::tuple<int,int>,std::tuple<int,int,int>>>
                                             ("sy_scombMN",[](std::tuple<int,int>&,
                                                              const std::tuple<int,int,int>&){});
        inst<SY::szip   <int,char>>          ("sy_szip");
        inst<SY::szipX  <int,3>>             ("sy_szipX");
        inst<SY::szipN  <int,char,bool>>     ("sy_szipN");
        inst<SY::sunzip <int,char>>          ("sy_sunzip");
        inst<SY::sunzipX<int,3>>             ("sy_sunzipX");
        inst<SY::sunzipN<int,char,bool>>     ("sy_sunzipN");
        inst<SY::sdelay <int>>               ("sy_sdelay", 0);
        inst<SY::sdelayn<int>>               ("sy_sdelayn", 0, 3u);
        inst<SY::sdpmap <int,int,4>>         ("sy_sdpmap", [](int&,const int&){});
        inst<SY::sdpreduce<int,4>>           ("sy_sdpreduce", [](int&,const int&,const int&){});
        inst<SY::sdpscan<int,int,4>>         ("sy_sdpscan", [](int&,const int&,const int&){}, 0);
        inst<SY::ssink  <int>>               ("sy_ssink", [](const int&){});
        inst<SY::sgroup <int>>               ("sy_sgroup", 4ul);
        inst<SY::smoore <int,int,int>>       ("sy_smoore", [](int&,const int&,const int&){},
                                              [](int&,const int&){}, 0);
        inst<SY::smealy <int,int,int>>       ("sy_smealy", [](int&,const int&,const int&){},
                                              [](int&,const int&,const int&){}, 0);
        inst<SY::sconstant<int>>             ("sy_sconstant", 1, 3ull);
        inst<SY::ssource<int>>               ("sy_ssource", [](int&,const int&){}, 0, 3ull);
        inst<SY::svsource<int>>              ("sy_svsource", std::vector<int>{1,2,3});

        // ---- SDF ------------------------------------------------------
        inst<SDF::comb <int,int>>            ("sdf_comb",  [](std::vector<int>&,const std::vector<int>&){}, 1u, 1u);
        inst<SDF::comb2<int,int,int>>        ("sdf_comb2", [](std::vector<int>&,const std::vector<int>&,const std::vector<int>&){}, 1u,1u,1u);
        inst<SDF::comb3<int,int,int,int>>    ("sdf_comb3", [](std::vector<int>&,const std::vector<int>&,const std::vector<int>&,const std::vector<int>&){}, 1u,1u,1u,1u);
        inst<SDF::comb4<int,int,int,int,int>>("sdf_comb4", [](std::vector<int>&,const std::vector<int>&,const std::vector<int>&,const std::vector<int>&,const std::vector<int>&){}, 1u,1u,1u,1u,1u);
        inst<SDF::combMN<std::tuple<int,int>,std::tuple<int,int,int>>>
                                             ("sdf_combMN",[](std::tuple<std::vector<int>,std::vector<int>>&,
                                                              const std::tuple<std::vector<int>,std::vector<int>,std::vector<int>>&){},
                                              std::array<size_t,2>{1,1}, std::array<size_t,3>{1,1,1});
        inst<SDF::zip   <int,char>>          ("sdf_zip", 1u, 1u);
        inst<SDF::zipN  <int,char,bool>>     ("sdf_zipN", std::array<size_t,3>{1,1,1});
        inst<SDF::unzip <int,char>>          ("sdf_unzip", 1u, 1u);
        inst<SDF::unzipN<int,char,bool>>     ("sdf_unzipN", std::array<size_t,3>{1,1,1});

        // ---- UT -------------------------------------------------------
        inst<UT::comb <int,int>>            ("ut_comb",  [](std::vector<int>&,const std::vector<int>&){}, 1u);
        inst<UT::comb2<int,int,int>>        ("ut_comb2", [](std::vector<int>&,const std::vector<int>&,const std::vector<int>&){}, 1u,1u);
        inst<UT::comb3<int,int,int,int>>    ("ut_comb3", [](std::vector<int>&,const std::vector<int>&,const std::vector<int>&,const std::vector<int>&){}, 1u,1u,1u);
        inst<UT::comb4<int,int,int,int,int>>("ut_comb4", [](std::vector<int>&,const std::vector<int>&,const std::vector<int>&,const std::vector<int>&,const std::vector<int>&){}, 1u,1u,1u,1u);
        inst<UT::zips  <int,char>>          ("ut_zips", 1u, 1u);
        inst<UT::zipsN <int,char,bool>>     ("ut_zipsN", std::array<size_t,3>{1,1,1});
        inst<UT::unzip <int,char>>          ("ut_unzip");
        inst<UT::unzipN<int,char,bool>>     ("ut_unzipN");
        inst<UT::scan  <int,int>>           ("ut_scan",
                                             [](unsigned int& n,const int&){n=1;},
                                             [](int&,const int&,const std::vector<int>&){}, 0);
        inst<UT::scand <int,int>>           ("ut_scand",
                                             [](unsigned int& n,const int&){n=1;},
                                             [](int&,const int&,const std::vector<int>&){}, 0);
        inst<UT::moore <int,int,int>>       ("ut_moore",
                                             [](unsigned int& n,const int&){n=1;},
                                             [](int&,const int&,const std::vector<int>&){},
                                             [](std::vector<int>&,const int&){}, 0);
        inst<UT::mealy <int,int,int>>       ("ut_mealy",
                                             [](unsigned int& n,const int&){n=1;},
                                             [](int&,const int&,const std::vector<int>&){},
                                             [](std::vector<int>&,const int&,const std::vector<int>&){}, 0);
        inst<UT::mooreMN<std::tuple<int,int>,std::tuple<int,int,int>,std::tuple<int>>>
                                            ("ut_mooreMN",
                                             [](std::array<size_t,3>& n,const std::tuple<int>&){n={1,1,1};},
                                             [](std::tuple<int>&,const std::tuple<int>&,
                                                const std::tuple<std::vector<int>,std::vector<int>,std::vector<int>>&){},
                                             [](std::tuple<std::vector<int>,std::vector<int>>&,
                                                const std::tuple<int>&){}, std::tuple<int>{0});
        inst<UT::mealyMN<std::tuple<int,int>,std::tuple<int,int,int>,std::tuple<int>>>
                                            ("ut_mealyMN",
                                             [](std::array<size_t,3>& n,const std::tuple<int>&){n={1,1,1};},
                                             [](std::tuple<int>&,const std::tuple<int>&,
                                                const std::tuple<std::vector<int>,std::vector<int>,std::vector<int>>&){},
                                             [](std::tuple<std::vector<int>,std::vector<int>>&,
                                                const std::tuple<int>&,
                                                const std::tuple<std::vector<int>,std::vector<int>,std::vector<int>>&){},
                                             std::tuple<int>{0});

        // ---- DT -------------------------------------------------------
        // The four timed Mealy variants of Jantsch section 5.2.1:
        // mealyT, mealyPT, mealyST and mealyTT. S and T are named by no
        // example, so this is the only thing that compiles them.
        inst<DT::mealy<int,int,int>>
            ("dt_mealy", [](size_t& n,const int&){n=1;},
             [](int&,const int&,const std::vector<abst_ext<int>>&){},
             [](std::vector<abst_ext<int>>&,const int&,const std::vector<abst_ext<int>>&){}, 0);
        inst<DT::mealyMN<std::tuple<int,int>,std::tuple<int,int,int>,int>>
            ("dt_mealyMN", [](size_t& n,const int&){n=1;},
             [](int&,const int&,const std::tuple<std::vector<abst_ext<int>>,std::vector<abst_ext<int>>,std::vector<abst_ext<int>>>&){},
             [](std::tuple<std::vector<abst_ext<int>>,std::vector<abst_ext<int>>>&,const int&,
                const std::tuple<std::vector<abst_ext<int>>,std::vector<abst_ext<int>>,std::vector<abst_ext<int>>>&){}, 0);
        inst<DT::P::mealy<int,int,int>>
            ("dt_p_mealy", [](size_t& n,const int&){n=1;},
             [](int&,const int&,const std::vector<int>&){},
             [](std::vector<int>&,const int&,const std::vector<int>&){}, 0);
        inst<DT::P::mealyMN<std::tuple<int,int>,std::tuple<int,int,int>,int>>
            ("dt_p_mealyMN", [](size_t& n,const int&){n=1;},
             [](int&,const int&,const std::tuple<std::vector<int>,std::vector<int>,std::vector<int>>&){},
             [](std::tuple<std::vector<int>,std::vector<int>>&,const int&,
                const std::tuple<std::vector<int>,std::vector<int>,std::vector<int>>&){}, 0);
        inst<DT::S::mealy<int,int,int>>
            ("dt_s_mealy", [](size_t& n,const int&){n=1;},
             [](int&,const int&,const std::vector<int>&){},
             [](std::vector<int>&,const int&,const std::vector<int>&){}, 0);
        inst<DT::T::mealy<int,int,int>>
            ("dt_t_mealy", [](size_t& n,size_t& t,const int&){n=1;t=4;},
             [](int&,const int&,const std::vector<int>&){},
             [](std::vector<int>&,const int&,const std::vector<int>&){}, 0);

        // ---- DDE ------------------------------------------------------
        // Distinct types again: DDE::zip<T1,T2> gave its second input an
        // abst_ext<T1> when no value was available for it, which only
        // compiles when the two types are the same -- and the one model
        // that uses it zips int with int.
        inst<DDE::comb <char,int>>          ("dde_comb",  [](abst_ext<char>&,const int&){});
        inst<DDE::comb2<char,int,short>>    ("dde_comb2",
            [](abst_ext<char>&,const abst_ext<int>&,const abst_ext<short>&){});
        inst<DDE::delay<int>>               ("dde_delay", abst_ext<int>(0), sc_core::sc_time(1,sc_core::SC_NS));
        inst<DDE::mealy<int,short,char>>
            ("dde_mealy", [](short&,const short&,const ttn_event<int>&){},
             [](abst_ext<char>&,const short&,const ttn_event<int>&){},
             short{0}, sc_core::sc_time(1,sc_core::SC_NS));
        inst<DDE::zip   <int,char>>         ("dde_zip");
        inst<DDE::zipX  <int,3>>            ("dde_zipX");
        inst<DDE::unzip <int,char>>         ("dde_unzip");
        inst<DDE::unzipX<int,3>>            ("dde_unzipX");
        inst<DDE::zipN  <int,char,bool>>    ("dde_zipN");
        inst<DDE::unzipN<int,char,bool>>    ("dde_unzipN");

        // ---- CT -------------------------------------------------------
        // CT is monomorphic on CTTYPE (double), so there are no type
        // parameters to get wrong here -- only the constructors to compile.
        inst<CT::comb >                     ("ct_comb",  [](CTTYPE&,const CTTYPE&){});
        inst<CT::comb2>                     ("ct_comb2", [](CTTYPE&,const CTTYPE&,const CTTYPE&){});
        inst<CT::combX<3>>                  ("ct_combX", [](CTTYPE&,const std::array<CTTYPE,3>&){});
        inst<CT::delay>                     ("ct_delay", sc_core::sc_time(1,sc_core::SC_NS));
        inst<CT::shift>                     ("ct_shift", sc_core::sc_time(1,sc_core::SC_NS));

        // The library's own composite processes -- built from other CT
        // constructors rather than being one themselves, and previously
        // the one place in ct_lib.hpp that predated ForSyDe::composite:
        // plain SC_MODULEs with no operator() of their own, so binding
        // one positionally silently fell through to SystemC's own
        // same-named positional bind instead of failing to compile.
        inst<CT::gaussian>("ct_gaussian", 0.01, 0.0, sc_core::sc_time(1,sc_core::SC_MS));
        inst<CT::filter>  ("ct_filter", std::vector<CTTYPE>{1.0}, std::vector<CTTYPE>{1.0,0.0},
                           sc_core::sc_time(1,sc_core::SC_MS));
        inst<CT::filterf> ("ct_filterf", std::vector<CTTYPE>{1.0}, std::vector<CTTYPE>{1.0,0.0},
                           sc_core::sc_time(1,sc_core::SC_MS));
        inst<CT::pif>     ("ct_pif", 1.0, 1.0, sc_core::sc_time(1,sc_core::SC_MS));

        // ---- SADF -----------------------------------------------------
        // The scenario tables differ in shape per arity: a pair of
        // scalars, an array plus a scalar, then two arrays.
        inst<SADF::kernel<int,int,int>>
            ("sadf_kernel", [](std::vector<int>&,const int&,const std::vector<int>&){},
             std::map<int,std::tuple<size_t,size_t>>{{0,{1,1}}});
        inst<SADF::kernel2<int,int,int,int>>
            ("sadf_kernel2", [](std::vector<int>&,const int&,const std::vector<int>&,const std::vector<int>&){},
             std::map<int,std::tuple<std::array<size_t,2>,size_t>>{{0,{{1,1},1}}});
        inst<SADF::kernelMN<std::tuple<int,int>,int,std::tuple<int,int,int>>>
            ("sadf_kernelMN", [](std::tuple<std::vector<int>,std::vector<int>>&,const int&,
                                 const std::tuple<std::vector<int>,std::vector<int>,std::vector<int>>&){},
             std::map<int,std::tuple<std::array<size_t,3>,std::array<size_t,2>>>{{0,{{1,1,1},{1,1}}}});
        // Distinct types on purpose: detector is <output, input, scenario>,
        // and instantiating it with three ints would hide a constructor
        // that named them in the wrong order -- which is exactly the
        // mistake the example suite had to catch instead.
        inst<SADF::detector<char,int,short>>
            ("sadf_detector", [](short&,const short&,const std::vector<int>&){},
             [](std::vector<char>&,const short&,const std::vector<int>&){},
             std::map<short,size_t>{{0,1}}, short{0}, size_t{1});
        inst<SADF::detectorMN<std::tuple<int,int>,std::tuple<int,int,int>,int>>
            ("sadf_detectorMN",
             [](int&,const int&,const std::tuple<std::vector<int>,std::vector<int>,std::vector<int>>&){},
             [](std::tuple<std::vector<int>,std::vector<int>>&,const int&,
                const std::tuple<std::vector<int>,std::vector<int>,std::vector<int>>&){},
             std::map<int,std::array<size_t,2>>{{0,{1,1}}}, 0,
             std::array<size_t,3>{1,1,1});

        // ---- MoC interfaces -------------------------------------------
        // Every one of these is a template that no example names, which
        // is the condition under which this library has repeatedly
        // shipped a class that does not compile or does not work. The
        // twelve strip/insert pairs are one class each, so one of each
        // shape is enough; group is DT to SY and has only the one.
        inst<MI::strip<moc_id::SY, moc_id::UT, int>>("mi_strip_sy_ut");
        inst<MI::strip<moc_id::DT, moc_id::SDF, int>>("mi_strip_dt_sdf");
        inst<MI::insert<moc_id::SADF, moc_id::SY, int>>("mi_insert_sadf_sy", 2ul);
        inst<MI::insert<moc_id::SY, moc_id::DT, int>>("mi_insert_sy_dt", 3ul);
        inst<MI::group<moc_id::DT, moc_id::SY, int>>("mi_group_dt_sy", 3ul);
    }
};

int sc_main(int, char*[])
{
    // Elaboration only -- this never runs a simulation. Building it is
    // the whole point; sc_start() would deadlock on unbound ports.
    new all("all");
    return 0;
}
