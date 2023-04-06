//
// Created by christoph on 06.04.23.
//

#include "BdfFont.h"

Util::Graphic::BdfFont::BdfFont(uint8_t charWidth, uint8_t charHeight, uint8_t *fontData, uint16_t *charLookup)
: Font(charWidth, charHeight, fontData), charLookup(charLookup) {}

uint8_t *Util::Graphic::BdfFont::getChar(uint8_t c) const {
    // return &fontData[charMemSize * c]; (Font.cpp)

    // NOTE: The lookup table is not needed for ASCII characters, as those are in the usual order.
    //       They start with "Space" (ASCII 32), so just subtract this offset for the lookup.
    //       If the UNICODE characters are to be used, the index might have to be searched in the indices.
    // NOTE: The bdf2c tool generates arrays with 48 bytes per letter, instead of width * height bits
    return &fontData[48 * (c - 32)];
}
