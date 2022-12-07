/*
 * Copyright (C) 2018-2022 Heinrich-Heine-Universitaet Duesseldorf,
 * Institute of Computer Science, Department Operating Systems
 * Burak Akguel, Christian Gesse, Fabian Ruhland, Filip Krakowski, Hannes Feil, Michael Schoettner
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#ifndef HHUOS_IP4MODULE_H
#define HHUOS_IP4MODULE_H

#include "network/NetworkModule.h"
#include "Ip4Header.h"
#include "Ip4RoutingModule.h"
#include "kernel/log/Logger.h"
#include "lib/util/stream/ByteArrayOutputStream.h"

namespace Network::Ip4 {

class Ip4Module : public NetworkModule {

public:
    /**
     * Default Constructor.
     */
    Ip4Module() = default;

    /**
     * Copy Constructor.
     */
    Ip4Module(const Ip4Module &other) = delete;

    /**
     * Assignment operator.
     */
    Ip4Module &operator=(const Ip4Module &other) = delete;

    /**
     * Destructor.
     */
    ~Ip4Module() = default;

    Ip4Interface& getInterface(const Util::Memory::String &deviceIdentifier);

    Ip4RoutingModule& getRoutingModule();

    void registerInterface(const Ip4Address &address, const Ip4Address &networkAddress, const Ip4NetworkMask &networkMask, Device::Network::NetworkDevice &device);

    void readPacket(Util::Stream::ByteArrayInputStream &stream, LayerInformation information, Device::Network::NetworkDevice &device) override;

    static const Ip4Interface &
    writeHeader(Util::Stream::ByteArrayOutputStream &stream, const Ip4Address &destinationAddress, Ip4Header::Protocol protocol,
                uint16_t payloadLength);

    static uint16_t calculateChecksum(const uint8_t *buffer, uint32_t offset, uint32_t length);

private:

    Ip4RoutingModule routingModule;
    Util::Data::ArrayList<Ip4Interface*> interfaces;

    static Kernel::Logger log;
};

}

#endif