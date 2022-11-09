#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>
#include <cassert>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

constexpr EthernetAddress ETHERNET_UNDEFINED = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address), table(), waitingIpDatagram() {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    // const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    // DUMMY_CODE(dgram, next_hop, next_hop_ip);
    const auto tableEntity = table.find(next_hop);
    const bool isInTable = tableEntity != table.end();

    EthernetFrame frame;
    auto& frameHeader = frame.header();
    frameHeader.src = _ethernet_address;

    if(isInTable && tableEntity->second.eAddr != ETHERNET_UNDEFINED) {
        frame.payload() = dgram.serialize();

        frameHeader.dst = tableEntity->second.eAddr;
        frameHeader.type = EthernetHeader::TYPE_IPv4;            

        _frames_out.push(frame);

        return;
    } else if(!isInTable){
        ARPMessage arpM;
        arpM.opcode = ARPMessage::OPCODE_REQUEST;
        arpM.sender_ethernet_address = _ethernet_address;
        arpM.sender_ip_address = _ip_address.ipv4_numeric();
        arpM.target_ip_address = next_hop.ipv4_numeric();

        frame.payload() = arpM.serialize();

        frameHeader.dst = ETHERNET_BROADCAST;
        frameHeader.type = EthernetHeader::TYPE_ARP;

        _frames_out.push(frame);

        table[next_hop] = EthernetAddressAndTime{ETHERNET_UNDEFINED, 1000 * 5};
    }

    waitingIpDatagram[next_hop].push_back(dgram); 
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    // DUMMY_CODE(frame);
    const auto& header = frame.header();
    if(header.dst != _ethernet_address && 
        header.dst != ETHERNET_BROADCAST)
        return {};
    
    const auto& payload = frame.payload();

    if(header.type == EthernetHeader::TYPE_IPv4) {

        InternetDatagram ipDatagram;
        const auto& parseRes = ipDatagram.parse(payload);
        if(parseRes != ParseResult::NoError) 
            return {};

        return ipDatagram;
    } else if (header.type == EthernetHeader::TYPE_ARP) {
        ARPMessage arpMes;
        const auto& parseRes = arpMes.parse(payload);
        if(parseRes != ParseResult::NoError) 
            return {};

        //学习其中的地址映射
        Address senderAddr = Address::from_ipv4_numeric(arpMes.sender_ip_address);
        
        table[senderAddr] = EthernetAddressAndTime{arpMes.sender_ethernet_address, 30 * 1000};

        if(waitingIpDatagram[senderAddr].size()) {
            for(const auto& dgram: waitingIpDatagram[senderAddr])
                send_datagram(dgram, senderAddr);
            
            waitingIpDatagram[senderAddr].clear();
        }

        //如果是Request， 回复一个Reply
        if(arpMes.opcode == ARPMessage::OPCODE_REQUEST && 
        arpMes.target_ip_address == _ip_address.ipv4_numeric()) {
            ARPMessage reply;
            reply.opcode = ARPMessage::OPCODE_REPLY;
            reply.sender_ethernet_address = _ethernet_address;
            reply.sender_ip_address = _ip_address.ipv4_numeric();
            reply.target_ethernet_address = arpMes.sender_ethernet_address;
            reply.target_ip_address = arpMes.sender_ip_address;

            EthernetFrame replyEth;
            replyEth.payload() = reply.serialize();

            auto& framHeader = replyEth.header();
            framHeader.type = EthernetHeader::TYPE_ARP;
            framHeader.dst = arpMes.sender_ethernet_address;
            framHeader.src = _ethernet_address;
            _frames_out.push(replyEth);
        }
    }

    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    // DUMMY_CODE(ms_since_last_tick); 
    decltype(table) tableAfter;
    for(auto&& entity: table) {
        auto& t = entity.second.ms2Expired;
        if(t > ms_since_last_tick) {
            t -= ms_since_last_tick;
            tableAfter.emplace(move(entity));
        }
    }
    table = tableAfter;
}
