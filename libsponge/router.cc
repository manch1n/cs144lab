#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    DUMMY_CODE(route_prefix, prefix_length, next_hop, interface_num);
    _routeEntries.push_back({route_prefix, prefix_length, next_hop, interface_num});
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    DUMMY_CODE(dgram);
    static const int NOT_FOUND = -1;
    uint8_t longest = 0;
    int foundIdx = NOT_FOUND;

    uint8_t ttl = dgram.header().ttl;
    if (ttl == 0 || ttl == 1) {
        return;
    } else {
        dgram.header().ttl = ttl - 1;
    }

    for (size_t i = 0; i < _routeEntries.size(); ++i) {
        const auto &[prefix, prefixLen, nextHop, interfaceNum] = _routeEntries[i];
        uint32_t netmask = getNetmask(prefixLen);
        if ((netmask & prefix) == (netmask & dgram.header().dst)) {
            if (prefixLen >= longest) {
                longest = prefixLen;
                foundIdx = i;
            }
        }
    }
    if (foundIdx != NOT_FOUND) {
        const auto &[prefix, prefixLen, nextHop, interfaceNum] = _routeEntries[foundIdx];
        AsyncNetworkInterface &outInterface = interface(interfaceNum);
        if (nextHop.has_value()) {
            outInterface.send_datagram(dgram, nextHop.value());
        } else {
            outInterface.send_datagram(dgram, Address::from_ipv4_numeric(dgram.header().dst));
        }
    }
}

uint32_t Router::getNetmask(uint8_t prefix) {
    uint32_t result = 0;
    for (int i = 0; i < prefix; ++i) {
        result |= (1 << (31 - i));
    }
    return result;
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
