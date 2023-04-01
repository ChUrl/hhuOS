//
// Created by christoph on 01.04.23.
//

#ifndef HHUOS_COMPOSITOR_H
#define HHUOS_COMPOSITOR_H

#include "application/edit/userinterface/component/Component.h"
#include "lib/util/collection/ArrayList.h"
#include "lib/util/graphic/BufferedLinearFrameBuffer.h"

class Compositor {
public:
    explicit Compositor(Util::Graphic::LinearFrameBuffer &lfb);

    ~Compositor() = default;

    void setRoot(Component *component);

    void update();

    void draw();

private:
    Util::Graphic::BufferedLinearFrameBuffer lfb;
    Component *root = nullptr;
};

#endif //HHUOS_COMPOSITOR_H
