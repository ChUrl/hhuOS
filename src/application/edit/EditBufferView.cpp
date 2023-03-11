//
// Created by christoph on 06.03.23.
//

#include "EditBufferView.h"

EditBufferView::EditBufferView(const EditBuffer &buffer) : fileBuffer(buffer.fileBuffer),
                                                           fileCursor(&buffer.fileCursor) {
    size = Util::Graphic::Ansi::getCursorLimits();
    // Ansi::getCursorLimits() returns the indices (start at 0)
    size = {static_cast<uint16_t>(size.column + 1), static_cast<uint16_t>(size.row + 1)};
}

void EditBufferView::moveViewUp(uint16_t repeat) {
    for (uint16_t i = 0; i < repeat; ++i) {
        if (position.row == 0) {
            return;
        }

        position = {position.column, static_cast<uint16_t>(position.row - 1)};
    }
    viewModified();
}

void EditBufferView::moveViewDown(uint16_t repeat) {
    for (uint16_t i = 0; i < repeat; ++i) {
        if (fileBuffer->size() < size.row || position.row + size.row == fileBuffer->size() - 2) {
            return;
        }

        position = {position.column, static_cast<uint16_t>(position.row + 1)};
        viewModified();
    }
}

void EditBufferView::moveViewLeft(uint16_t repeat) {
    // TODO
    viewModified();
}

void EditBufferView::moveViewRight(uint16_t repeat) {
    // TODO
    viewModified();
}

void EditBufferView::fixView() {
    if (fileCursor->row < position.row) {
        moveViewUp(position.row - fileCursor->row);
    } else if (fileCursor->row > position.row + size.row) {
        moveViewDown(fileCursor->row - (position.row + size.row));
    }
}

Util::Graphic::Ansi::CursorPosition EditBufferView::getScreenCursor() const {
    return {static_cast<uint16_t>(fileCursor->column - position.column),
            static_cast<uint16_t>(fileCursor->row - position.row)};
}

bool EditBufferView::requiresRedraw() const {
    return redraw;
}

void EditBufferView::drew() {
    redraw = false;
}

// TODO: Editing in the last line is buggy. I don't know if the printing is the problem,
//       or the cursor. It is possible, that the cursor is somehow able to skip past the
//       last line.
EditBufferView::operator Util::String() const {
    Util::String string = "";
    uint16_t maxRow = position.row + size.row;
    if (maxRow > fileBuffer->size()) {
        maxRow = fileBuffer->size();
    }

    for (uint16_t row = position.row; row < maxRow; ++row) {
        string += fileBuffer->rowContent({0, row}).substring(position.column, position.column + size.column);
        if (row + 1 != maxRow) {
            // Skip last line
            string += '\n';
        }
    }
    return string;
}

void EditBufferView::viewModified() {
    redraw = true;
}