//
// Created by christoph on 06.03.23.
//

#include "EditBufferView.h"

EditBufferView::EditBufferView(const EditBuffer &buffer) : fileBuffer(buffer.fileBuffer),
                                                           fileCursor(&buffer.fileCursor) {
    size = Util::Graphic::Ansi::getCursorLimits();
}

void EditBufferView::moveViewUp(uint16_t repeat) {
    // TODO
}

void EditBufferView::moveViewDown(uint16_t repeat) {
    // TODO
}

void EditBufferView::moveViewLeft(uint16_t repeat) {
    // TODO
}

void EditBufferView::moveViewRight(uint16_t repeat) {
    // TODO
}

Util::Graphic::Ansi::CursorPosition EditBufferView::getScreenCursor() const {
    return {static_cast<uint16_t>(fileCursor->column - position.column),
            static_cast<uint16_t>(fileCursor->row - position.row)};
}

EditBufferView::operator Util::String() const {
    auto string = Util::String();

    // Stub printer until the view can actually be moved
    for (uint16_t row = 0; row < fileBuffer->size(); ++row) {
        string += fileBuffer->rowContent(row);
        string += '\n';
    }
    return string;

    // TODO: Print only the window
}
