//
// Created by christoph on 06.03.23.
//

#include "EditBufferView.h"

EditBufferView::EditBufferView(const EditBuffer &buffer) : fileBuffer(buffer.fileBuffer),
                                                           fileCursor(&buffer.fileCursor) {
    size = Util::Graphic::Ansi::getCursorLimits();

    if (size.row < 10 || size.column < 10) {
        // Arbitrary minimum size
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "View window too small!");
    }
}

void EditBufferView::moveViewUp(uint16_t repeat) {
    if (position.row == 0) {
        return;
    }

    position = {position.column, static_cast<uint16_t>(position.row - 1)};
}

void EditBufferView::moveViewDown(uint16_t repeat) {
    if (fileBuffer->size() < size.row || position.row + size.row == fileBuffer->size() - 2) {
        return;
    }

    position = {position.column, static_cast<uint16_t>(position.row + 1)};
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
    Util::String string = "";

    // Stub printer until the view can actually be moved horizontally
    volatile uint16_t maxRow = position.row + size.row < fileBuffer->size()
                      ? position.row + size.row
                      : fileBuffer->size();
    for (uint16_t row = position.row; row < maxRow; ++row) {
        string += fileBuffer->rowContent({0, row});
        if (row + 1 != maxRow) {
            // Skip last line
            string += '\n';
        }
    }

    // Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, static_cast<const char *>(Util::String::format("Debug Checkpoint: [%d], [%d]", row, maxRow)));

    return string;

    // TODO: Print only the window
}
