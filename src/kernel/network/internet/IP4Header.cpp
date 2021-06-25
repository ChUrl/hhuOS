//
// Created by hannes on 15.06.21.
//

#include <kernel/network/NetworkDefinitions.h>
#include "IP4Header.h"

uint8_t IP4Header::calculateChecksum(uint16_t *target) {
    if (target == nullptr) {
        return 1;
    }

    auto *byteBlock = new Kernel::NetworkByteBlock(IP4HEADER_MIN_LENGTH);
    if (this->copyTo(byteBlock)) {
        return 1;
    }
    byteBlock->resetIndex();

    uint16_t tempValue = 0;
    uint16_t result = 0;

    for (uint8_t i = 0; i < 5; i++) {
        if (byteBlock->readStraightTo(&tempValue, sizeof tempValue)) {
            return 1;
        }
        result += tempValue;
    }

    *target = ~result;

    return 0;
}

IP4Header::IP4Header(IP4Address *destinationAddress, IP4DataPart *dataPart) {
    this->destinationAddress = destinationAddress;
    protocolType = dataPart->getIP4ProtocolType();

    //we use minimal header if we create one
    totalLength = (uint16_t) IP4HEADER_MIN_LENGTH + (uint16_t) dataPart->getLengthInBytes();
}

IP4Header::~IP4Header() {
    delete sourceAddress;
    delete destinationAddress;
}

size_t IP4Header::getTotalDatagramLength() const {
    return (size_t) totalLength;
}

IP4DataPart::IP4ProtocolType IP4Header::getIP4ProtocolType() const {
    return protocolType;
}

IP4Address *IP4Header::getDestinationAddress() {
    return destinationAddress;
}

IP4Address *IP4Header::getSourceAddress() {
    return sourceAddress;
}

void IP4Header::setSourceAddress(IP4Address *address) {
    //Cleanup if already set
    delete this->sourceAddress;
    this->sourceAddress = address;
}

size_t IP4Header::getHeaderLength() const {
    //IP4 header length is not fixed size
    //-> calculate it from real header value for header length!
    return (size_t) (version_headerLength - 0x40) * 4;
}

uint8_t IP4Header::copyTo(Kernel::NetworkByteBlock *output) {
    uint8_t errors = 0;
    errors += output->appendOneByte(version_headerLength);
    errors += output->appendOneByte(typeOfService);
    errors += output->appendTwoBytesSwapped(totalLength);
    errors += output->appendTwoBytesSwapped(identification);
    errors += output->appendTwoBytesSwapped(flags_fragmentOffset);
    errors += output->appendOneByte(timeToLive);
    errors += output->appendOneByte((uint8_t) protocolType);
    errors += output->appendTwoBytesSwapped(headerChecksum);

    if (errors) {
        return errors;
    }

    uint8_t addressBytes[IP4ADDRESS_LENGTH];
    sourceAddress->copyTo(addressBytes);
    errors += output->appendStraightFrom(addressBytes, IP4ADDRESS_LENGTH);

    if (errors) {
        return errors;
    }

    destinationAddress->copyTo(addressBytes);
    errors += output->appendStraightFrom(addressBytes, IP4ADDRESS_LENGTH);
    return errors;
}

uint8_t IP4Header::parse(Kernel::NetworkByteBlock *input) {
    if (input == nullptr || input->bytesRemaining() < IP4HEADER_MIN_LENGTH) {
        return 1;
    }

    if (sourceAddress != nullptr || destinationAddress != nullptr) {
        //Stop if already initialized!
        //-> no existing data is overwritten
        return 1;
    }

    uint8_t errors = 0;
    errors += input->readOneByteTo(&version_headerLength);
    errors += input->readOneByteTo(&typeOfService);
    errors += input->readTwoBytesSwappedTo(&totalLength);
    errors += input->readTwoBytesSwappedTo(&identification);
    errors += input->readTwoBytesSwappedTo(&flags_fragmentOffset);
    errors += input->readOneByteTo(&timeToLive);

    uint8_t typeValue = 0;
    errors += input->readOneByteTo(&typeValue);
    protocolType = IP4DataPart::parseIntAsIP4ProtocolType(typeValue);

    errors += input->readTwoBytesSwappedTo(&headerChecksum);

    if (errors) {
        return errors;
    }

    uint8_t addressBytes[IP4ADDRESS_LENGTH];

    errors += input->readStraightTo(addressBytes, IP4ADDRESS_LENGTH);
    sourceAddress = new IP4Address(addressBytes);

    if (errors) {
        return errors;
    }

    errors += input->readStraightTo(addressBytes, IP4ADDRESS_LENGTH);
    destinationAddress = new IP4Address(addressBytes);

    if (errors) {
        return errors;
    }

    //Skip additional bytes if incoming header is larger than our internal one
    //-> next layer would read our remaining header bytes as data otherwise!
    size_t remainingHeaderBytes = getHeaderLength() - IP4HEADER_MIN_LENGTH;
    //True if remainingHeaderBytes > 0
    if (remainingHeaderBytes) {
        auto *discardedBytes = new uint8_t[remainingHeaderBytes];
        errors += input->readStraightTo(discardedBytes, remainingHeaderBytes);
        delete[] discardedBytes;
    }//TODO: Refactor this, it will fail with checksumError that way!

    return errors;
}

bool IP4Header::headerValid() {
    if (headerChecksum == 0) {
        //Header checksum not parsed!
        return false;
    }
    uint16_t calculationResult = 0,
            tempChecksum = headerChecksum;

    //set header checksum to zero for calculation
    headerChecksum = 0;
    if (calculateChecksum(&calculationResult)) {
        return false;
    }
    //set header checksum back to previous value
    headerChecksum = tempChecksum;

    return headerChecksum == calculationResult;
}

uint8_t IP4Header::fillChecksumField() {
    if (headerChecksum != 0) {
        //Header checksum already set!
        return 1;
    }
    uint16_t calculationResult = 0;
    if (calculateChecksum(&calculationResult)) {
        return 1;
    }
    headerChecksum = calculationResult;
    return 0;
}
