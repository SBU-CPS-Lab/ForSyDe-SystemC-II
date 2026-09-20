/**********************************************************************
    * ir.hpp -- the ForSyDe intermediate representation                *
    *                                                                  *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)              *
    *                                                                  *
    * Purpose: An explicit, in-memory model of an elaborated process    *
    *          network, which the export backends are views over        *
    *                                                                  *
    * Usage:   This file is included automatically                     *
    *                                                                  *
    * License: BSD3                                                    *
    *******************************************************************/

#ifndef FORSYDE_IR_HPP
#define FORSYDE_IR_HPP

/*! \file ir.hpp
 * \brief The model graph, as data rather than as a report
 *
 * Until now the structure of an elaborated model existed only as a
 * traversal: XMLExport walked SystemC's object tree, and wrote what it
 * found straight into a rapidxml document. The structure was never a
 * value anything could hold, so there was exactly one thing that could
 * be done with it, and it had to be done during that one walk.
 *
 * This header makes the graph a value. The walk below produces an
 * ir::model; XMLExport (and any later backend -- dot, SDF3, a query
 * interface, a self-model) reads one. That is the "IR stops being a
 * report and becomes the model" step of the plan, in its first and
 * smallest form: this file changes where the structure lives, and
 * nothing about how a model elaborates.
 */

#include "config.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <systemc>

#include "abssemantics.hpp"

namespace ForSyDe
{

//! Extracts the MoC and the process constructor name from a ForSyDe kind
/*! "SY::comb2" becomes {"SY", "comb2"}. It splits on the first and the
 * last colon rather than on "::", which is what lets a constructor name
 * that itself contains punctuation -- MI::strip<SY,SDF> does -- come
 * through whole.
 *
 * `inline`: this is a non-template free function defined in a header, so
 * without it each including translation unit emits a strong definition
 * and linking two of them fails (D1; see the note in types.hpp). It
 * lived in xml.hpp until the XML backend stopped being the only thing
 * that needed to read a kind.
 */
inline void get_moc_and_pc(const std::string& kind, std::string& moc, std::string& pc)
{
    moc = kind.substr(0, kind.find(':'));
    pc = kind.substr(kind.rfind(':') + 1, kind.length());
}

namespace ir
{

//! Which end of a process a port is on
enum class direction {in, out};

//! Whether a node is a process or an instance of a composite
enum class node_kind {leaf, composite};

//! Which of a network's three child lists an ordered slot refers to
enum class child_kind {node, port, channel};

//! One constructor argument, as the process recorded it
/*! Name and rendered value, which is all process::arg_vec ever held --
 * the value is already a string by the time a constructor pushes it.
 */
struct param
{
    std::string name;
    std::string value;
};

//! A port, on a node or on the boundary of a network
/*! bound_process/bound_port are filled only for a network's own
 * boundary ports, where they name what the port is bound to one level
 * up; they stay empty on a node's ports, which is how the XML view has
 * always distinguished the two cases.
 */
struct port
{
    std::string name;
    std::string moc;        //!< as the port reports it: "SY", "SDF", ...
    std::string type;       //!< the modeller's token type, not the MoC's wrapper
    direction dir;
    std::string bound_process;
    std::string bound_port;
    //! Static production/consumption rate, if this port's process has
    //! one (ForSyDe::process::rates()) -- unset for the great majority
    //! of ports, which carry no such notion at all. A network's own
    //! boundary ports never carry one: a rate belongs to the leaf on
    //! one side of a channel, not to the channel or the boundary it
    //! crosses.
    std::optional<std::size_t> rate;
};

//! A signal, with both ends resolved to (process, port) pairs
struct channel
{
    std::string name;
    std::string moc;
    std::string type;
    std::string source;
    std::string source_port;
    std::string target;
    std::string target_port;
};

//! A leaf process, or an instance of a composite
/*! pc_name/pc_moc are the two halves of a leaf's forsyde_kind() --
 * "SY::comb2" becomes {"comb2", "SY"}. component is the composite
 * counterpart: the instance name with its trailing digits removed, so
 * that compAvg1 and compAvg2 are two instances of compAvg.
 *
 * sub indexes model::networks rather than owning a network, which is
 * what keeps ir::model a flat, copyable value with no pointers in it.
 * That matters beyond tidiness: a model that is a plain value can be
 * copied into a child process or written to a pipe, which is what the
 * digital-twin work will need of it, and it mirrors the shape of what
 * the XML backend already emits -- one document per level of hierarchy,
 * not one nested document.
 */
struct node
{
    node_kind kind = node_kind::leaf;
    std::string name;
    std::string component;
    std::string pc_name;
    std::string pc_moc;
    std::vector<param> params;
    std::vector<port> ports;
    std::size_t sub = npos;
    //! ForSyDe::process::initial_tokens(), for a leaf; always 0 for a
    //! composite instance, which has no such notion of its own -- see
    //! that hook's comment. Kept on the node rather than only on the
    //! one direct channel it happens to feed, because a flattening
    //! consumer (sdf3.hpp) needs it to survive however many composite
    //! boundaries the leaf's output port is nested behind.
    std::size_t initial_tokens = 0;

    static constexpr std::size_t npos = static_cast<std::size_t>(-1);
};

//! One slot in a network's child order
/*! The three lists below are what a consumer wants to iterate -- every
 * node, every channel -- but a backend that has to reproduce a file
 * byte for byte needs the order the children were elaborated in, which
 * interleaves the three. Keeping the order as a separate list of
 * (which list, which index) pairs serves both without duplicating
 * anything.
 */
struct child_ref
{
    child_kind kind;
    std::size_t index;
};

//! One level of hierarchy: a composite's contents, or the model's top
struct network
{
    std::string name;       //!< instance name with trailing digits removed
    std::string instance;   //!< instance name as elaborated
    std::vector<node> nodes;
    std::vector<port> ports;
    std::vector<channel> channels;
    std::vector<child_ref> order;
};

//! A whole elaborated model: every level of hierarchy, top first
struct model
{
    std::vector<network> networks;

    const network& top() const {return networks.front();}
};

//! Everything below reads the structural record FORSYDE_REFLECTION keeps
/*! The data model above is plain data and is always available -- an
 * ir::model can be held, copied or handed around by anything. Building
 * one *from a running elaboration*, though, reads process::arg_vec and
 * the bound-channel vectors, which exist only when the library is
 * compiled with its reflection on (config.hpp).
 */
#ifdef FORSYDE_REFLECTION

namespace detail
{

//! The instance-name-to-component-name convention, in one place
/*! "compAvg1" -> "compAvg". The rule is the library's own and predates
 * this file: a composite instance is named by its component followed by
 * a serial number. It was written out three times in the XML backend,
 * which is why it is here rather than there.
 */
inline std::string component_of(const std::string& instance_name)
{
    return instance_name.substr(0, instance_name.find_last_not_of("0123456789") + 1);
}

inline bool is_module(const sc_core::sc_object* obj)
{
    return obj->kind() == std::string("sc_module");
}

inline bool is_leaf(sc_core::sc_object* obj)
{
    return dynamic_cast<ForSyDe::process*>(obj) != nullptr;
}

inline bool is_port(sc_core::sc_object* obj)
{
    return dynamic_cast<ForSyDe::introspective_port*>(obj) != nullptr;
}

inline bool is_signal(const sc_core::sc_object* obj)
{
    return obj->kind() == std::string("sc_fifo");
}

//! A port's direction, from the SystemC kind string
/*! The port classes do not carry their own direction -- in_port and
 * out_port are separate templates with no common base that records
 * which is which -- so this reads sc_fifo_in/sc_fifo_out, as the XML
 * backend did before it.
 */
inline direction direction_of(const sc_core::sc_object* obj)
{
    return obj->kind() == std::string("sc_fifo_in") ? direction::in : direction::out;
}

inline port port_from(ForSyDe::introspective_port* p, direction dir)
{
    port out;
    out.name = dynamic_cast<sc_core::sc_object*>(p)->basename();
    out.moc = p->moc();
    out.type = p->token_type();
    out.dir = dir;
    return out;
}

} // namespace detail

//! Build the IR of an elaborated module and everything below it
/*! Call no earlier than end_of_elaboration: it reads the bound-channel
 * vectors that process::bindInfo() fills at that point, and the
 * port-to-port and port-to-channel back-pointers that the binding
 * operators record as a model is built.
 *
 * Each composite becomes its own network, appended to model::networks,
 * with the node standing for it holding that network's index. The top
 * module is networks[0].
 */
inline std::size_t build_into(model& m, sc_core::sc_module* mod)
{
    const std::size_t self = m.networks.size();
    m.networks.emplace_back();
    // Every access below goes through m.networks[self] rather than a
    // reference taken once here, and the recursive call's result is
    // stored as an index rather than a pointer. Both are for the same
    // reason: building a child network emplaces onto this very vector,
    // so anything referring into it across that call can dangle. Not a
    // hypothetical -- mi/genregproc has seven networks, so the vector
    // reallocates twice while descending into them.
    m.networks[self].instance = mod->basename();
    m.networks[self].name = detail::component_of(m.networks[self].instance);

    for (sc_core::sc_object* child : mod->get_child_objects())
    {
        if (detail::is_module(child))
        {
            node n;
            n.name = child->basename();

            if (detail::is_leaf(child))
            {
                auto* p = static_cast<ForSyDe::process*>(child);
                n.kind = node_kind::leaf;

                get_moc_and_pc(p->forsyde_kind(), n.pc_moc, n.pc_name);
                n.initial_tokens = p->initial_tokens();

                for (const auto& arg : p->arg_vec)
                    n.params.push_back({std::get<0>(arg), std::get<1>(arg)});

                for (const auto& bound : p->boundInChans)
                    n.ports.push_back(detail::port_from(
                        dynamic_cast<ForSyDe::introspective_port*>(bound.port), direction::in));
                for (const auto& bound : p->boundOutChans)
                    n.ports.push_back(detail::port_from(
                        dynamic_cast<ForSyDe::introspective_port*>(bound.port), direction::out));

                // rates() is a does-nothing default on most processes
                // (empty both ways), so this only ever fills in ports
                // for the SDF comb/zip/unzip families that override it
                // -- everything else's n.ports keeps rate unset. Input
                // ports were pushed first above, so in_rates lines up
                // against the front of n.ports and out_rates against
                // the back, the same split boundInChans/boundOutChans
                // already made.
                const auto pr = p->rates();
                std::size_t idx = 0;
                for (std::size_t i = 0; i < pr.in_rates.size() && idx < n.ports.size(); i++, idx++)
                    n.ports[idx].rate = pr.in_rates[i];
                for (std::size_t i = 0; i < pr.out_rates.size() && idx < n.ports.size(); i++, idx++)
                    n.ports[idx].rate = pr.out_rates[i];
            }
            else
            {
                n.kind = node_kind::composite;
                n.component = detail::component_of(n.name);

                for (sc_core::sc_object* sub_child : static_cast<sc_core::sc_module*>(child)->get_child_objects())
                    if (detail::is_port(sub_child))
                        n.ports.push_back(detail::port_from(
                            dynamic_cast<ForSyDe::introspective_port*>(sub_child),
                            detail::direction_of(sub_child)));

                // Recurse first, then store the index: build_into may
                // reallocate m.networks, but the index it returns stays
                // valid where a reference would not.
                n.sub = build_into(m, static_cast<sc_core::sc_module*>(child));
            }

            m.networks[self].order.push_back({child_kind::node, m.networks[self].nodes.size()});
            m.networks[self].nodes.push_back(std::move(n));
        }
        else if (detail::is_port(child))
        {
            auto* ip = dynamic_cast<ForSyDe::introspective_port*>(child);
            port pt = detail::port_from(ip, detail::direction_of(child));
            // A network's own port is bound one level up, and that is
            // the binding this records -- the port it is bound to, and
            // the process owning that port.
            pt.bound_process = ip->bound_port->get_parent_object()->basename();
            pt.bound_port = ip->bound_port->basename();

            m.networks[self].order.push_back({child_kind::port, m.networks[self].ports.size()});
            m.networks[self].ports.push_back(std::move(pt));
        }
        else if (detail::is_signal(child))
        {
            auto* ic = dynamic_cast<ForSyDe::introspective_channel*>(child);
            channel ch;
            ch.name = child->basename();
            ch.moc = ic->moc();
            ch.type = ic->token_type();
            ch.source = ic->oport->get_parent_object()->basename();
            ch.source_port = ic->oport->basename();
            ch.target = ic->iport->get_parent_object()->basename();
            ch.target_port = ic->iport->basename();

            m.networks[self].order.push_back({child_kind::channel, m.networks[self].channels.size()});
            m.networks[self].channels.push_back(std::move(ch));
        }
    }

    return self;
}

//! Build the IR of an elaborated model
inline model build(sc_core::sc_module* top)
{
    model m;
    build_into(m, top);
    return m;
}

#endif // FORSYDE_REFLECTION

} // namespace ir

} // namespace ForSyDe

#endif
