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

#ifndef HHUOS_RANDOM_H
#define HHUOS_RANDOM_H

#include <cstdint>
#include "lib/util/time/Timestamp.h"

namespace Util::Math {

class Random {

public:
    /**
     * Constructor.
     */
    explicit Random(uint32_t max = UINT32_MAX, uint32_t seed = Util::Time::getSystemTime().toMilliseconds());

    /**
     * Copy Constructor.
     */
    Random(const Random &copy) = delete;

    /**
     * Assignment operator.
     */
    Random &operator=(const Random &other) = default;

    /**
     * Destructor.
     */
    ~Random() = default;

    uint32_t nextRandomNumber();

private:

    uint32_t max;
    uint32_t seed;

    static const constexpr uint32_t FACTOR = 13121797;
    static const constexpr uint32_t ADDEND = 17021856;
};

}

#endif