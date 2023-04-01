//
// Created by christoph on 01.04.23.
//

#include "TextView.h"
#include "lib/util/graphic/Font.h"
#include "lib/util/graphic/Colors.h"

TextView::TextView(uint16_t charactersX, uint16_t charactersY, Util::Graphic::Font &font,
                   const CursorBuffer &cursorBuffer)
        : Component(charactersX * font.getCharWidth(), charactersY * font.getCharHeight()),
          font(font), cursorBuffer(cursorBuffer) {}

void TextView::draw() {
    clear();

    // TODO
    uint16_t x = 0, y = 0;
    auto [begin, end] = cursorBuffer.getViewIterators();
    for (auto it = begin; it != end; ++it) {
        if (*it == '\n') {
            x = 0;
            y++;
            continue;
        }

        stringDrawer.drawChar(font, x * font.getCharWidth(), y * font.getCharHeight(),
                              *it, Util::Graphic::Colors::WHITE, Util::Graphic::Colors::BLACK);
        x++;
    }
}
