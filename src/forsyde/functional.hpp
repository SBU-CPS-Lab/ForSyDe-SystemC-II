/**********************************************************************
    * functional.hpp -- an applicative surface over the same IR        *
    *                                                                  *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)              *
    *                                                                  *
    * Purpose: Writing a process network as composition on signals,     *
    *          rather than as declare-then-bind                         *
    *                                                                  *
    * Usage:   This file is included automatically                     *
    *                                                                  *
    * License: BSD3                                                    *
    *******************************************************************/

#ifndef FORSYDE_FUNCTIONAL_HPP
#define FORSYDE_FUNCTIONAL_HPP

/*! \file functional.hpp
 * \brief A Haskell-shaped front end over the explicit surface
 *
 * A ForSyDe-SystemC model is written imperatively: declare the signals,
 * then construct processes that bind to them. The signal has to exist
 * before the process that produces it, so data flows left to right in
 * the declarations and right to left in the argument lists.
 *
 *     SY::signal<int> s1, s2, s3;
 *     add(new SY::sconstant("src", 1, 10))(s1);
 *     add(new SY::comb("p", f))(s2, s1);
 *     add(new SY::comb("q", g))(s3, s2);
 *     add(new SY::ssink("snk", h))(s3);
 *
 * In Haskell ForSyDe the same network is a composition of functions on
 * signals, because a process constructor is curried:
 * combSY f :: Signal a -> Signal b. The reason C++ could not have that
 * was never syntax -- it was that an sc_fifo cannot be a value you
 * return, because it is a channel that must exist at elaboration and be
 * bound by reference. A handle to one can be.
 *
 *     fn::network net(*this);
 *     auto s1 = fn::SY::constant(1, 10)(net);
 *     auto s3 = (fn::SY::comb(f) | fn::SY::comb(g))(s1);
 *               fn::SY::sink(h)(s3);
 *
 * fn::SY::comb(f) does not build a module. It builds a small value
 * holding f -- a partially applied constructor -- whose operator()
 * takes signal handles, appends a process and its output signals to the
 * network, and returns handles to those outputs. That is the whole
 * trick, and it is what makes the type read like Signal a -> Signal b.
 *
 * \section fn_thin This is a front end, not a second implementation
 *
 * Every application here ends in exactly what a hand-written model
 * does: `new P(name, args...)`, composite::add, and the positional
 * binder from binding.hpp. Nothing about elaboration, the IR or the
 * exported XML knows which surface described the model, and
 * tests/functional builds the same network both ways and diffs the IR
 * to keep that true rather than merely intended.
 *
 * Both surfaces stay. The explicit one is what maps onto hand-written
 * SystemC, and it is what a co-simulation wrapper or a hand-partitioned
 * MPI model needs; this one is the default for models that are just
 * models.
 *
 * \section fn_feedback Feedback
 *
 * An applicative surface cannot tie a knot without laziness, and rather
 * than simulate laziness the back edge is declared:
 *
 *     auto fb = net.declare<SY::signal<int>>();
 *     auto out = fn::SY::comb2(f)(in, fb);
 *     fn::SY::delay(0).into(fb)(out);
 *
 * declare() makes a real signal with no producer yet; into() says which
 * process writes it. A declared signal that never gets one is an error
 * at the end of the description -- which is a better diagnostic than
 * today's silent deadlock on a FIFO nobody writes.
 *
 * \section fn_scope What this is not
 *
 * Shallow embedding with a functional face, not FRP: no continuous-time
 * semantics in the surface, no switch/hold promoted to first class, no
 * dynamic reconfiguration hidden inside a signal function. Mode
 * switching stays explicit, where ForSyDe puts it. FRP's higher-order
 * switching is exactly the construct that costs static analysability.
 *
 * Composition is flat here: `a | b` applies a then b in the current
 * network rather than emitting a composite around them. Hierarchy is
 * asked for, with fn::box, rather than falling out of every use of `|`
 * -- see the note on box() below.
 */

#include "config.hpp"

#ifdef FORSYDE_REFLECTION

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <systemc>

#include "abssemantics.hpp"

namespace ForSyDe
{

//! The applicative surface
namespace fn
{

class network;

//! A handle to a signal in the network being described
/*! A value, which is the whole point: it can be returned from a process
 * application, passed to the next one and copied around, where the
 * sc_fifo it refers to can do none of those things.
 */
template <typename Chan>
struct sig
{
    using chan_type = Chan;

    network* net = nullptr;
    Chan* ch = nullptr;

    bool valid() const {return net != nullptr && ch != nullptr;}
};

namespace detail
{

//! The argument types of a plain function, a function pointer or a lambda
/*! Only used to let fn::SY::comb(f) deduce its token types from f the
 * way the explicit surface's CTAD does from the same argument. A
 * std::function parameter deduces nothing on its own, which is why the
 * explicit surface sometimes needs its types spelled out; going through
 * the callable's own signature is what avoids that here.
 */
template <typename F> struct callable_traits
    : callable_traits<decltype(&std::remove_reference_t<F>::operator())> {};

template <typename R, typename... A> struct callable_traits<R(*)(A...)>
{
    using args = std::tuple<A...>;
};
template <typename R, typename... A> struct callable_traits<R(A...)>
{
    using args = std::tuple<A...>;
};
template <typename C, typename R, typename... A>
struct callable_traits<R(C::*)(A...) const>
{
    using args = std::tuple<A...>;
};
template <typename C, typename R, typename... A>
struct callable_traits<R(C::*)(A...)>
{
    using args = std::tuple<A...>;
};

//! The Nth argument of a callable, stripped of reference and const
template <typename F, std::size_t N>
using arg_t = std::decay_t<
    std::tuple_element_t<N, typename callable_traits<F>::args>>;

//! Peel abst_ext<T> back to T, for the SY constructors whose functions
//! are written over the wrapped type while the process is named by the
//! bare one.
template <typename T> struct unwrap_abst {using type = T;};
template <typename T> struct unwrap_abst<ForSyDe::abst_ext<T>> {using type = T;};
template <typename T> using unwrap_abst_t = typename unwrap_abst<T>::type;

//! Peel std::vector<T> back to T, for the SDF constructors, whose
//! functions take a vector of however many tokens the rate declares.
template <typename T> struct unwrap_vec {using type = T;};
template <typename T> struct unwrap_vec<std::vector<T>> {using type = T;};
template <typename T> using unwrap_vec_t = typename unwrap_vec<T>::type;

} // namespace detail

//! The composite currently being described
/*! A handle to the composite whose constructor is running, not a thing
 * that owns a model. That matters and is not an implementation detail:
 * SystemC decides an object's parent from the module being constructed
 * at that moment, so a process or signal built anywhere else would be
 * parented wrongly -- and, since 3e, recorded in the wrong composite or
 * in none. Describing inside the constructor is what keeps the two
 * surfaces producing the same hierarchy.
 */
class network
{
public:
    explicit network(ForSyDe::composite& owner) : owner_(&owner) {}

    //! Nothing described so far may be left dangling when this goes away
    ~network() noexcept(false)
    {
        // check() is the check; this runs it for a description that
        // did not. Skipped once it has been called explicitly, so that
        // a caller which handled the failure itself does not get it
        // thrown at them again from here.
        //
        // Throwing out of a destructor is only safe when nothing else
        // is already unwinding; when something is, that failure is the
        // one worth reporting anyway.
        if (!checked_ && std::uncaught_exceptions() == 0) check();
    }

    network(const network&) = delete;
    network& operator=(const network&) = delete;

    ForSyDe::composite& owner() const {return *owner_;}

    //! A fresh signal, owned by the composite being described
    /*! Default-constructed so SystemC names it the way it names every
     * other anonymous channel -- fifo_0, fifo_1 -- which is what a
     * model written the explicit way with unnamed signal members gets
     * too.
     */
    template <typename Chan>
    sig<Chan> fresh()
    {
        auto* c = new Chan();
        owner_->own_object(c);
        return sig<Chan>{this, c};
    }

    //! A signal whose producer comes later: the back edge of a loop
    template <typename Chan>
    sig<Chan> declare()
    {
        sig<Chan> s = fresh<Chan>();
        pending_.push_back({static_cast<sc_core::sc_object*>(s.ch), false});
        return s;
    }

    //! Record that something now writes \a s, satisfying a declare()
    template <typename Chan>
    void satisfy(const sig<Chan>& s)
    {
        auto* obj = static_cast<sc_core::sc_object*>(s.ch);
        for (auto& p : pending_)
            if (p.obj == obj)
            {
                if (p.written)
                    fail(std::string("signal '") + obj->basename()
                         + "' is written by more than one process");
                p.written = true;
                return;
            }
        fail(std::string("into(...) names signal '") + obj->basename()
             + "', which was not produced by declare()");
    }

    //! Every declared signal must have found a producer
    /*! Called by the destructor, so a description that forgets one says
     * so at the end of elaboration rather than deadlocking silently on
     * a FIFO nobody writes -- which is what the explicit surface does
     * with the same mistake.
     */
    void check()
    {
        checked_ = true;
        for (const auto& p : pending_)
            if (!p.written)
                fail(std::string("signal '") + p.obj->basename()
                     + "' was declared for feedback but nothing ever writes it");
    }

    //! The next automatic instance name for a process constructor
    /*! comb, comb, comb becomes comb1, comb2, comb3 -- the same
     * component-plus-serial convention the rest of the library names
     * composite instances by, and what the explicit surface's models
     * are written with by hand.
     */
    std::string next_name(const std::string& base)
    {
        std::ostringstream os;
        os << base << ++counts_[base];
        return os.str();
    }

private:
    struct pending {sc_core::sc_object* obj; bool written;};

    static void fail(const std::string& what)
    {
        throw std::runtime_error("ForSyDe::fn: " + what);
    }

    ForSyDe::composite* owner_;
    std::vector<pending> pending_;
    std::map<std::string, unsigned> counts_;
    bool checked_ = false;
};

namespace detail
{

//! The first network mentioned by any of these arguments
inline network* net_of() {return nullptr;}

template <typename Chan, typename... Rest>
network* net_of(const sig<Chan>& s, const Rest&... rest)
{
    return s.net ? s.net : net_of(rest...);
}

template <typename T, typename... Rest>
network* net_of(const T&, const Rest&... rest)
{
    return net_of(rest...);
}

//! One output signal: the into() target if this is slot 0 and one was named
/*! The target is used in place of allocating, not after allocating. An
 * earlier version allocated a fresh signal and then dropped it in
 * favour of the target, which left a signal owned by the composite,
 * recorded in its contents and bound at neither end -- and ir::build
 * dereferences both ends of every channel it is given.
 */
template <typename Chan>
sig<Chan> one_output(network& net, sc_core::sc_object* target)
{
    if (target == nullptr) return net.template fresh<Chan>();

    auto* want = dynamic_cast<Chan*>(target);
    if (want == nullptr)
        throw std::runtime_error(
            "ForSyDe::fn: into(...) was given a signal of a different type "
            "from the one this process produces");
    return sig<Chan>{&net, want};
}

//! One output handle per port in a process's out_ports() tuple
template <typename Ports, std::size_t... I>
auto fresh_for(network& net, sc_core::sc_object* target, std::index_sequence<I...>)
{
    return std::make_tuple(
        one_output<typename std::decay_t<std::tuple_element_t<I, Ports>>::chan_type>(
            net, I == 0 ? target : nullptr)...);
}

template <typename P>
auto make_outputs(network& net, P* p, sc_core::sc_object* target)
{
    if constexpr (ForSyDe::detail::has_out_ports<P>::value)
    {
        using Ports = std::decay_t<decltype(p->out_ports())>;
        return fresh_for<Ports>(net, target,
            std::make_index_sequence<std::tuple_size_v<Ports>>{});
    }
    else
    {
        if (target != nullptr)
            throw std::runtime_error(
                "ForSyDe::fn: into(...) used on a process with no outputs");
        return std::tuple<>{};
    }
}

//! Unpack a tuple of one into the handle itself; leave the rest alone
template <typename Tup>
auto simplify(Tup&& t)
{
    if constexpr (std::tuple_size_v<std::decay_t<Tup>> == 0)
        return;
    else if constexpr (std::tuple_size_v<std::decay_t<Tup>> == 1)
        return std::get<0>(std::forward<Tup>(t));
    else
        return std::forward<Tup>(t);
}

} // namespace detail

//! A partially applied process constructor
/*! Holds whatever the constructor was given and not yet the signals --
 * which is what makes it a value that can be named, stored, composed
 * with another, and only then applied.
 *
 * \a Make is a callable (const char*) -> P*, so that class template
 * argument deduction happens inside it, at the new-expression, where
 * C++17 allows it. That is the same reason composite::add takes an
 * already-constructed process: deduction works on a new-expression and
 * on nothing else.
 */
template <typename Make>
class pc
{
public:
    pc(std::string base, Make make) : base_(std::move(base)), make_(std::move(make)) {}

    //! Name this instance explicitly instead of base1, base2, ...
    pc named(std::string n) const
    {
        pc copy(*this);
        copy.explicit_name_ = std::move(n);
        return copy;
    }

    //! Write into an already-declared signal rather than a fresh one
    /*! For the back edge of a feedback loop; see the file comment.
     * Only meaningful for a constructor with exactly one output.
     */
    template <typename Chan>
    pc into(const sig<Chan>& target) const
    {
        pc copy(*this);
        copy.target_ = static_cast<sc_core::sc_object*>(target.ch);
        copy.target_net_ = target.net;
        return copy;
    }

    //! Apply to signal handles: build the process, bind it, hand back its outputs
    template <typename... Ins>
    auto operator()(Ins&&... ins) const
    {
        network* net = detail::net_of(ins...);
        if (net == nullptr) net = target_net_;
        if (net == nullptr)
            throw std::runtime_error(
                "ForSyDe::fn: a process constructor with no signal arguments "
                "must be applied to the network, as in constant(...)(net)");
        return apply(*net, std::forward<Ins>(ins)...);
    }

    //! Apply to the network itself, for a constructor with no inputs
    auto operator()(network& net) const
    {
        return apply(net);
    }

private:
    template <typename... Ins>
    auto apply(network& net, Ins&&... ins) const
    {
        const std::string name = explicit_name_.empty()
                               ? net.next_name(base_) : explicit_name_;

        auto* p = make_(name.c_str());
        net.owner().add(p);

        auto outs = detail::make_outputs(net, p, target_);
        if constexpr (std::tuple_size_v<decltype(outs)> > 0)
            if (target_ != nullptr) net.satisfy(std::get<0>(outs));
        bind(p, outs, ins...);
        return detail::simplify(std::move(outs));
    }

    template <typename P, typename Outs, typename... Ins>
    static void bind(P* p, Outs& outs, Ins&... ins)
    {
        std::apply(
            [&](auto&... o) {(*p)(*o.ch..., *ins.ch...);},
            outs);
    }

    std::string base_;
    Make make_;
    std::string explicit_name_;
    sc_core::sc_object* target_ = nullptr;
    network* target_net_ = nullptr;
};

//! Build a partially applied constructor from a factory
template <typename Make>
auto make_pc(std::string base, Make make)
{
    return pc<Make>(std::move(base), std::move(make));
}

//! Two partially applied constructors, applied in sequence
/*! Flat rather than hierarchical: `a | b` puts both in the network
 * being described. The alternative -- every `|` emitting a composite
 * around its operands -- makes composition and hierarchy the same
 * operation, which is elegant and also means a three-stage pipeline
 * written with two bars produces two nested composites nobody asked
 * for. Hierarchy is available by asking, which is what a composite
 * written the ordinary way already is.
 */
template <typename A, typename B>
class composed
{
public:
    composed(A a, B b) : a_(std::move(a)), b_(std::move(b)) {}

    template <typename... Ins>
    auto operator()(Ins&&... ins) const
    {
        return b_(a_(std::forward<Ins>(ins)...));
    }

private:
    A a_;
    B b_;
};

template <typename MakeA, typename MakeB>
auto operator|(pc<MakeA> a, pc<MakeB> b)
{
    return composed<pc<MakeA>, pc<MakeB>>(std::move(a), std::move(b));
}

template <typename A, typename B, typename MakeC>
auto operator|(composed<A,B> ab, pc<MakeC> c)
{
    return composed<composed<A,B>, pc<MakeC>>(std::move(ab), std::move(c));
}

} // namespace fn

} // namespace ForSyDe

#include "functional_sy.hpp"
#include "functional_sdf.hpp"

#endif // FORSYDE_REFLECTION

#endif
