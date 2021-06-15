//
// Created by hannes on 17.05.21.
//

#include <lib/libc/printf.h>
#include "UDP4Datagram.h"

UDP4Datagram::UDP4Datagram(UDP4Port *sourcePort, UDP4Port *destinationPort, uint8_t *outgoingBytes, size_t length) {
    this->dataBytes=new NetworkByteBlock(length);
    this->dataBytes->append(outgoingBytes, length);
    this->dataBytes->resetIndex();

    destinationPort->copyTo(header.destinationPort);
    sourcePort->copyTo(header.sourcePort);
    header.length = (uint16_t) getLengthInBytes();
    //TODO: Implement checksum calculation
    header.checksum = 0;

    this->sourcePort = sourcePort;
    this->destinationPort = destinationPort;
}

UDP4Datagram::~UDP4Datagram() {
    //delete on nullptr simply does nothing!
    delete dataBytes;
    delete destinationPort;
    delete sourcePort;
}

uint8_t UDP4Datagram::copyTo(NetworkByteBlock *output) {
    if (
            dataBytes == nullptr ||
            output == nullptr ||
            dataBytes->getLength() > (size_t) (UDP4DATAPART_MAX_LENGTH - sizeof header) ||
            dataBytes->getLength() == 0 ||
            sizeof header > UDP4HEADER_MAX_LENGTH
            ) {
        return 1;
    }

    uint8_t errors = 0;
    errors += output->append(header.sourcePort);
    errors += output->append(header.destinationPort);
    errors += output->append(header.length);
    errors += output->append(header.checksum);

    //True if errors>0
    if (errors) {
        return errors;
    }

    //Append dataBytes if no errors occurred yet
    output->append(dataBytes, dataBytes->getLength());
    return 0;
}

size_t UDP4Datagram::getLengthInBytes() {
    return (sizeof header) + dataBytes->getLength();
}

IP4DataPart::IP4ProtocolType UDP4Datagram::getIP4ProtocolType() {
    return IP4ProtocolType::UDP;
}

uint8_t UDP4Datagram::parseHeader(NetworkByteBlock *input) {
    if (input == nullptr || input->bytesRemaining() < sizeof this->header) {
        return 1;
    }
    uint8_t errors = 0;
    errors += input->read(&header.sourcePort);
    errors += input->read(&header.destinationPort);
    errors += input->read(&header.length);
    errors += input->read(&header.checksum);

    this->destinationPort = new UDP4Port(header.destinationPort);
    this->sourcePort = new UDP4Port(header.sourcePort);

    return errors;
}

UDP4Port *UDP4Datagram::getDestinationPort() const{
    return destinationPort;
}