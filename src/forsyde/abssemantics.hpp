/**********************************************************************           
    * abssemantics.hpp -- The common abstract semantics for all MoCs  *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *                                                                 *
    * Purpose: The common base for mapping supported MoCs on top of   *
    *          the SystemC simulation kernel.                         *
    *                                                                 *
    * Usage:   This file is included automatically                    *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/

#ifndef ABSSEMANTICS_HPP
#define ABSSEMANTICS_HPP

/*! \file abssemantics.hpp
 * \brief The common abstract semantics for all MoCs.
 * 
 *  The common abstract semantics which is used by other MoCs is
 * provided in this file.
 * It is used by other MoCs to implement their semantics on top of the
 * SystemC DE kernel.
 */

//! The namespace for ForSyDe
/*! General namespace that includes everything provided by the SFF.
 * Each MoC has its own sub-namespace.
 */

#include "config.hpp"

#include <systemc>
#include <sstream>
#include <fstream>
#include <memory>
#include <vector>

// get_type_name<T>() (used a few lines below, under the same macro) is
// declared in types.hpp. forsyde.hpp includes types.hpp itself before
// abssemantics.hpp, so this was masked there, but that made
// abssemantics.hpp -- like the other 41 headers fixed alongside it --
// not self-contained: including it on its own with FORSYDE_REFLECTION
// defined failed to compile, needing a translation unit to happen to
// have pulled in types.hpp first for unrelated reasons.
#ifdef FORSYDE_REFLECTION
#include "types.hpp"
#endif

#include "binding.hpp"


namespace ForSyDe
{

using namespace sc_core;

// This is the sole definition of write_multiport in the library.
// adaptivity.hpp used to shadow it with a #define of the same name, and
// because the preprocessor has no namespace scoping, that macro stayed
// active for the rest of the translation unit after adaptivity.hpp was
// included -- silently swapping every later write_multiport(...) call
// (this template's, everywhere else in the library) from a real
// function call to raw textual substitution for as long as the macro
// remained defined. That is fragile in a header-only library where a
// user's own single-TU build can combine these headers in whatever
// order they like, and it broke down further inside a fold expression
// (`(write_multiport(port, val), ...)`, used in the SY process
// constructors): the macro expands to a for-loop *statement*, which is
// not valid where a fold expression requires an *expression*.
//
// Three optional-backend files (sy_wrappers.hpp/GDB, ct_wrappers.hpp/
// FMI, parallel_sim.hpp/MPI) had, in turn, come to depend on that macro
// being active: each has one or more write_multiport(...) call with no
// trailing semicolon, relying on the macro's own trailing `;` to
// terminate the statement -- which only worked because forsyde.hpp
// happens to include all three after adaptivity.hpp. Removing the macro
// fixed those calls too, by adding the semicolon each was missing.
template<typename T, typename If>
void inline write_multiport(If& PORT, const T& VAL)  {
    for (int WMPi=0;WMPi<PORT.size();WMPi++)
        PORT[WMPi]->write(VAL);
}

template<typename T, typename If>
void inline write_vec_multiport(If& PORT, const std::vector<T>& VEC)  {
    for (int WMPi=0;WMPi<PORT.size();WMPi++)
        for (auto WMPit=VEC.begin();WMPit!=VEC.end();WMPit++)
            PORT[WMPi]->write(*WMPit);
}

//! Advance a process's local time to \a t, refusing to move it backwards
/*! The timed MoCs synchronise each process's local clock with the kernel
 * by waiting out the difference between where they are and the time tag
 * they are acting on. Written directly that is
 *
 *      wait(t - sc_time_stamp());
 *
 * which is a trap, because sc_time is unsigned and SystemC does not
 * check: measured against SystemC 3.0.2, sc_time(3,SC_NS) -
 * sc_time(5,SC_NS) is 18446744073709549616 ps, roughly 213 days of
 * simulated time. A model that ever presents an event tagged in the past
 * -- an out-of-order emitter, a MoC interface handing back a stale tag --
 * therefore does not fail, it appears to hang, with nothing said. That
 * is the worst available outcome for a mistake that is easy to make and
 * hard to see.
 *
 * Going backwards is a modelling error in every timed MoC here, so it is
 * reported as one.
 */
inline void wait_until(const sc_time& t,        ///< the local time to advance to
                       const char* process_name ///< reporting process, for the message
                      )
{
    if (t < sc_time_stamp())
    {
        std::ostringstream msg;
        msg << "tried to advance its local time backwards, to " << t
            << ", but it is already at " << sc_time_stamp()
            << ". An event has arrived carrying a time tag in this "
               "process's past, which means something upstream emitted "
               "out of tag order.";
        SC_REPORT_ERROR(process_name, msg.str().c_str());
        return;
    }
    sc_core::wait(t - sc_time_stamp());
}

//! Type of the object bound to a port
enum bound_type {PORT, CHANNEL};

//! A helper class used to provide introspective channels
class composite;

//! What a composite recorded, in the order it was described
/*! A composite accumulates one of these per thing built inside it, at
 * the moment it is built, which is what lets the IR be a product of
 * describing a model rather than something recovered afterwards by
 * walking SystemC's object tree (3e).
 *
 * The order is construction order, and that is not an approximation of
 * what the tree walk used to report -- it is the same thing. SystemC
 * registers a child with its parent when the child is constructed, so
 * get_child_objects() was always returning construction order; members
 * first, because they are constructed before the constructor body runs,
 * and anything built in the body interleaved exactly where it was
 * built. Recording at construction reproduces that by definition.
 */
enum class content_kind {node, port, channel};

struct content_entry
{
    content_kind what;
    sc_core::sc_object* obj;
};

namespace detail
{
//! Record this object with the composite it is being built inside
/*! A no-op when there is no enclosing composite -- a process's own
 * ports have the process as their parent, not a composite, and are
 * reached through the process's bound-channel vectors instead; and a
 * model may nest a composite inside a plain SC_MODULE, which records
 * nothing and falls back to the tree walk.
 *
 * Defined below, after composite is a complete type. Both callers are
 * templates, so they are not instantiated until long after that.
 */
inline void record_with_parent(sc_core::sc_object* self, content_kind what);
} // namespace detail

class introspective_channel
{
public:
    //! Name of the tokens in the channels
    virtual const char* token_type() const = 0;
    
    // TODO: remove if proved not to be needed
    //~ //! Size of the tokens in the channels
    //~ virtual unsigned token_size() const = 0;
    
    //! To which MoC does the signal belong
    virtual std::string moc() const = 0;
    
    //! Input port to which a channel is bound
    sc_object* iport;
    
    //! Output port to which a channel is bound
    sc_object* oport;
};

//! A ForSyDe signal is used to inter-connect processes
template <typename T, typename TokenType>
class signal: public sc_fifo<TokenType>
#ifdef FORSYDE_REFLECTION
            , public ForSyDe::introspective_channel
#endif
{
public:
#ifdef FORSYDE_REFLECTION
    signal() : sc_fifo<TokenType>() {detail::record_with_parent(this, content_kind::channel);}
    signal(sc_module_name name, unsigned size) : sc_fifo<TokenType>(name, size)
        {detail::record_with_parent(this, content_kind::channel);}
#else
    signal() : sc_fifo<TokenType>() {}
    signal(sc_module_name name, unsigned size) : sc_fifo<TokenType>(name, size) {}
#endif
#ifdef FORSYDE_REFLECTION
    typedef T type;
    
    // TODO: remove if proved not to be needed
    //~ //! Returns only the size of the token type
    //~ virtual unsigned token_size() const
    //~ {
        //~ return sizeof(T);
    //~ }
    
    // TODO: remove if proved not to be needed
    //! Returns the name of the token type
    virtual const char* token_type() const
    {
        return get_type_name<T>();
    }
    
    virtual std::string moc() const = 0;
#endif
};

//! This type is used in the process base class to store structural information
struct PortInfo
{
    sc_object* port;
    // TODO: remove if proved not to be needed
    //~ unsigned toks;
    std::string portType;
};

//! A helper class used to provide introspective ports
class introspective_port
{
public:
    //! To which port it is bound (used for binding ports of composite processes in the hierarchy)
    sc_object* bound_port;
    
    //! Name of the tokens of the port
    virtual const char* token_type() const = 0;

    //! To which MoC does the signal belong
    virtual std::string moc() const = 0;
};

//! The in_port port is used for input ports of ForSyDe processes
template <typename T, typename TokenType, typename ChanType>
class in_port: public sc_fifo_in<TokenType>
#ifdef FORSYDE_REFLECTION
            , public ForSyDe::introspective_port
#endif
{
public:
#ifdef FORSYDE_REFLECTION
    in_port() : sc_fifo_in<TokenType>(){detail::record_with_parent(this, content_kind::port);}
    in_port(const char* name) : sc_fifo_in<TokenType>(name)
        {detail::record_with_parent(this, content_kind::port);}
#else
    in_port() : sc_fifo_in<TokenType>(){}
    in_port(const char* name) : sc_fifo_in<TokenType>(name){}
#endif
#ifdef FORSYDE_REFLECTION
    typedef T type;
    
    // NOTE: The following member functions could be overriden easier if
    //       bind() was declared virtual in the sc_port base classes.
    //       This will happen in SystemC 2.3, so adapt these accordingly.
    //! Record the bounded channels
    void operator()(sc_fifo_in_if<TokenType>& i)
    {
        sc_fifo_in<TokenType>::operator()(i);
        static_cast<ChanType&>(i).iport = this;
    }
    
    //! Record the bounded ports
    void operator()(in_port<T,TokenType,ChanType>& p)
    {
        sc_fifo_in<TokenType>::operator()(p);
        p.bound_port = this;
    }
    
    //! Returns the plain name of the token type
    virtual const char* token_type() const
    {
        return get_type_name<T>();
    }

    virtual std::string moc() const = 0;
#endif
};

//! The UT_out port is used for output ports of UT processes
template <typename T, typename TokenType, typename ChanType>
class out_port: public sc_fifo_out<TokenType>
#ifdef FORSYDE_REFLECTION
            , public ForSyDe::introspective_port
#endif
{
public:
#ifdef FORSYDE_REFLECTION
    out_port() : sc_fifo_out<TokenType>(){detail::record_with_parent(this, content_kind::port);}
    out_port(const char* name) : sc_fifo_out<TokenType>(name)
        {detail::record_with_parent(this, content_kind::port);}
#else
    out_port() : sc_fifo_out<TokenType>(){}
    out_port(const char* name) : sc_fifo_out<TokenType>(name){}
#endif
#ifdef FORSYDE_REFLECTION
    typedef T type;
    
    // NOTE: The following member functions could be overriden easier if
    //       bind() was declared virtual in the sc_port base classes.
    //       This will happen in SystemC 2.3, so adapt these accordingly.
    //! Record the bounded channels
    void operator()(sc_fifo_out_if<TokenType>& i)
    {
        sc_fifo_out<TokenType>::operator()(i);
        // Register the port-to-port binding
        static_cast<ChanType&>(i).oport = this;
    }
    
    //! Record the bounded ports
    void operator()(out_port<T,TokenType,ChanType>& p)
    {
        sc_fifo_out<TokenType>::operator()(p);
        // Register the port-to-port binding
        p.bound_port = this;
    }
    
    //! Returns the name of the actual type (not abst_ext version)
    virtual const char* token_type() const
    {
        return get_type_name<T>();
    }

    virtual std::string moc() const = 0;
#endif
};

//! The process constructor which defines the abstract semantics of execution
/*! This class defines a set of methods and their execution order which
 * together define the abstract execution semantics of the processes in
 * ForSyDe-SystemC.
 * In each MoC, process constructors implement the these methods 
 * according to its own semantics. 
 * Additionally, this class contains members which are used to collect
 * and store information about the structure of the models which is used
 * for introspection in the elaboration phase.
 * 
 * Note that this is an abstract class and can not be directly
 * instantiated.
 * The designer uses the process constructors which implement the
 * abstract methods in a specific MoC.
 */
class process : public sc_module
{
private:
    //! 
    // SC_HAS_PROCESS(process) stood here. In SystemC 3.0 the macro
    // expands to a static_assert that does nothing -- SC_THREAD
    // reaches the module type through SC_CURRENT_USER_MODULE_TYPE,
    // which is decltype(*this) -- so it is deleted rather than kept
    // as decoration.

    //! The main and only execution thread of the module
    void worker()
    {
        //  We run the init stage here and not in the constructor to
        // force running it after the elaboration phase.
        init();
        while (1)
        {
            prep();     // The preparaion stage
            exec();     // The execution stage
            prod();     // The production stage
        }
    }

protected:
    //! The init stage
    /*! This stage is executed once in the beginning and is responsible
     * for initialization tasks such as allocating IO buffers, etc.
     */
    virtual void init() = 0;
    
    //! The prep stage
    /*! This stage is executed continuously in a loop and is responsible
     * for preparaing the inputs to the execution phase.
     */
    virtual void prep() = 0;
    
    //! The exece stage
    /*! This stage is executed continuously in a loop and executes the
     * main functionality of the process (e.g., by applying a supplied
     * function).
     */
    virtual void exec() = 0;
    
    //! The prod stage
    /*! This stage is executed continuously in a loop and is responsible
     * for writing the computed results to the output.
     */
    virtual void prod() = 0;
    
    //! The clean stage
    /*! This stage is executed once at the end and is responsible for
     * cleaning jobs such as deallocation of the allocated memories, etc.
     */
    virtual void clean() = 0;
    
    //! This hook is used to run the clean stage
    void end_of_simulation()
    {
        clean();
    }
    
#ifdef FORSYDE_REFLECTION

    //! This hook is used to collect additional structural information
    void end_of_elaboration()
    {
        bindInfo();
    }

    //! This method is called during end_of_elaboration to gather binded channels information
    /*! This function should save the pointers to all of the channels
     * objects bound to the input and output channels in boundInChans
     * and boundOutChans respectively
     */
    virtual void bindInfo() = 0;
#endif

public:

#ifdef FORSYDE_REFLECTION
    //! Pointers to the input ports and their bound channels
    std::vector<PortInfo> boundInChans;
    //! Pointers to the output ports and their bound channels
    std::vector<PortInfo> boundOutChans;
    
    //! Vector holding a list of argument/value tuples passed to the process constructor
    std::vector<std::tuple<std::string,std::string>> arg_vec;
#endif
 
    //! The constructor requires the module name
    /*! It creates an SC_THREAD which reads data from its input port,
     * processes them and writes the results using the output port.
     */
    process(sc_module_name _name    ///< The name of the ForSyDe process
            ): sc_module(_name)
    {
        SC_THREAD(worker);
#ifdef FORSYDE_REFLECTION
        // A process records itself with the composite it is built
        // inside, rather than being recorded by composite::add(). Both
        // would be the same thing for add(new X(...)), but a process
        // may equally be declared as a *member* of a composite and
        // constructed in its initializer list -- CT::filterf and
        // CT::pif in ct_lib.hpp both do -- and those never pass through
        // add() at all. Registering here is the one place every
        // process goes through however it was built.
        detail::record_with_parent(this, content_kind::node);
#endif
    }
    
    //! The ForSyDe process type represented by the current module
    virtual std::string forsyde_kind() const = 0;
    
};

//! A module that owns the processes and sub-modules built inside it
/*! The base for a model's top level and for any composite process, in
 * place of a bare SC_MODULE.
 *
 * It exists to answer a question the library had been leaving open.
 * Every one of the 137 make_* helpers ended in `return p;` after a new,
 * no example ever deleted anything, and SystemC registers a module with
 * its parent without owning it -- so every process in every model leaked.
 * In a one-shot simulation that is invisible, because the objects live
 * until exit(). It stops being invisible as soon as anything elaborates
 * a model more than once, which is exactly what Phase 4a's forked sim_A
 * does.
 *
 * add() is also what lets a process be built with class template
 * argument deduction. CTAD applies to a new-expression and to a
 * block-scope variable, and to nothing else -- a non-static data member
 * cannot deduce, in C++17 or in C++20 -- so a process declared as a
 * member has to spell its token types out. Passing the new-expression
 * through here keeps the deduction and fixes the ownership at the same
 * time:
 *
 *      add(new SY::comb2("mul1", mul_func))(result, srca, srcb);
 *
 * which is one statement, as the helper it replaces was.
 *
 * On destruction order: the owned processes are released when this base
 * subobject is destroyed, which is after the derived class's own members
 * -- its signals among them. That is safe because a port's destructor
 * does not reach back into the channel it is bound to, and it is the
 * order any arrangement would give short of declaring each process as a
 * member of the module, which is what CTAD cannot do.
 */
class composite : public sc_module
{
public:
    //! Both forms sc_module offers
    /*! The default one is what SC_CTOR(top) reaches: the macro expands to
     * top(sc_module_name) with no base initializer, and sc_module's own
     * default constructor takes the name off the stack the sc_module_name
     * temporary pushed it onto. A composite that did not have it could
     * only be written with the base spelled out by hand.
     */
#ifdef FORSYDE_REFLECTION
    // A composite records itself with its own parent for the same
    // reason a process does: it is a node in the network above it,
    // whether it was reached through add() or declared as a member.
    composite() : sc_module() {detail::record_with_parent(this, content_kind::node);}
    composite(sc_module_name _name) : sc_module(_name)
        {detail::record_with_parent(this, content_kind::node);}
#else
    composite() : sc_module() {}
    composite(sc_module_name _name) : sc_module(_name) {}
#endif

    //! Take ownership of a freshly constructed process or sub-module
    /*! Returns it by reference, so that the call reads as one statement
     * with the signal binding that follows it.
     */
    template <typename P>
    P& add(P* p)
    {
        // Ownership only. The node was already recorded by the process
        // or composite itself as it was constructed, which is a moment
        // earlier than this and happens whether or not add() is
        // involved.
        owned.emplace_back(p);
        return *p;
    }

#ifdef FORSYDE_REFLECTION
    //! What was built inside this composite, in the order it was built
    /*! This is the record 3e made the IR out of. ir::build reads it
     * instead of walking get_child_objects() and sorting the results
     * back out with dynamic_cast, which means the structure comes from
     * what the model said rather than from what SystemC happened to
     * keep. A composite knows its own contents; it no longer has to be
     * asked about them from outside.
     */
    const std::vector<content_entry>& contents() const {return contents_;}

    //! Called by a port or signal as it is constructed inside this composite
    void record_content(content_kind what, sc_core::sc_object* obj)
    {
        contents_.push_back({what, obj});
    }
#endif

    //! SystemC's positional binding, hidden on purpose
    /*! sc_module::operator() binds a module's ports in declaration order
     * through sc_port::bind, which is not the operator() that in_port
     * and out_port override to record the bound channel. A composite
     * bound that way elaborates perfectly and then emits introspection
     * XML with no channels in it at all -- a failure with no symptom
     * until someone reads the XML. It is easy to reach for by accident,
     * because it accepts exactly the call a ForSyDe binder would.
     *
     * A composite's own ports are bound by name instead, which is what
     * every composite process in the examples already does.
     */
    template <typename... Sigs>
    void operator()(Sigs&...)
    {
        static_assert(sizeof...(Sigs) != sizeof...(Sigs),
            "Bind a composite's ports by name -- m.a(sig) -- rather than "
            "positionally. SystemC's positional binding is hidden here "
            "because it does not record the binding for introspection, so "
            "a model that used it would elaborate correctly and emit an "
            "XML with no channels in it.");
    }

private:
    std::vector<std::unique_ptr<sc_module>> owned;
#ifdef FORSYDE_REFLECTION
    std::vector<content_entry> contents_;
#endif
};

#ifdef FORSYDE_REFLECTION
inline void detail::record_with_parent(sc_core::sc_object* self, content_kind what)
{
    // dynamic_cast rather than a static one because the parent may be
    // any sc_object: a plain SC_MODULE, a process (for its own ports),
    // or nothing at all for something built outside a module. Only a
    // composite records.
    if (auto* c = dynamic_cast<composite*>(self->get_parent_object()))
        c->record_content(what, self);
}
#endif

//! Declares a composite process the way SC_MODULE declares a plain one
/*! Expands to `struct name : public ForSyDe::composite` -- the
 * constructor, any parameters beyond the name, and everything else
 * about the class body are written exactly as they would be without
 * the macro; a composite's constructor already takes arbitrary extra
 * parameters today, the same as any other SystemC module's. It exists
 * for the same reason SC_MODULE does: so a composite process reads as
 * one recognizable declaration rather than an inheritance clause
 * someone has to notice.
 */
#define FORSYDE_COMPOSITE(name) struct name : public ForSyDe::composite

}

#endif
