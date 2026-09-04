/**********************************************************************
    * top.hpp -- the top module and testbench for the cruise control  *
    *          example                                                *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a heterogeneous system                *
    *                                                                 *
    * Usage:   Cruise control example                                 *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include <forsyde.hpp>
#include "plant.hpp"
#include "controller.hpp"

using namespace sc_core;
using namespace ForSyDe;

void sub_func(double& out, const double& inp1, const double& inp2)
{
    out = inp1 - inp2;
}

struct top : ForSyDe::composite
{
  CT::signal u, v, vout;
  SY::signal<double> r, e, du, dv;

  SC_CTOR(top)
  {
    add(new SY::sconstant("step", 1.0, 0))(r);

    #ifndef FORSYDE_WITH_GDB
    add(new SY::scomb2("sub1", sub_func))(e, r, dv);
    #else
    add(new SY::pipewrap2<double,double,double>("sub1", -1, "simulink"))(e, r, dv);
    #endif

    #ifndef FORSYDE_WITH_GDB
    add(new SY::smealy("controller1",
              controller_ns_func,
              controller_od_func,
              std::make_tuple(0.0, 0.0)
        ))(du, e);
    #else
    add(new SY::gdbwrap<double,double>("controller1",
              "software/controller"
        ))(du, e);
    #endif

    add(new SY2CT("d2a", sc_time(20,SC_MS), HOLD))(u, du);

    auto& plant1 = add(new plant("plant1"));
    plant1.u(u);
    plant1.v(v);
    plant1.v(vout);

    add(new CT2SY("a2d", sc_time(20,SC_MS)))(dv, v);

    add(new CT::traceSig("output", sc_time(20,SC_MS)))(vout);
  }
#ifdef FORSYDE_INTROSPECTION
    void start_of_simulation()
    {
        ForSyDe::XMLExport dumper("gen/");
        dumper.traverse(this);
    }
#endif
};
