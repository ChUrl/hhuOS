//
// Created by hannes on 16.05.21.
//

#ifndef HHUOS_ARPREQUEST_H
#define HHUOS_ARPREQUEST_H

#include <kernel/network/ethernet/EthernetFrame.h>
#include <kernel/network/internet/addressing/IP4Address.h>

class ARPRequest : public EthernetDataPart {
private:
    IP4Address *ip4Address;

public:
    explicit ARPRequest(IP4Address *ip4Address);

    uint8_t copyTo(NetworkByteBlock *byteBlock) override;

    size_t getLengthInBytes() override;

    EtherType getEtherType() override;

    uint8_t parse(NetworkByteBlock *input) override;
};


#endif //HHUOS_ARPREQUEST_H
