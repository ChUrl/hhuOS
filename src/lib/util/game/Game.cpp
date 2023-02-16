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

#include "Game.h"

namespace Util {
namespace Game {
class Graphics2D;
class KeyListener;
class MouseListener;
class Camera;
}  // namespace Game
}  // namespace Util

namespace Util::Game {

Game::~Game() {
    for (const auto *drawable : entities) {
        delete drawable;
    }

    entities.clear();
}

void Game::addObject(Entity *object) {
    addList.add(object);
}

void Game::removeObject(Entity *object) {
    removeList.add(object);
}

void Game::applyChanges() {
    for (auto *object : addList) {
        entities.add(object);
    }

    for (auto *object : removeList) {
        entities.remove(object);
        delete object;
    }

    addList.clear();
    removeList.clear();
}

void Game::updateEntities(double delta) {
    for (auto *object : entities) {
        object->update(delta);
    }
}

void Game::draw(Graphics2D &graphics) {
    for (const auto *object : entities) {
        object->draw(graphics);
    }
}

bool Game::isRunning() const {
    return running;
}

void Game::stop() {
    Game::running = false;
}

uint32_t Game::getObjectCount() const {
    return entities.size();
}

void Game::setKeyListener(KeyListener &listener) {
    keyListener = &listener;
}

void Game::setMouseListener(MouseListener &listener) {
    mouseListener = &listener;
}

Camera& Game::getCamera() {
    return camera;
}

}