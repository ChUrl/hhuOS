/*
 * Copyright (C) 2018-2022 Heinrich-Heine-Universitaet Duesseldorf,
 * Institute of Computer Science, Department Operating Systems
 * Burak Akguel, Christian Gesse, Fabian Ruhland, Filip Krakowski, Michael Schoettner
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

#ifndef HHUOS_DATAGRAMSOCKET_H
#define HHUOS_DATAGRAMSOCKET_H

#include "Socket.h"
#include "Datagram.h"
#include "lib/util/async/Spinlock.h"
#include "lib/util/data/ArrayListBlockingQueue.h"

namespace Network {

namespace Udp {
class UdpModule;
}

namespace Ip4 {
class Ip4Module;
}

namespace Ethernet {
class EthernetModule;
}

class DatagramSocket : public Socket {

friend class Udp::UdpModule;
friend class Ip4::Ip4Module;
friend class Ethernet::EthernetModule;

public:
    /**
     * Default Constructor.
     */
    DatagramSocket() = default;

    /**
     * Copy Constructor.
     */
    DatagramSocket(const DatagramSocket &other) = delete;

    /**
     * Assignment operator.
     */
    DatagramSocket &operator=(const DatagramSocket &other) = delete;

    /**
     * Destructor.
     */
    ~DatagramSocket() = default;

    virtual void send(const Datagram &datagram) = 0;

    const Datagram& receive();

private:

    void handleIncomingDatagram(Datagram *datagram);

    Util::Async::Spinlock lock;
    Util::Data::ArrayListBlockingQueue<Datagram*> incomingDatagramQueue;
};

}

#endif