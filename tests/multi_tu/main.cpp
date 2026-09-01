/**********************************************************************
    * main.cpp -- the first translation unit                          *
    *                                                                 *
    * Purpose: Half of the multi-translation-unit regression test.    *
    *          See README.md in this directory.                       *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include "sub.hpp"
#include <iostream>

using namespace ForSyDe;

SC_MODULE(top)
{
    SY::signal<sample> src, result;

    SC_CTOR(top)
    {
        SY::make_ssource("source1", [](sample& out, const sample& in) {
            out = sample(in.value + 1);
        }, sample(1), 5, src);

        auto sub1 = new sub("sub1");
        sub1->iport(src);
        sub1->oport(result);

        SY::make_ssink("sink1", [](const sample& out) {
            std::cout << "result: " << out.value << std::endl;
        }, result);
    }

#ifdef FORSYDE_INTROSPECTION
    void start_of_simulation()
    {
        ForSyDe::XMLExport dumper("gen/");
        dumper.traverse(this);
    }
#endif
};

int sc_main(int argc, char **argv)
{
    top top1("top1");

    sc_start();

    return 0;
}
