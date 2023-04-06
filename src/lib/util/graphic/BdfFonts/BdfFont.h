//
// Created by christoph on 06.04.23.
//

#ifndef HHUOS_BDFFONT_H
#define HHUOS_BDFFONT_H

#include "lib/util/graphic/Font.h"

namespace Util::Graphic {

class BdfFont : public Util::Graphic::Font {
public:
    BdfFont(uint8_t charWidth, uint8_t charHeight, uint8_t *fontData, uint16_t *charLookup);

    BdfFont& operator=(const BdfFont &other) = delete;

    BdfFont(const BdfFont &copy) = delete;

    ~BdfFont() = default;

    [[nodiscard]] uint8_t *getChar(uint8_t c) const override;

private:
    uint16_t *charLookup;
};

}

#endif //HHUOS_BDFFONT_H
