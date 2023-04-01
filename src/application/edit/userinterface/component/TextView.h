//
// Created by christoph on 01.04.23.
//

#ifndef HHUOS_TEXTVIEW_H
#define HHUOS_TEXTVIEW_H

#include "application/edit/CursorBuffer.h"
#include "application/edit/userinterface/component/Component.h"

class TextView : public Component {
public:
    // TODO: This should not receive the CursorBuffer, but a general const char * buffer.
    //       The cursor management and scrolling should probably also happen here.
    //       It seems like I should ditch the ArrayList<char> in the FileBuffer and implement
    //       a raw dynamic array again after all.
    // TODO: Additional constructor with resolutionX/resolutionY
    explicit TextView(uint16_t charactersX, uint16_t charactersY, Util::Graphic::Font &font,
                      const CursorBuffer &cursorBuffer);

    ~TextView() override = default;

    void draw() override;

private:
    Util::Graphic::Font &font;
    const CursorBuffer &cursorBuffer;
};

#endif //HHUOS_TEXTVIEW_H
