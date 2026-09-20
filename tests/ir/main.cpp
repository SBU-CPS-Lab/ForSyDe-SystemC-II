// tests/ir -- the intermediate representation, as a value.
//
// ir::build walks an elaborated model and returns an ir::model (see
// src/forsyde/ir.hpp). Until 3a that structure existed only inside
// XMLExport's traversal, so the only way to check it was to read the
// XML it produced -- which checks the backend and the graph at once and
// cannot tell you which of the two is wrong.
//
// This pins the graph on its own terms. It builds a model whose shape
// is known by construction -- two levels of hierarchy, a composite
// instance with boundary ports, leaf processes with constructor
// arguments, and signals crossing between them -- dumps the IR
// canonically, and then checks the invariants a *consumer* of the IR
// depends on and no backend happens to exercise:
//
//   * order covers each node, port and channel exactly once, so a
//     backend that walks `order` and one that walks the three lists see
//     the same graph;
//   * every channel endpoint names a node that exists in the same
//     network, so an edge cannot point at nothing;
//   * every boundary port names a node in its own network, likewise;
//   * every composite node's `sub` resolves to a real network, no two
//     composite nodes claim the same one, and every network but the top
//     is claimed by exactly one.
//
// Those are the properties that make the IR a graph rather than three
// lists that happen to sit next to each other, and they are what 3b
// (reflection always compiled in) and 3d (describe/instantiate) have to
// keep true while changing how the thing is populated.
//
// Both harness configurations now produce the same dump, and that is
// the point of 3b. The structural record used to be gated on
// FORSYDE_INTROSPECTION, so an ordinary build of a model held no
// structure at all and the only way to get an IR was to also ask for
// the XML export. Reflection is its own switch now, on by default, so
// the "off" row here -- which is only "without the XML backend" --
// has an IR exactly like the "on" row. What is left of the #else below
// is the FORSYDE_NO_REFLECTION opt-out, which tests/no_reflection is
// the thing that actually compiles.
#include <forsyde.hpp>

#include <iostream>
#include <set>
#include <string>
#include <vector>

using namespace ForSyDe;

void scale_func(int& out, const int& inp) {out = inp * 2;}
void offset_func(int& out, const int& inp) {out = inp + 1;}
void report_func(const int& inp) {std::cout << "out " << inp << "\n";}

//! A composite with two leaves in series, entered and left by port
FORSYDE_COMPOSITE(inner)
{
    SY::in_port<int>  iport1;
    SY::out_port<int> oport1;

    SY::signal<int> mid;

    SC_CTOR(inner)
    {
        add(new SY::scomb("scale", scale_func))(mid, iport1);
        add(new SY::scomb("offset", offset_func))(oport1, mid);
    }
};

#ifdef FORSYDE_REFLECTION

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "ok   " : "FAIL ") << what << "\n";
    if (!ok) ++failures;
}

//! Render one network in a form that is stable across runs
void dump_network(const ir::model& m, std::size_t index)
{
    const ir::network& net = m.networks[index];
    std::cout << "network " << index << ": " << net.name
              << " (instance " << net.instance << ")\n";

    for (const auto& slot : net.order)
    {
        switch (slot.kind)
        {
            case ir::child_kind::node:
            {
                const ir::node& n = net.nodes[slot.index];
                if (n.kind == ir::node_kind::leaf)
                {
                    std::cout << "  leaf      " << n.name
                              << " : " << n.pc_moc << "::" << n.pc_name << "\n";
                    for (const auto& p : n.params)
                        std::cout << "    arg  " << p.name << " = " << p.value << "\n";
                }
                else
                {
                    std::cout << "  composite " << n.name
                              << " : " << n.component << " -> network " << n.sub << "\n";
                }
                for (const auto& p : n.ports)
                    std::cout << "    port " << p.name
                              << " " << (p.dir == ir::direction::in ? "in " : "out")
                              << " " << p.moc << " " << p.type << "\n";
                break;
            }
            case ir::child_kind::port:
            {
                const ir::port& p = net.ports[slot.index];
                std::cout << "  port      " << p.name
                          << " " << (p.dir == ir::direction::in ? "in " : "out")
                          << " " << p.moc << " " << p.type
                          << " -> " << p.bound_process << "." << p.bound_port << "\n";
                break;
            }
            case ir::child_kind::channel:
            {
                const ir::channel& c = net.channels[slot.index];
                std::cout << "  channel   " << c.name
                          << " " << c.moc << " " << c.type << " : "
                          << c.source << "." << c.source_port << " -> "
                          << c.target << "." << c.target_port << "\n";
                break;
            }
        }
    }
}

//! The invariants a consumer of the IR relies on
void check_invariants(const ir::model& m)
{
    check(!m.networks.empty(), "model has at least one network");

    std::set<std::size_t> claimed_subs;

    for (std::size_t i = 0; i < m.networks.size(); i++)
    {
        const ir::network& net = m.networks[i];
        const std::string where = "network " + std::to_string(i) + " (" + net.name + "): ";

        // order covers every child exactly once
        std::vector<bool> seen_node(net.nodes.size(), false);
        std::vector<bool> seen_port(net.ports.size(), false);
        std::vector<bool> seen_chan(net.channels.size(), false);
        bool double_counted = false;
        for (const auto& slot : net.order)
        {
            std::vector<bool>& seen =
                slot.kind == ir::child_kind::node ? seen_node :
                slot.kind == ir::child_kind::port ? seen_port : seen_chan;
            if (slot.index >= seen.size() || seen[slot.index]) double_counted = true;
            else seen[slot.index] = true;
        }
        const bool complete =
            net.order.size() == net.nodes.size() + net.ports.size() + net.channels.size();
        check(!double_counted && complete,
              where + "order covers every node, port and channel exactly once");

        // the names a network's own edges refer to
        std::set<std::string> node_names;
        for (const auto& n : net.nodes) node_names.insert(n.name);

        bool endpoints_resolve = true;
        for (const auto& c : net.channels)
            if (!node_names.count(c.source) || !node_names.count(c.target))
                endpoints_resolve = false;
        check(endpoints_resolve, where + "every channel endpoint names a node in this network");

        bool bounds_resolve = true;
        for (const auto& p : net.ports)
            if (!node_names.count(p.bound_process)) bounds_resolve = false;
        check(bounds_resolve, where + "every boundary port names a node in this network");

        // composite nodes point at real, distinct networks
        bool subs_ok = true;
        for (const auto& n : net.nodes)
        {
            if (n.kind == ir::node_kind::composite)
            {
                if (n.sub >= m.networks.size()) subs_ok = false;
                else if (!claimed_subs.insert(n.sub).second) subs_ok = false;
            }
            else if (n.sub != ir::node::npos) subs_ok = false;
        }
        check(subs_ok, where + "composite nodes resolve to distinct networks, leaves to none");
    }

    // every network but the top is some composite's contents
    check(claimed_subs.size() + 1 == m.networks.size(),
          "every network except the top is claimed by exactly one composite node");
}

#endif

FORSYDE_COMPOSITE(top)
{
    SY::signal<int> src, inner_out;

    SC_CTOR(top)
    {
        add(new SY::sconstant("src", 3, 4))(src);

        auto& inner1 = add(new inner("inner1"));
        inner1.iport1(src);
        inner1.oport1(inner_out);

        add(new SY::ssink("snk", report_func))(inner_out);
    }

    void start_of_simulation()
    {
#ifdef FORSYDE_REFLECTION
        // The IR outlives the call that built it, which is the whole
        // point of 3a: it is a value, not a traversal.
        const ir::model m = ir::build(this);

        for (std::size_t i = 0; i < m.networks.size(); i++) dump_network(m, i);
        std::cout << "--\n";
        check_invariants(m);
#else
        std::cout << "IR unavailable: built with FORSYDE_NO_REFLECTION,\n";
        std::cout << "so the members ir::build reads are not compiled in.\n";
#endif
    }
};

//! A container that is a plain SC_MODULE rather than a ForSyDe::composite
/*! 3e made ir::build read a composite's own record of what was built
 * inside it, instead of walking SystemC's child objects and classifying
 * what came back. A plain SC_MODULE keeps no such record, so build()
 * falls back to the walk for one -- and nothing in this repository is
 * written this way, which is exactly the condition under which a path
 * rots. A model outside this repository may well be, so the fallback is
 * exercised here on purpose rather than assumed to work.
 */
SC_MODULE(plain_container)
{
    SY::signal<int> a, b;

    SC_CTOR(plain_container)
    {
        auto* s = new SY::sconstant("psrc", 5, 3); (*s)(a);
        auto* c = new SY::comb<int,int>("pcomb",
            [](abst_ext<int>& o, const abst_ext<int>& i)
            {o = abst_ext<int>(unsafe_from_abst_ext(i) + 1);});
        (*c)(b, a);
        auto* k = new SY::ssink("psnk", [](const abst_ext<int>&){}); (*k)(b);
    }
};

int sc_main(int, char*[])
{
    top t("top1");
    plain_container pc("plain1");

    // A signal nothing binds to used to be a segmentation fault with
    // nothing said, because introspective_channel's two endpoint
    // pointers were left uninitialised and every reader dereferences
    // both. Leaving an unused signal behind while editing a model is an
    // ordinary thing to do, so ir::build names it instead. Built here,
    // outside any module, so that it belongs to no composite's contents
    // and before sc_start, because SystemC rejects a channel created
    // once elaboration is over.
    SY::signal<int> orphan;

    sc_core::sc_start();

#ifdef FORSYDE_REFLECTION
    // The fallback path: same graph, recovered the old way.
    const ir::model pm = ir::build(&pc);
    check(pm.networks.size() == 1, "a plain SC_MODULE yields one network");
    check(pm.top().nodes.size() == 3,
          "the walk fallback finds every process in a non-composite container");
    check(pm.top().channels.size() == 2,
          "the walk fallback finds every signal in a non-composite container");

    check(orphan.iport == nullptr && orphan.oport == nullptr,
          "an unbound signal's endpoints are null rather than uninitialised");
#endif

#ifdef FORSYDE_REFLECTION
    std::cout << (failures == 0 ? "all invariants hold\n" : "INVARIANTS BROKEN\n");
    return failures == 0 ? 0 : 1;
#else
    return 0;
#endif
}
