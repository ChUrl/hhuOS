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

    lineDrawer.drawLine(0, 0, getResolutionX() - 1, 0, Util::Graphic::Colors::WHITE);
    lineDrawer.drawLine(0, 0, 0, getResolutionY() - 1, Util::Graphic::Colors::WHITE);
    lineDrawer.drawLine(getResolutionX() - 1, getResolutionY() - 1, getResolutionX() - 1, 0, Util::Graphic::Colors::WHITE);
    lineDrawer.drawLine(getResolutionX() - 1, getResolutionY() - 1, 0, getResolutionY() - 1, Util::Graphic::Colors::WHITE);

    // TODO: Scrolling the buffer is probably faster because of MMX?
    //       But this is going to change anyway, after I work with const char * buffer...
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
