/*
 * Copyright (C) 2018/19 Thiemo Urselmann
 * Heinrich-Heine University
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
 *
 * Note:
 * All references marked with [...] refer to the following developers manual.
 * Intel Corporation. PCI/PCI-X Family of Gigabit Ethernet Controllers Software Developer’s Manual.
 * 317453006EN.PDF Revision 4.0. 2009.
 */

#include "ReceiveEvent.h"


ReceiveEvent::ReceiveEvent(volatile uint8_t *packet, uint16_t length) : Event() {
    this->packet = packet;
    this->length = length;
}

String ReceiveEvent::getType() const {
    return TYPE;
}

volatile uint8_t *ReceiveEvent::getPacket() {
    return this->packet;
}

uint16_t ReceiveEvent::getLength() {
    return this->length;
}

ReceiveEvent::ReceiveEvent(const ReceiveEvent &other) : Event(other) {
    this->packet = other.packet;
    this->length = other.length;
}


