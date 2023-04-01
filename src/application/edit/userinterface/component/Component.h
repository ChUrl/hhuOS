//
// Created by christoph on 01.04.23.
//

#ifndef HHUOS_COMPONENT_H
#define HHUOS_COMPONENT_H

#include "lib/util/collection/ArrayList.h"
#include "lib/util/collection/Pair.h"
#include "lib/util/graphic/LinearFrameBuffer.h"
#include "lib/util/graphic/PixelDrawer.h"
#include "lib/util/graphic/StringDrawer.h"
#include "lib/util/graphic/LineDrawer.h"
#include "lib/util/graphic/BufferScroller.h"

class Component : public Util::Graphic::LinearFrameBuffer {
public:
    // enum Alignment : uint8_t {
    //     LEFT,
    //     RIGHT,
    //     TOP,
    //     BOTTOM,
    //     CENTER
    // };

    // struct Position : public Util::Pair<uint32_t, uint32_t> {
    //     Position(uint32_t x, uint32_t y);

    //     ~Position() = default;

    //     [[nodiscard]] auto x() const -> uint32_t;

    //     [[nodiscard]] auto y() const -> uint32_t;
    // };

public:
    Component(uint16_t resolutionX, uint16_t resolutionY);

    ~Component() override;

    virtual void draw() = 0;

protected:
    Util::Graphic::PixelDrawer pixelDrawer;
    Util::Graphic::LineDrawer lineDrawer;
    Util::Graphic::StringDrawer stringDrawer;
    Util::Graphic::BufferScroller bufferScroller;

    // Position origin;

    // Util::ArrayList<Component *> children;
};

#endif //HHUOS_COMPONENT_H
