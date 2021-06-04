//
// Created by hannes on 14.05.21.
//

#ifndef HHUOS_IP4DATAGRAM_H
#define HHUOS_IP4DATAGRAM_H

#define IP4DATAPART_MAX_LENGTH 576 //Recommended in RFC 791 page 13
#define IP4HEADER_MAX_LENGTH 60 //RFC 791 page 13

#include "lib/libc/stdlib.h"
#include <kernel/network/ethernet/EthernetDataPart.h>
#include <kernel/network/NetworkByteBlock.h>
#include <kernel/network/internet/icmp/messages/GenericICMP4Message.h>
#include <kernel/network/udp/UDPDatagram.h>
#include "addressing/IP4Address.h"
#include "IP4DataPart.h"

class IP4Datagram : public EthernetDataPart {
private:
    //Defined internally, should not be visible outside
    //Usage of IP4Datagram should only happen via given public methods
    //-> changing our header's internal representation is possible at any time then!
    typedef struct ip4header {
        //First four bits are value "4" -> IPv4
        //Second four bits are value "5" -> 5 "lines" header length, 4 Bytes per line
        uint8_t version_headerLength = 0x45;

        //standard type of service, no priority etc.
        uint8_t typeOfService = 0;

        uint16_t totalLength = 0;

        //fragmentation not used here, fragment parameters not set
        uint16_t identification = 0;
        uint16_t flags_fragmentOffset = 0;

        //solid default value for small LANs, can be set from constructor if necessary
        uint8_t timeToLive = 64;

        uint8_t protocolType = 0;
        uint16_t headerChecksum = 0;
        uint8_t sourceAddress[IP4ADDRESS_LENGH]{0, 0, 0, 0};
        uint8_t destinationAddress[IP4ADDRESS_LENGH]{0, 0, 0, 0};
    } ip4Header_t;

    ip4Header_t header;

    IP4Address *destinationAddress = nullptr;
    IP4Address *sourceAddress = nullptr;
    IP4DataPart *ip4DataPart = nullptr;

public:

    IP4Datagram(IP4Address *destinationAddress, IP4DataPart *ip4DataPart);

    virtual ~IP4Datagram();

    void setSourceAddress(IP4Address *source);

    GenericICMP4Message *buildGenericICMP4MessageWithInput();

    [[nodiscard]] IP4DataPart::IP4ProtocolType getIP4ProtocolType() const;

    [[nodiscard]] IP4Address *getSourceAddress() const;

    [[nodiscard]] IP4Address *getDestinationAddress() const;

    uint8_t copyTo(NetworkByteBlock *output) override;

    size_t getLengthInBytes() override;

    EtherType getEtherType() override;

    uint8_t parse(NetworkByteBlock *input) override;

    UDPDatagram *buildUDPDatagramWithInput();
};


#endif //HHUOS_IP4DATAGRAM_H
