/**********************************************************************
    * xml.hpp -- Dumps the system model as the abstract XML+C format  *
    *                                                                 *
    * Authors: Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: Dumps the structure and behavior of a system model     *
    *                                                                 *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef XML_HPP
#define XML_HPP

/*! \file xml.hpp
 * \brief Dumps the system model as the XML+C abstract format.
 *
 *  This file includes functions which can be used in order to export
 * the structure and behavior of a specified system in an abstract
 * format represented as an XML file plus a set of CPP fiels.
 * This format can be used by other tools for further manipulation.
 *
 * This backend is a *view* over ForSyDe::ir::model (ir.hpp) rather than
 * a traversal of SystemC's object tree. It used to be both at once: the
 * walk that discovered the structure and the code that wrote it out
 * were the same loop, so the structure existed only for as long as the
 * walk did and only one thing could ever be done with it. Splitting
 * them changes nothing about the output -- the golden corpus under
 * tests/golden_ir pins that, file for file -- and it is what lets a
 * second backend, or a self-model, read the same graph.
 */

#include <systemc>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include "rapidxml_print.hpp"

#include "abssemantics.hpp"
#include "ir.hpp"

// D6: this file used to also carry an unconditional
// "using namespace rapidxml;" here, alongside every rapidxml type it
// names (xml_node, xml_document, xml_attribute, node_element) below.
// Qualified those instead and dropped the using-directive: rapidxml is
// this file's own vendored XML backend, not part of ForSyDe's public
// surface, and this using-directive sat at *file* scope -- before
// `namespace ForSyDe {` even opens below -- so it polluted the global
// namespace of every translation unit that so much as included
// forsyde.hpp with FORSYDE_INTROSPECTION defined, whether or not that
// TU ever wrote `using namespace ForSyDe;`.
//
// "using namespace boost;" used to sit here unconditionally, with
// nothing in this file ever using anything from it -- it compiled only
// because something else in a whole-library build happened to include a
// boost header first (dde_process_constructors.hpp's
// boost/numeric/ublas/matrix.hpp), which is exactly the kind of
// implicit, include-order-dependent link D5 is about. Confirmed unused
// (grep for boost:: and every Boost facility this library touches
// elsewhere turns up nothing in this file) and removed outright rather
// than given its own include, rather than keep a namespace-polluting
// using-directive (D6) alive to serve a dependency that doesn't exist.
//
// get_moc_and_pc lived here too, and moved to ir.hpp when splitting a
// forsyde_kind() stopped being something only this backend did.

namespace ForSyDe
{
using namespace sc_core;

//! Abstract class used to Export a system as an XML file
/*! This class provides basic facilities to export a ForSyDe-SystemC
 * process network as an XML file.
 */
class XMLExport
{
public:
    //! The constructor takes the gneration path
    XMLExport(std::string path) : path(path)
    {
        // The output path is a per-example convention (each example's
        // top.hpp names its own "gen/"), not something the build system
        // creates, and a fresh checkout has no such directory since it
        // holds only generated output.
        if (!path.empty())
            std::filesystem::create_directories(path);

        // Allocate global names
        const_name = (char*)"name";
        const_leaf_process = (char*)"leaf_process";
        const_composite_process = (char*)"composite_process";
        const_component_name = (char*)"component_name";
        const_process_network = (char*)"process_network";
        const_process_constructor = (char*)"process_constructor";
        const_argument = (char*)"argument";
        const_value = (char*)"value";
        const_moc = (char*)"moc";
        const_type = (char*)"type";
        const_sdf = (char*)"sdf";
        const_sadf = (char*)"sadf";
        const_ut = (char*)"ut";
        const_sy = (char*)"sy";
        const_dde = (char*)"dde";
        const_dt = (char*)"dt";
        const_ct = (char*)"ct";
        const_mi = (char*)"mi";
        const_port = (char*)"port";
        const_port_dir = (char*)"port_dir";
        const_direction = (char*)"direction";
        const_in = (char*)"in";
        const_out = (char*)"out";
        const_signal = (char*)"signal";
        const_source = (char*)"source";
        const_source_port = (char*)"source_port";
        const_target = (char*)"target";
        const_target_port = (char*)"target_port";
        const_bound_process = (char*)"bound_process";
        const_bound_port = (char*)"bound_port";
    }

    //! The destructor makes sure all XML nodes are deallocated.
    ~XMLExport()
    {
        xml_doc.clear();
    }

    //! The traverse function requires the top ForSyDe process name
    /*! It builds the IR of the elaborated model and writes one XML
     * document per level of hierarchy, which is the form this format
     * has always taken: a composite process appears in its parent's
     * document as an instance, and its contents are a document of their
     * own, named after the component rather than the instance.
     */
    void traverse(sc_module* top)
    {
        write(ForSyDe::ir::build(top));
    }

    //! Write an already-built model out, one document per network
    void write(const ForSyDe::ir::model& m)
    {
        for (const auto& net : m.networks)
        {
            // One document per network, and this object holds exactly
            // one, so each level gets its own exporter. The documents
            // are independent -- a composite's contents do not nest
            // inside its parent's document -- so there is nothing to
            // carry between them.
            XMLExport dumper(path);
            dumper.write_network(net);
        }
    }

private:
    //! Write one network as a complete XML document
    void write_network(const ForSyDe::ir::network& net)
    {
        rapidxml::xml_node<>* pn_node = allocate_append_node(&xml_doc, const_process_network);
        allocate_append_attribute(pn_node, const_name, net.name.c_str());

        for (const auto& slot : net.order)
        {
            switch (slot.kind)
            {
                case ForSyDe::ir::child_kind::node:
                {
                    const auto& n = net.nodes[slot.index];
                    if (n.kind == ForSyDe::ir::node_kind::leaf)
                        add_leaf_process(n, pn_node);
                    else
                        add_composite_process(n, pn_node);
                    break;
                }
                case ForSyDe::ir::child_kind::port:
                    add_port(net.ports[slot.index], pn_node);
                    break;
                case ForSyDe::ir::child_kind::channel:
                    add_signal(net.channels[slot.index], pn_node);
                    break;
            }
        }

        printXML(path + net.name + std::string(".xml"));
    }

    //! The print method writes the XML file to the output.
    /*! The XML structure is already generated, so this command only
     * checks for the availability of the output file and dumps the XML
     * to it.
     */
    void printXML(std::string fileName)
    {
        std::ofstream outFile(fileName);
        if (!outFile.is_open())
            SC_REPORT_ERROR(fileName.c_str(), "file could not be opened to write the introspection output. Does the path exists?");
        outFile << "<?xml version=\"1.0\" ?>" << std::endl;
        outFile << "<!-- Automatically generated by ForSyDe -->" << std::endl;
        outFile << "<!DOCTYPE process_network SYSTEM \"forsyde.dtd\" >"  << std::endl;
        outFile << xml_doc;
    }

    //! Add a leaf process
    void add_leaf_process(const ForSyDe::ir::node& n, rapidxml::xml_node<>* pn_node)
    {
        rapidxml::xml_node<> *p_node = allocate_append_node(pn_node, const_leaf_process);
        allocate_append_attribute(p_node, const_name, n.name.c_str());

            // Add the leaf process ports
            for (const auto& pt : n.ports)
                add_port(pt, p_node);

            // Add the process constructor node
            rapidxml::xml_node<> *pc_node = allocate_append_node(p_node, const_process_constructor);
            allocate_append_attribute(pc_node, const_name, n.pc_name.c_str());
            allocate_append_attribute(pc_node, const_moc, moc_attribute(n.pc_moc));

            // Add arguments
            for (const auto& arg : n.params)
            {
                rapidxml::xml_node<> *arg_node = allocate_append_node(pc_node, const_argument);
                allocate_append_attribute(arg_node, const_name, arg.name.c_str());
                allocate_append_attribute(arg_node, const_value, arg.value.c_str());
            }
    }

    //! Add a composite process
    void add_composite_process(const ForSyDe::ir::node& n, rapidxml::xml_node<>* pn_node)
    {
        rapidxml::xml_node<> *p_node = allocate_append_node(pn_node, const_composite_process);
        allocate_append_attribute(p_node, const_name, n.name.c_str());
        allocate_append_attribute(p_node, const_component_name, n.component.c_str());
        for (const auto& pt : n.ports)
            add_port(pt, p_node);
    }

    //! Add a port
    void add_port(const ForSyDe::ir::port& pt, rapidxml::xml_node<>* pn_node)
    {
        rapidxml::xml_node<> *p_node = allocate_append_node(pn_node, const_port);
        allocate_append_attribute(p_node, const_name, pt.name.c_str());
        allocate_append_attribute(p_node, const_moc, moc_attribute(pt.moc));
        allocate_append_attribute(p_node, const_type, pt.type.c_str());
        allocate_append_attribute(p_node, const_direction,
            pt.dir == ForSyDe::ir::direction::in ? const_in : const_out);
        if (!pt.bound_process.empty() && !pt.bound_port.empty())
        {
            allocate_append_attribute(p_node, const_bound_process, pt.bound_process.c_str());
            allocate_append_attribute(p_node, const_bound_port, pt.bound_port.c_str());
        }
    }

    //! Add a ForSyDe signal
    void add_signal(const ForSyDe::ir::channel& ch, rapidxml::xml_node<>* sig_parent)
    {
        rapidxml::xml_node<> *sig_node = allocate_append_node(sig_parent, const_signal);
        allocate_append_attribute(sig_node, const_name, ch.name.c_str());
        allocate_append_attribute(sig_node, const_moc, moc_attribute(ch.moc));
        allocate_append_attribute(sig_node, const_type, ch.type.c_str());
        allocate_append_attribute(sig_node, const_source, ch.source.c_str());
        allocate_append_attribute(sig_node, const_source_port, ch.source_port.c_str());
        allocate_append_attribute(sig_node, const_target, ch.target.c_str());
        allocate_append_attribute(sig_node, const_target_port, ch.target_port.c_str());
    }

    //! The lower-case spelling this format uses for a MoC
    /*! The IR keeps a MoC as the process, port or channel reports it --
     * "SY" -- and every backend spells it its own way; this one spells
     * it in lower case. "MI" reaches here only from a process: an MI
     * process's ports and signals are the MoC-specific ones of whichever
     * two MoCs it sits between, never MI's own.
     */
    char* moc_attribute(const std::string& moc)
    {
        if (moc=="SDF") return const_sdf;
        else if (moc=="SADF") return const_sadf;
        else if (moc=="UT") return const_ut;
        else if (moc=="SY") return const_sy;
        else if (moc=="DDE") return const_dde;
        else if (moc=="DT") return const_dt;
        else if (moc=="CT") return const_ct;
        else if (moc=="MI") return const_mi;
        SC_REPORT_ERROR("XML Backend", "MoC could not be deduced from kind.");
        return const_sy;
    }

private:
    //! The Path for generating the output
    std::string path;

    //! The RapidXML DOM
    rapidxml::xml_document<> xml_doc;

    //! Some global constant names
    char *const_name, *const_leaf_process, *const_composite_process,
         *const_process_network, *const_process_constructor, *const_moc,
         *const_type, *const_port,
         *const_sdf, *const_sadf, *const_ut, *const_sy, *const_dde, *const_dt, *const_ct, *const_mi,
         *const_port_dir, *const_direction, *const_in, *const_out,
         *const_signal, *const_component_name, *const_argument, *const_value,
         *const_source, *const_source_port, *const_target, *const_target_port,
         *const_bound_process, *const_bound_port;

    inline rapidxml::xml_node<>* allocate_append_node(rapidxml::xml_node<>* top, const char* name)
    {
        rapidxml::xml_node<>* node = xml_doc.allocate_node(rapidxml::node_element, name);
        top->append_node(node);
        return node;
    }

    //! Copy a value into the document's pool and attach it
    /*! allocate_string rather than the pointer: rapidxml stores what it
     * is given without copying, so an attribute pointing into an
     * ir::model would outlive its source the moment a backend is handed
     * a model it does not own.
     */
    inline void allocate_append_attribute(rapidxml::xml_node<>* node, const char* attr_name, const char* attr_val)
    {
        rapidxml::xml_attribute<>* attr = xml_doc.allocate_attribute(
            attr_name, xml_doc.allocate_string(attr_val));
        node->append_attribute(attr);
    }

};



}

#endif
