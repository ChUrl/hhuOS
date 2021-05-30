//
// Created by hannes on 17.05.21.
//

#ifndef HHUOS_ICMP4DESTINATIONUNREACHABLE_H
#define HHUOS_ICMP4DESTINATIONUNREACHABLE_H


#include <kernel/network/internet/icmp/ICMP4Message.h>
#include <kernel/network/NetworkByteBlock.h>

class ICMP4DestinationUnreachable : public ICMP4Message {
public:
    //Sending constructor
    ICMP4DestinationUnreachable();

    //Receiving constructor
    ICMP4DestinationUnreachable(IP4DataPart *dataPart);

    uint8_t copyDataTo(NetworkByteBlock *byteBlock) override;

    uint16_t getLengthInBytes() override;

    ICMP4MessageType getICMP4MessageType() override;

    void *getMemoryAddress() override;
};


#endif //HHUOS_ICMP4DESTINATIONUNREACHABLE_H
