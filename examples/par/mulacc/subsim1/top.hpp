/**********************************************************************
    * Top.hpp -- the top module and testbench for the mulacc example  *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Demonstration of a simple sequential processes.        *
    *                                                                 *
    * Usage:   Parallel MulAcc example                                *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#include "mulacc.hpp"
#include <iostream>

using namespace ForSyDe::SY;

struct top : ForSyDe::composite
{
    SY2SY<int> srca, srcb, result;

    SC_CTOR(top)
    {
        int world_rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
        int world_size;
        MPI_Comm_size(MPI_COMM_WORLD, &world_size );
        int partner_rank = (world_rank + 1) % 2;

        add(new constant("constant1", abst_ext<int>(3), 10))(srca);

        add(new receiver<int>("receiver1", partner_rank, 1))(srcb);

        auto& mulacc1 = add(new mulacc("mulacc1"));
        mulacc1.a(srca);
        mulacc1.b(srcb);
        mulacc1.result(result);

        add(new sender<int>("sender1", partner_rank, 0))(result);
    }
#ifdef FORSYDE_INTROSPECTION
    void start_of_simulation()
    {
        ForSyDe::XMLExport dumper("subsim1/gen/");
        dumper.traverse(this);
    }
#endif
};
