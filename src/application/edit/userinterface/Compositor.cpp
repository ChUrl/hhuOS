//
// Created by christoph on 01.04.23.
//

#include "Compositor.h"

Compositor::Compositor(Util::Graphic::LinearFrameBuffer &lfb)
        : lfb(Util::Graphic::BufferedLinearFrameBuffer(lfb)) {}

void Compositor::setRoot(Component *component) {
    root = component;
}

void Compositor::update() {
    root->draw();
}

// TODO: The component positioning/alignment is not implemented yet
void Compositor::draw() {
    lfb.clear();
    // Copy line by line for now, without positioning/alignment
    for (uint32_t i = 0; i < root->getResolutionY(); ++i) {
        auto address = root->getBuffer().add(i * root->getPitch());
        lfb.getBuffer().add(i * lfb.getPitch()).copyRange(address, root->getPitch());
    }
    lfb.flush();
}
