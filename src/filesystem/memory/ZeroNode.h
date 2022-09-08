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

#ifndef HHUOS_ZERONODE_H
#define HHUOS_ZERONODE_H

#include "MemoryNode.h"

namespace Filesystem::Memory {

class ZeroNode : public MemoryNode {

public:
    /**
     * Constructor.
     */
    ZeroNode(const Util::Memory::String &name = "zero");

    /**
     * Copy Constructor.
     */
    ZeroNode(const ZeroNode &copy) = delete;

    /**
     * Assignment operator.
     */
    ZeroNode &operator=(const ZeroNode &other) = delete;

    /**
     * Destructor.
     */
    ~ZeroNode() override = default;

    /**
     * Overriding function from Node.
     */
    Util::File::Type getFileType() override;

    /**
     * Overriding function from Node.
     */
    uint64_t readData(uint8_t *targetBuffer, uint64_t pos, uint64_t numBytes) override;
};

}

#endif