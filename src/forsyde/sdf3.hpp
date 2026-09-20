/**********************************************************************
    * sdf3.hpp -- export a pure-SDF model as an SDF3 application graph *
    *                                                                  *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)              *
    *                                                                  *
    * Purpose: A second view over ForSyDe::ir::model, for the static    *
    *          dataflow analysis tools this project has always cited   *
    *                                                                  *
    * Usage:   This file is included automatically                     *
    *                                                                  *
    * License: BSD3                                                    *
    *******************************************************************/

#ifndef FORSYDE_SDF3_HPP
#define FORSYDE_SDF3_HPP

/*! \file sdf3.hpp
 * \brief Flattens an ir::model into an SDF3 "sdf" applicationGraph
 *
 * SDF3 (http://www.es.ele.tue.nl/sdf3/) is the analysis tool this
 * project's introspection XML has been described as feeding since j1 --
 * consistency, deadlock, repetition vectors, throughput, buffer sizing
 * -- and unlike the dot view (f2dot, an external tool that already
 * reads ForSyDe's own XML directly and needed nothing new here), SDF3's
 * "sdf" format has no notion of hierarchy at all: one flat actor graph,
 * every actor with a fixed per-port rate declared up front. Producing
 * one is therefore two real jobs, not a rendering:
 *
 *   * flatten() eliminates every composite boundary, resolving a
 *     composite instance's port down to whichever leaf and port it is
 *     ultimately wired to -- recursively, since a composite can contain
 *     another composite -- and qualifies leaf names by the instance
 *     path to them so that two different composite instances of the
 *     same component cannot collide.
 *   * the per-port rates flatten() needs did not exist on the IR at
 *     all before this: ForSyDe::process::rates() (abssemantics.hpp) and
 *     ForSyDe::process::initial_tokens() are new virtual hooks,
 *     overridden only by the SDF comb/zip/unzip families and by
 *     SDF::delay/delayn respectively, which is what makes them
 *     reachable through ir::build's one polymorphic ForSyDe::process*
 *     the same way forsyde_kind() and the bound-channel vectors always
 *     have been.
 *
 * What this does not attempt: SADF (a genuinely different SDF3 XSD,
 * scenario-indexed, not attempted here), and any process whose rate is
 * not statically known at all -- flatten() refuses rather than guess,
 * naming the exact process and port that blocked it, the same way
 * xml.hpp's moc_attribute() refuses a MoC string it cannot place rather
 * than emit something plausible-looking and wrong.
 */

#include "config.hpp"

#include <cstddef>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef FORSYDE_REFLECTION
#include "ir.hpp"
#endif

namespace ForSyDe
{

namespace sdf3
{

//! One port of a flattened actor: a name, a direction, a fixed rate
struct port
{
    std::string name;
    bool is_output;
    std::size_t rate;
};

//! One actor in the flattened graph
/*! name is the instance path ("top__mulacc1__add1" for a leaf three
 * levels deep); type is its process-constructor name ("scombMN"),
 * which is what SDF3 shows as the actor's kind. type is descriptive
 * only -- SDF3 schedules on rates, not on what a "scombMN" is.
 */
struct actor
{
    std::string name;
    std::string type;
    std::vector<port> ports;
};

//! One channel in the flattened graph, both ends already resolved to leaves
struct channel
{
    std::string name;
    std::string src_actor, src_port;
    std::string dst_actor, dst_port;
    std::size_t initial_tokens = 0;
};

//! A flat SDF3 "sdf" application graph
struct graph
{
    std::string name;
    std::vector<actor> actors;
    std::vector<channel> channels;
};

#ifdef FORSYDE_REFLECTION

namespace detail
{

//! Where a channel endpoint ultimately leads, after resolving every
//! composite boundary it passes through
struct resolved
{
    std::string actor;
    std::string port;
    //! The leaf's own initial_tokens() -- meaningful only when this is
    //! used as a channel's *source*; a target's is never read.
    std::size_t initial_tokens = 0;
};

//! Follow (node_name, port_name) in network net_idx down to a leaf
/*! If node_name names a leaf directly, that is the answer. If it names
 * a composite instance, port_name is one of *its* ports -- found among
 * m.networks[node.sub].ports, whose bound_process/bound_port say what
 * it is wired to *inside* that composite -- so the resolution recurses
 * one level down, with the qualified prefix extended by the instance's
 * own name. A composite containing a composite containing a leaf
 * resolves through two such steps.
 */
inline resolved resolve(const ir::model& m, std::size_t net_idx,
                         const std::string& node_name, const std::string& port_name,
                         const std::string& prefix)
{
    const ir::network& net = m.networks[net_idx];
    for (const auto& n : net.nodes)
    {
        if (n.name != node_name) continue;
        if (n.kind == ir::node_kind::leaf)
            return {prefix + n.name, port_name, n.initial_tokens};

        const ir::network& sub = m.networks[n.sub];
        for (const auto& p : sub.ports)
            if (p.name == port_name)
                return resolve(m, n.sub, p.bound_process, p.bound_port,
                                prefix + n.name + "__");

        throw std::runtime_error("sdf3: composite '" + prefix + n.name +
            "' (component " + n.component + ") has no port '" + port_name + "'");
    }
    throw std::runtime_error("sdf3: network '" + net.name +
        "' has no node '" + node_name + "'");
}

//! This leaf's declared rate for one port, or the default for a
//! process whose class never leaves a port's rate unspecified
/*! Every SDF process this library has that does not override
 * ForSyDe::process::rates() -- source, sink, constant, vsource,
 * file_source, file_sink, delay, delayn -- fires exactly one token
 * through each of its ports every firing; that is a structural fact
 * about those eight classes, checked by reading them, not a guess
 * applied to whatever happens to be missing. It is *only* applied to
 * an SDF-family leaf (n.pc_moc == "SDF"): a port with no declared rate
 * on anything else is what makes flatten() refuse the model, below.
 */
inline std::size_t rate_of(const ir::node& n, const ir::port& p)
{
    if (p.rate) return *p.rate;
    if (n.pc_moc == "SDF") return 1;
    throw std::runtime_error("sdf3: " + n.pc_moc + "::" + n.pc_name + " '" +
        n.name + "' has no declared rate on port '" + p.name +
        "', and is not an SDF process this exporter knows a default for");
}

//! Depth-first walk collecting flattened actors and resolved channels
inline void flatten_into(const ir::model& m, std::size_t net_idx,
                          const std::string& prefix, graph& g)
{
    const ir::network& net = m.networks[net_idx];

    for (const auto& n : net.nodes)
    {
        if (n.kind == ir::node_kind::leaf)
        {
            if (n.pc_moc != "SDF")
                throw std::runtime_error("sdf3: '" + prefix + n.name + "' is " +
                    n.pc_moc + "::" + n.pc_name + ", not SDF -- the sdf3 'sdf' "
                    "format can only represent a pure SDF process network");

            actor a;
            a.name = prefix + n.name;
            a.type = n.pc_name;
            for (const auto& p : n.ports)
                a.ports.push_back({p.name, p.dir == ir::direction::out, rate_of(n, p)});
            g.actors.push_back(std::move(a));
        }
        else
        {
            flatten_into(m, n.sub, prefix + n.name + "__", g);
        }
    }

    for (const auto& c : net.channels)
    {
        const resolved src = resolve(m, net_idx, c.source, c.source_port, prefix);
        const resolved dst = resolve(m, net_idx, c.target, c.target_port, prefix);

        // The token count is the *source leaf's* initial_tokens()
        // (SDF::delay/delayn only), which resolve() carried along from
        // wherever it actually found that leaf -- it survives however
        // many composite boundaries were just resolved through because
        // it lives on the node, not on any one channel along the way.
        // See ir::node::initial_tokens's own comment.
        g.channels.push_back({
            "ch" + std::to_string(g.channels.size()),
            src.actor, src.port, dst.actor, dst.port, src.initial_tokens});
    }
}

//! Give every output port at most one channel, the way SDF3 requires
/*! ForSyDe's readers(...) lets one output be read by more than one
 * downstream signal -- ordinary, legal fan-out, and toysdfMN's own
 * compAvg uses it (outs(readers(oport1, din))): the averager's one
 * output both leaves the composite and feeds the delay that closes its
 * feedback loop. SDF3's port model has no such thing -- a port is one
 * end of exactly one channel -- which is not a limitation of this
 * exporter, it is the standard dataflow-tool answer: a fan-out is an
 * explicit broadcast actor, one input and N outputs, all at the
 * source's own rate, since duplicating a token does not change how
 * many of them there are.
 *
 * A post-pass over the flattened graph rather than something done
 * during flatten_into's own walk, because the two channels a fan-out
 * produces are typically discovered at *different* levels of the
 * hierarchy -- one resolved directly inside the composite that owns
 * the port, the other only after resolving down through the composite
 * boundary from its parent -- so there is no single point during the
 * recursive walk where both are in hand together.
 */
inline void insert_fanouts(graph& g)
{
    std::map<std::pair<std::string,std::string>, std::vector<std::size_t>> by_source;
    for (std::size_t i = 0; i < g.channels.size(); i++)
        by_source[{g.channels[i].src_actor, g.channels[i].src_port}].push_back(i);

    std::vector<channel> rewritten;
    for (const auto& [key, idxs] : by_source)
    {
        if (idxs.size() == 1) {rewritten.push_back(g.channels[idxs[0]]); continue;}

        std::size_t rate = 0;
        for (const auto& a : g.actors)
            if (a.name == key.first)
                for (const auto& p : a.ports)
                    if (p.name == key.second) rate = p.rate;

        const std::string fname = key.first + "__" + key.second + "__fanout";
        actor fa;
        fa.name = fname;
        fa.type = "fanout";
        fa.ports.push_back({"in", false, rate});
        for (std::size_t k = 0; k < idxs.size(); k++)
            fa.ports.push_back({"out" + std::to_string(k), true, rate});
        g.actors.push_back(std::move(fa));

        // The real source's initial_tokens applies once, to the single
        // stream leaving it -- not to each of the N branches, which is
        // what carrying it on every rewritten channel would silently
        // duplicate it into.
        rewritten.push_back({"", key.first, key.second, fname, "in",
                              g.channels[idxs[0]].initial_tokens});
        for (std::size_t k = 0; k < idxs.size(); k++)
        {
            const channel& orig = g.channels[idxs[k]];
            rewritten.push_back({"", fname, "out" + std::to_string(k),
                                  orig.dst_actor, orig.dst_port, 0});
        }
    }

    for (std::size_t i = 0; i < rewritten.size(); i++)
        rewritten[i].name = "ch" + std::to_string(i);
    g.channels = std::move(rewritten);
}

} // namespace detail

//! Flatten an elaborated model's IR into a flat SDF3 application graph
/*! Throws std::runtime_error, naming the offending process or port,
 * if any leaf in the model is not SDF, or is SDF but has no rate this
 * exporter can determine (detail::rate_of). Never partially succeeds:
 * either the whole graph is returned, or nothing is.
 *
 * Every output port ends up the source of at most one channel
 * (detail::insert_fanouts): any that started with more, from
 * readers(...) fan-out, gets an explicit broadcast actor instead,
 * which is the representation SDF3 itself expects a fan-out in, not a
 * limitation being routed around.
 */
inline graph flatten(const ir::model& m)
{
    graph g;
    g.name = m.top().name;
    detail::flatten_into(m, 0, "", g);
    detail::insert_fanouts(g);
    return g;
}

//! Write a flattened graph as SDF3 XML
/*! The shape is the one SDF3's own testbench models use: one
 * <sdf3><applicationGraph><sdf>...</sdf></applicationGraph></sdf3>
 * document, actors each listing their ports with a rate attribute, and
 * channels naming both endpoints by actor and port. Verified against
 * the real tool, not only the schema: sdf3analysis-sdf reads a file
 * written by this function and reports repetition vectors and
 * consistency that match what the model's own token rates say -- see
 * tests/sdf3/README.md.
 */
inline void write(const graph& g, std::ostream& os)
{
    os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
       << "<sdf3 type=\"sdf\" version=\"1.0\"\n"
       << "    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
       << "    xsi:noNamespaceSchemaLocation="
          "\"http://www.es.ele.tue.nl/sdf3/xsd/sdf3-sdf.xsd\">\n"
       << "  <applicationGraph name=\"" << g.name << "\">\n"
       << "    <sdf name=\"" << g.name << "\" type=\"" << g.name << "\">\n";

    for (const auto& a : g.actors)
    {
        os << "      <actor name=\"" << a.name << "\" type=\"" << a.type << "\">\n";
        for (const auto& p : a.ports)
            os << "        <port name=\"" << p.name << "\" type=\""
               << (p.is_output ? "out" : "in") << "\" rate=\"" << p.rate << "\"/>\n";
        os << "      </actor>\n";
    }
    for (const auto& c : g.channels)
    {
        os << "      <channel name=\"" << c.name
           << "\" srcActor=\"" << c.src_actor << "\" srcPort=\"" << c.src_port
           << "\" dstActor=\"" << c.dst_actor << "\" dstPort=\"" << c.dst_port << "\"";
        if (c.initial_tokens > 0)
            os << " initialTokens=\"" << c.initial_tokens << "\"";
        os << "/>\n";
    }

    os << "    </sdf>\n"
       << "  </applicationGraph>\n"
       << "</sdf3>\n";
}

#endif // FORSYDE_REFLECTION

} // namespace sdf3

} // namespace ForSyDe

#endif
