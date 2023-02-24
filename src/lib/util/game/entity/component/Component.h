/*
 * Copyright (C) 2018-2023 Heinrich-Heine-Universitaet Duesseldorf,
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

#ifndef HHUOS_COMPONENT_H
#define HHUOS_COMPONENT_H

namespace Util {
namespace Game {
class Entity;
}  // namespace Game
}  // namespace Util

namespace Util::Game {

class Component {

friend class Entity;

public:
    /**
    * Constructor.
    */
    explicit Component(Entity &entity);

    /**
     * Copy Constructor.
     */
    Component(const Component &other) = delete;

    /**
     * Assignment operator.
     */
    Component &operator=(const Component &other) = delete;

    /**
     * Destructor.
     */
    ~Component() = default;

protected:

    virtual void update(double delta) = 0;

    [[nodiscard]] Entity& getEntity();

private:

    Entity &entity;
};

}

#endif