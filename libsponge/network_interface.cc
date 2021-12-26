#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`
namespace {
auto addrComp = [](const Address &left, const Address &right) { return left.ip() < right.ip(); };
}

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void NetworkInterface::requestARP(const Address &ipaddr) {
    ARPMessage msg;
    msg.opcode = ARPMessage::OPCODE_REQUEST;
    msg.sender_ethernet_address = _ethernet_address;
    msg.sender_ip_address = _ip_address.ipv4_numeric();
    msg.target_ethernet_address = {};
    msg.target_ip_address = ipaddr.ipv4_numeric();
    EthernetFrame frame =
        constructNormalEtherFrame(_ethernet_address, ETHERNET_BROADCAST, EthernetHeader::TYPE_ARP, msg.serialize());
    _frames_out.push(frame);
}

void NetworkInterface::replyARP(const Address &ipaddr, const EthernetAddress &eaddr) {
    ARPMessage msg;
    msg.opcode = ARPMessage::OPCODE_REPLY;
    msg.sender_ethernet_address = _ethernet_address;
    msg.sender_ip_address = _ip_address.ipv4_numeric();
    msg.target_ethernet_address = eaddr;
    msg.target_ip_address = ipaddr.ipv4_numeric();
    EthernetFrame frame =
        constructNormalEtherFrame(_ethernet_address, eaddr, EthernetHeader::TYPE_ARP, msg.serialize());
    _frames_out.push(frame);
}

void NetworkInterface::updateARP(const Address &addr, const EthernetAddress &eaddr) {
    _ipToEther[addr] = {eaddr, 0, 0, true};
}

void NetworkInterface::tryPushEthernetFrame(const Address &addr) {
    if (_remainDatagrams.find(addr) == _remainDatagrams.end()) {
        return;
    }
    const auto &datagrams = _remainDatagrams[addr];
    for (const auto &datagram : datagrams) {
        EthernetFrame frame = constructNormalEtherFrame(
            _ethernet_address, std::get<0>(_ipToEther[addr]), EthernetHeader::TYPE_IPv4, datagram.serialize());
        _frames_out.push(std::move(frame));
    }
    _remainDatagrams.erase(addr);
}

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address), _ipToEther(addrComp), _remainDatagrams(addrComp) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    DUMMY_CODE(dgram, next_hop, next_hop_ip);
    if (_ipToEther.find(next_hop) != _ipToEther.end() && validEtherAddr(_ipToEther[next_hop])) {
        EthernetAddress nextHopEtherAddr = std::get<0>(_ipToEther[next_hop]);
        EthernetFrame frame = constructNormalEtherFrame(
            _ethernet_address, nextHopEtherAddr, EthernetHeader::TYPE_IPv4, dgram.serialize());
        _frames_out.push(frame);
    } else {
        if (_ipToEther.find(next_hop) == _ipToEther.end()) {
            _ipToEther.insert({next_hop, {{}, 0, 0, false}});
            requestARP(next_hop);
        } else {
            size_t &msSinceLastSendARP = std::get<2>(_ipToEther[next_hop]);
            if (msSinceLastSendARP >= WAIT_ARP_REPLY_TIME) {
                requestARP(next_hop);
                msSinceLastSendARP = 0;
            }
        }
        _remainDatagrams[next_hop].push_back(dgram);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    DUMMY_CODE(frame);
    if (frame.header().dst != ETHERNET_BROADCAST && frame.header().dst != _ethernet_address) {
        return {};
    }
    if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage msg;
        if (msg.parse(frame.payload()) != ParseResult::NoError) {
            return {};
        }
        if (msg.target_ip_address != _ip_address.ipv4_numeric()) {
            return {};
        }
        if (msg.opcode == ARPMessage::OPCODE_REPLY && msg.target_ethernet_address == _ethernet_address) {
            updateARP(Address::from_ipv4_numeric(msg.sender_ip_address), msg.sender_ethernet_address);
        } else if (msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ethernet_address == EthernetAddress{}) {
            updateARP(Address::from_ipv4_numeric(msg.sender_ip_address), msg.sender_ethernet_address);
            replyARP(Address::from_ipv4_numeric(msg.sender_ip_address), msg.sender_ethernet_address);
        }
        tryPushEthernetFrame(Address::from_ipv4_numeric(msg.sender_ip_address));
        return {};
    } else if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram datagram;
        if (datagram.parse(frame.payload()) != ParseResult::NoError) {
            return {};
        }
        return {datagram};
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    DUMMY_CODE(ms_since_last_tick);
    auto iter = _ipToEther.begin();
    for (; iter != _ipToEther.end();) {
        auto &[eaddr, expired, lastSent, valid] = iter->second;
        expired += ms_since_last_tick;
        lastSent += ms_since_last_tick;
        if (expired >= EXPIRED_TIME) {
            iter = _ipToEther.erase(iter);
        } else {
            iter++;
        }
    }
}
