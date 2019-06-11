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

#include "LcDefault.h"

LcDefault::LcDefault(uint8_t *address, BitManipulation<uint8_t> *manipulation)
        : address(address), manipulation(manipulation) {}

void LcDefault::isEndOfPacket(bool enable) {
    manipulation->decide(1u << 0u, enable);
}

void LcDefault::insertFrameCheckSequence(bool enable) {
    manipulation->decide(1u << 1u, enable);
}

void LcDefault::insertChecksum(bool enable) {
    manipulation->decide(1u << 2u, enable);
}

void LcDefault::reportStatus(bool enable) {
    manipulation->decide(1u << 3u, enable);
}

void LcDefault::legacyMode(bool enable) {
    manipulation->decide(1u << 5u, !enable);
}

void LcDefault::enableVlanPacket(bool enable) {
    manipulation->decide(1u << 6u, enable);
}

void LcDefault::enableInterruptDelay(bool enable) {
    manipulation->decide(1u << 7u, enable);
}

void LcDefault::manage() {
    *address = manipulation->applyTo(0x0);
}



