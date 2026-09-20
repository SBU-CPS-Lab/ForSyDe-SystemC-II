/**********************************************************************
    * reflection_tcp.hpp -- reporting firings over a TCP socket        *
    *                                                                  *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)              *
    *                                                                  *
    * Purpose: A reflection observer that broadcasts to any number of   *
    *          listeners, on any platform                               *
    *                                                                  *
    * Usage:   This file is included automatically                     *
    *                                                                  *
    * License: BSD3                                                    *
    *******************************************************************/

#ifndef FORSYDE_REFLECTION_TCP_HPP
#define FORSYDE_REFLECTION_TCP_HPP

/*! \file reflection_tcp.hpp
 * \brief A TCP transport for ForSyDe::reflection
 *
 * reflection.hpp says a process reports a firing and whoever wants it
 * subscribes; it does not say how a report leaves the process. to_pipe()
 * was the first answer and is a fine one when a single local reader is
 * all that is wanted, but a named pipe is POSIX-only and has exactly one
 * reader. This is the other answer: the model listens on a TCP port and
 * every connected client gets every line.
 *
 * That buys three things a pipe does not have. It works on Windows as
 * well as POSIX, over the one socket API both provide. It is one-to-N --
 * a viewer, a logger and an analysis tool can all watch the same run,
 * and one that disconnects is dropped without disturbing the others or
 * the run. And it needs nothing installed: no broker, no library, no
 * build option. Anything that wants a broker -- MQTT, ZeroMQ, DDS --
 * can be a bridge process that connects here and republishes, which
 * keeps that dependency out of models that do not want it, the same way
 * an export backend is a separate tool rather than a header in here.
 *
 * One property to know about: a client that connects and then stops
 * reading applies backpressure, because the write blocks rather than
 * dropping the line. That is the right way round for a stream something
 * is meant to consume -- a silently truncated report is worse than a
 * slow one -- but it does mean a wedged client can stall a run, and a
 * client that only wants to sample should read and discard rather than
 * connect and idle.
 *
 * The wire format is as_report_line()'s, unchanged: one line per firing,
 * so `nc localhost 5000` is a working client and so is three lines of
 * Python.
 *
 * This header is included with the rest of the library rather than left
 * for a model to find, so that it is compiled on every build. A
 * transport nothing compiles is how the last one rotted.
 */

#include "config.hpp"
#include "reflection.hpp"

#ifdef FORSYDE_REFLECTION

#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <arpa/inet.h>
  #include <fcntl.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <sys/socket.h>
  #include <unistd.h>
#endif

namespace ForSyDe
{

namespace reflection
{

namespace detail
{

// The two socket APIs differ in a handful of spellings and in nothing
// else that matters here, so the shim is this small on purpose.
#ifdef _WIN32
using socket_t = SOCKET;
inline constexpr socket_t bad_socket = INVALID_SOCKET;
inline void close_socket(socket_t s) {::closesocket(s);}
inline bool would_block() {return WSAGetLastError() == WSAEWOULDBLOCK;}
inline void set_nonblocking(socket_t s) {u_long m = 1; ::ioctlsocket(s, FIONBIO, &m);}
//! Winsock needs starting, once per process, before any other call
inline void start_sockets()
{
    static bool started = []{
        WSADATA d;
        return WSAStartup(MAKEWORD(2,2), &d) == 0;
    }();
    if (!started) throw std::runtime_error("reflection::to_tcp: WSAStartup failed");
}
#else
using socket_t = int;
inline constexpr socket_t bad_socket = -1;
inline void close_socket(socket_t s) {::close(s);}
inline bool would_block() {return errno == EAGAIN || errno == EWOULDBLOCK;}
inline void set_nonblocking(socket_t s) {::fcntl(s, F_SETFL, ::fcntl(s, F_GETFL, 0) | O_NONBLOCK);}
inline void start_sockets() {}
#endif

// Sending to a peer that has gone away raises SIGPIPE on POSIX, whose
// default action is to kill the process. A model must not die because
// something that was watching it closed its window, so the write asks
// for an error return instead. Linux spells that per-call, macOS and the
// BSDs per-socket, and Winsock does not have the problem.
#if defined(MSG_NOSIGNAL)
  inline constexpr int send_flags = MSG_NOSIGNAL;
#else
  inline constexpr int send_flags = 0;
#endif

inline void suppress_sigpipe(socket_t s)
{
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
    int on = 1;
    ::setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#else
    (void)s;
#endif
}

//! Listens on a port and writes every line to every client attached
/*! Owned by the observer to_tcp() returns, through a shared_ptr, so the
 * socket's lifetime is the subscription's and a model does not have to
 * keep anything alive by hand.
 */
class tcp_broadcaster
{
public:
    tcp_broadcaster(unsigned short port, bool wait_for_client)
    {
        start_sockets();

        listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd == bad_socket)
            throw std::runtime_error("reflection::to_tcp: cannot create socket");

        // Without this, re-running a model inside the kernel's
        // TIME_WAIT window fails to bind -- which is exactly what
        // re-running a model does.
        int on = 1;
        ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
                     reinterpret_cast<const char*>(&on), sizeof(on));

        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // localhost only

        if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
        {
            close_socket(listen_fd);
            listen_fd = bad_socket;
            throw std::runtime_error("reflection::to_tcp: cannot bind port "
                                     + std::to_string(port));
        }
        if (::listen(listen_fd, 8) != 0)
        {
            close_socket(listen_fd);
            listen_fd = bad_socket;
            throw std::runtime_error("reflection::to_tcp: cannot listen on port "
                                     + std::to_string(port));
        }

        // Blocking here is the named pipe's one genuinely useful
        // property -- a run that starts before its reader does loses
        // the beginning of what it was meant to show. Optional, because
        // unlike the pipe it does not have to be paid.
        if (wait_for_client) accept_one_blocking();

        set_nonblocking(listen_fd);
    }

    ~tcp_broadcaster()
    {
        for (socket_t c : clients) close_socket(c);
        if (listen_fd != bad_socket) close_socket(listen_fd);
    }

    tcp_broadcaster(const tcp_broadcaster&) = delete;
    tcp_broadcaster& operator=(const tcp_broadcaster&) = delete;

    //! The port actually bound, which is the one asked for
    unsigned short port() const
    {
        sockaddr_in a;
#ifdef _WIN32
        int len = sizeof(a);
#else
        socklen_t len = sizeof(a);
#endif
        if (::getsockname(listen_fd, reinterpret_cast<sockaddr*>(&a), &len) != 0)
            return 0;
        return ntohs(a.sin_port);
    }

    //! Take on anyone who has connected since last time, without waiting
    void accept_pending()
    {
        for (;;)
        {
            socket_t c = ::accept(listen_fd, nullptr, nullptr);
            if (c == bad_socket) return;      // nobody waiting, or a real error
            adopt(c);
        }
    }

    //! Write to every client, dropping any that has gone away
    void broadcast(const std::string& line)
    {
        accept_pending();
        for (std::size_t i = 0; i < clients.size(); )
        {
            if (send_all(clients[i], line)) ++i;
            else
            {
                close_socket(clients[i]);
                clients.erase(clients.begin() + i);
            }
        }
    }

private:
    void adopt(socket_t c)
    {
        suppress_sigpipe(c);
        // A report is one short line and a reader wants it now, not
        // when the kernel has gathered enough to be worth a packet.
        int on = 1;
        ::setsockopt(c, IPPROTO_TCP, TCP_NODELAY,
                     reinterpret_cast<const char*>(&on), sizeof(on));
        clients.push_back(c);
    }

    void accept_one_blocking()
    {
        socket_t c = ::accept(listen_fd, nullptr, nullptr);
        if (c != bad_socket) adopt(c);
    }

    //! true if the whole line went out
    /*! send() is allowed to take less than it was given, so this loops.
     * A short write on a socket is rare and a report line is small, but
     * "rare" is how a corrupted stream gets shipped.
     */
    static bool send_all(socket_t s, const std::string& line)
    {
        std::size_t sent = 0;
        while (sent < line.size())
        {
#ifdef _WIN32
            int n = ::send(s, line.data() + sent,
                           static_cast<int>(line.size() - sent), send_flags);
#else
            ssize_t n = ::send(s, line.data() + sent, line.size() - sent, send_flags);
#endif
            if (n > 0) {sent += static_cast<std::size_t>(n); continue;}
            if (n < 0 && would_block()) continue;   // client is slow, not gone
            return false;                            // closed, reset, or refused
        }
        return true;
    }

    socket_t listen_fd = bad_socket;
    std::vector<socket_t> clients;
};

} // namespace detail

//! An observer that broadcasts report lines to every TCP client attached
/*! Listens on \a port of the loopback interface. Any number of clients
 * may connect, at any time, and each gets every firing reported after it
 * arrives; one that disconnects is dropped without disturbing the run.
 *
 * With \a wait_for_client the constructor blocks until the first client
 * connects, which is the named pipe's behaviour and the right choice
 * when the beginning of the run is the interesting part.
 *
 * Throws std::runtime_error if the port cannot be bound, rather than
 * running a simulation whose output silently goes nowhere.
 */
inline observer to_tcp(unsigned short port, bool wait_for_client = false)
{
    auto b = std::make_shared<detail::tcp_broadcaster>(port, wait_for_client);
    return [b](const firing& f) {b->broadcast(as_report_line(f));};
}

} // namespace reflection

} // namespace ForSyDe

#endif // FORSYDE_REFLECTION

#endif
