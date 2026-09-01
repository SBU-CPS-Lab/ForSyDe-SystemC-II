// Force every class touched by 2a to be instantiated, whether or not any
// example uses it. A class template that nothing names is never compiled,
// which is how UT::zipsN kept a constructor that named its *output* port
// "iport1" and a bindInfo() that wrote that output port into
// boundInChans[0], clobbering the first input.
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
    }
};

int sc_main(int, char*[])
{
    // Elaboration only -- this never runs a simulation. Building it is
    // the whole point; sc_start() would deadlock on unbound ports.
    new all("all");
    return 0;
}
