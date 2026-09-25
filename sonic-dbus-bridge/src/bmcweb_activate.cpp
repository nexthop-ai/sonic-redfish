///////////////////////////////////////
// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Nexthop AI
// Author: Shreyansh Jain <shreyansh@nexthop.ai>
// License file: sonic-redfish/LICENSE
///////////////////////////////////////

// bmcweb-activate - provide a listening socket to bmcweb the way systemd would.
//
// bmcweb takes its listening socket from systemd socket activation and, when
// there is none, falls back to a port compiled into the binary. In a container
// neither is usable: systemd cannot hand a file descriptor across the docker
// daemon into the container, and the compiled fallback is not the port we
// serve on.
//
// Socket activation itself is a small convention rather than anything
// systemd-specific: bind the socket, leave it on file descriptor 3, describe
// it in the environment, and exec the service. This does exactly that from
// inside the container, so bmcweb binds the port we were given without any
// modification to bmcweb and without a port mapping in front of it.
//
//   bmcweb-activate <port> <program> [args...]

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

namespace
{
// systemd hands the first socket over on this descriptor.
constexpr int LISTEN_FDS_START = 3;

constexpr int LISTEN_BACKLOG = 128;

int fail(std::string_view what)
{
    std::cerr << "bmcweb-activate: " << what << ": " << std::strerror(errno)
              << std::endl;
    return 1;
}

} // anonymous namespace

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: " << argv[0] << " <port> <program> [args...]"
                  << std::endl;
        return 2;
    }

    std::string_view portArg(argv[1]);
    uint16_t port = 0;
    auto [end, ec] =
        std::from_chars(portArg.data(), portArg.data() + portArg.size(), port);
    if (ec != std::errc() || end != portArg.data() + portArg.size() ||
        port == 0)
    {
        std::cerr << "bmcweb-activate: invalid port '" << portArg << "'"
                  << std::endl;
        return 2;
    }

    int fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return fail("socket");
    }

    // Without this a restart can fail to bind against a socket still in
    // TIME_WAIT, which supervisord would see as bmcweb failing to start.
    int on = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        return fail("SO_REUSEADDR");
    }

    // Serve IPv4 and IPv6 on the one socket, as bmcweb's own fallback does.
    int off = 0;
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)) < 0)
    {
        return fail("IPV6_V6ONLY");
    }

    sockaddr_in6 addr{};
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(port);

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        return fail("bind port " + std::to_string(port));
    }

    if (listen(fd, LISTEN_BACKLOG) < 0)
    {
        return fail("listen");
    }

    // dup2 both places the socket on the expected descriptor and clears
    // close-on-exec, so it survives into the program we exec.
    if (fd != LISTEN_FDS_START)
    {
        if (dup2(fd, LISTEN_FDS_START) < 0)
        {
            return fail("dup2");
        }
        close(fd);
    }

    // bmcweb splits the name on '_' and reads the third field as the protocol,
    // without checking how many fields there are, so the name must always have
    // at least three.
    std::string fdName = "bmcweb_" + std::to_string(port) + "_https_auth";

    // LISTEN_PID must match the process that receives the descriptor. exec
    // keeps the pid, so this stays correct for the program we hand over to.
    std::string pid = std::to_string(getpid());

    if (setenv("LISTEN_FDS", "1", 1) != 0 ||
        setenv("LISTEN_FDNAMES", fdName.c_str(), 1) != 0 ||
        setenv("LISTEN_PID", pid.c_str(), 1) != 0)
    {
        return fail("setenv");
    }

    execv(argv[2], &argv[2]);

    return fail(std::string("exec ") + argv[2]);
}
