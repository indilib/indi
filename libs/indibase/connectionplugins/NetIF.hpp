/*
Copyright (c) 2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance for Sustainable
Energy, LLC. See the top-level NOTICE for additional details. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause
*/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

// other platforms might need different headers -- detecting with macros:
// https://sourceforge.net/p/predef/wiki/OperatingSystems/
// https://stackoverflow.com/questions/142508/how-do-i-check-os-with-a-preprocessor-directive
#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "IPHLPAPI.lib")
        #pragma comment(lib, "Ws2_32.lib")
    #endif
#else
    #if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) ||                     \
        defined(__bsdi__) || defined(__DragonFly__)
        #include <sys/socket.h>
        #include <netinet/in.h>
    #endif
    #include <arpa/inet.h>
    #include <ifaddrs.h>
#endif

namespace gmlc {
namespace netif {
#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
    using IF_ADDRS = PIP_ADAPTER_ADDRESSES;
#else
    using IF_ADDRS = struct ifaddrs*;
#endif

#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
    using IF_ADDRS_UNICAST = PIP_ADAPTER_UNICAST_ADDRESS;
#else
    using IF_ADDRS_UNICAST = struct ifaddrs*;
#endif

#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

    /**
     * a helper function to convert the IP address in a sockaddr struct to text
     * @param addr a pointer to a sockaddr struct
     * @param sa_len the length of the sockaddr (primarily for systems that don't support inet_ntop)
     * @return an IP address in text form, or an empty string if conversion to text failed
     */
    inline std::string addressToString(struct sockaddr* addr, int sa_len)
    {
#if defined(__MINGW32__)
        char addr_str[NI_MAXHOST];
        if (getnameinfo(addr, sa_len, addr_str, NI_MAXHOST, 0, 0, NI_NUMERICHOST) != 0) {
            return std::string();
        }
#else
        (void)sa_len;
        int family = addr->sa_family;
        char addr_str[INET6_ADDRSTRLEN];
        void* src_addr = nullptr;
        switch (family) {
            case AF_INET:
                src_addr = &(reinterpret_cast<struct sockaddr_in*>(addr)->sin_addr);
                break;
            case AF_INET6:
                src_addr = &(reinterpret_cast<struct sockaddr_in6*>(addr)->sin6_addr);
                break;
            default:  // Invalid address type for conversion to text
                return std::string();
        }
        inet_ntop(family, src_addr, addr_str, INET6_ADDRSTRLEN);
#endif

        return std::string(addr_str);
    }

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif

    /**
     * a helper function to free the memory allocated to store a set of interface addresses
     * @param addrs a pointer to the allocated address structure (type depends on OS)
     */
    inline void freeAddresses(IF_ADDRS addrs)
    {
#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
        HeapFree(GetProcessHeap(), 0, addrs);
#else
        freeifaddrs(addrs);
#endif
    }

    /**
     * a helper function to get a list of all interface adapters using OS system calls. the returned
     * pointer from addrs must be freed with a call to freeAddresses.
     * @param family type of adapter addresses to get on Windows; one of AF_INET (IPv4), AF_INET6
     * (IPv6), or AF_UNSPEC (both)
     * @param[out] addrs pointer to a pointer to be filled in with the address of the allocated
     * address structure; must be freed when done by calling freeAddresses
     * @return 0 on success, or -1 if an error occurred
     */
    inline auto getAddresses(int family, IF_ADDRS* addrs)
    {
#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
        // Windows API:
        // https://docs.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getadaptersaddresses
        DWORD rv = 0;
        ULONG bufLen = 15000;  // recommended size from Windows API docs to avoid error
        ULONG iter = 0;
        do {
            *addrs = (IP_ADAPTER_ADDRESSES*)HeapAlloc(GetProcessHeap(), 0, bufLen);
            if (*addrs == NULL) {
                return -1;
            }

            rv = GetAdaptersAddresses(family, GAA_FLAG_INCLUDE_PREFIX, NULL, *addrs, &bufLen);
            if (rv == ERROR_BUFFER_OVERFLOW) {
                freeAddresses(*addrs);
                *addrs = NULL;
                bufLen = bufLen * 2;  // Double buffer length for the next attempt
            } else {
                break;
            }
            iter++;
        } while ((rv == ERROR_BUFFER_OVERFLOW) && (iter < 3));
        if (rv != NO_ERROR) {
            // May want to change this depending on the type of error
            return -1;
        }
        return 0;
#else
        (void)family;
        return getifaddrs(addrs);
#endif
    }

    /**
     * a helper function to get the underlying socket address structure based on OS
     * @param addr a pointer to an address structure for an (unicast) interface/adapter
     * @return a socket address structure containing the IP address information for an interface
     */
    inline auto getSockAddr(IF_ADDRS_UNICAST addr)
    {
#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
        return addr->Address.lpSockaddr;
#else
        return addr->ifa_addr;
#endif
    }

    /**
     * a helper function to get the underlying socket address length on systems where it is needed
     * @param addr a pointer to an address structure for an (unicast) interface/adapter
     * @return the socket address length
     */
    inline int getSockAddrLen(IF_ADDRS_UNICAST addr)
    {
#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
        return addr->Address.iSockaddrLength;
#else
        return sizeof(*addr->ifa_addr);
#endif
    }

    /**
     * a helper function to get the next interface/adapter address based on OS
     * @param family specify the type of address desired on non-Windows systems; one of AF_INET
     * (IPv4), AF_INET6 (IPv6), or AF_UNSPEC (both)
     * @param addrs a pointer to an addresses structure for an interface/adapter list
     * @return the next interface/adapter in the addresses list (that matches the family given for
     * non-Windows systems), or nullptr if there is no next address
     */
    inline IF_ADDRS_UNICAST getNextAddress(int family, IF_ADDRS_UNICAST addrs)
    {
#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
        (void)family;
        return addrs->Next;
#else
        auto next = addrs->ifa_next;
        while (next != NULL) {
            // Skip entries with a null sockaddr
            auto next_sockaddr = getSockAddr(next);
            if (next_sockaddr == NULL) {
                next = next->ifa_next;
                continue;
            }

            int next_family = next_sockaddr->sa_family;
            // Skip if the family is not IPv4 or IPv6
            if (next_family != AF_INET && next_family != AF_INET6) {
                next = next->ifa_next;
                continue;
            }
            // Skip if a specific IP family was requested and the family doesn't match
            if ((family == AF_INET || family == AF_INET6) && family != next_family) {
                next = next->ifa_next;
                continue;
            }
            // An address entry meeting the requirements was found, return it
            return next;
        }
        return nullptr;
#endif
    }

    /**
     * get all addresses associated with network interfaces on the system of the address family
     * given.
     * @param family the type of IP address to return; one of AF_INET (IPv4), AF_INET6 (IPv6), or
     * AF_UNSPEC (both)
     * @return a list of addresses as text
     */
    std::vector<std::string> getInterfaceAddresses(int family)
    {
        std::vector<std::string> result_list;

        IF_ADDRS allAddrs = NULL;

        getAddresses(family, &allAddrs);

#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
        WSADATA wsaData;
        if (WSAStartup(0x202, &wsaData) != 0) {
            return result_list;
        }
        auto winAddrs = allAddrs;
        while (winAddrs) {
            auto addrs = winAddrs->FirstUnicastAddress;
#else
        auto addrs = allAddrs;
#endif

            for (auto a = addrs; a != NULL; a = getNextAddress(family, a)) {
                std::string ipAddr = addressToString(getSockAddr(a), getSockAddrLen(a));
                if (!ipAddr.empty()) {
                    result_list.push_back(ipAddr);
                }
            }

#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
            winAddrs = winAddrs->Next;
        }
        WSACleanup();
#endif

        if (allAddrs) {
            freeAddresses(allAddrs);
        }
        return result_list;
    }

    /**
     * returns a list of all IPv4 addresses associated with network interfaces on the system.
     * @return a list of IPv4 addresses as text
     */
    std::vector<std::string> getInterfaceAddressesV4() { return getInterfaceAddresses(AF_INET); }

    /**
     * returns a list of all IPv6 addresses associated with network interfaces on the system.
     * @return a list of IPv6 addresses as text
     */
    std::vector<std::string> getInterfaceAddressesV6() { return getInterfaceAddresses(AF_INET6); }

    /**
     * returns a list of all IPv4 and IPv6 addresses associated with network interfaces on the
     * system.
     * @return a list of IPv4 and IPv6 addresses as text
     */
    std::vector<std::string> getInterfaceAddressesAll() { return getInterfaceAddresses(AF_UNSPEC); }
}  // namespace netif
}  // namespace gmlc
