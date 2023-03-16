//
// Created by christoph on 06.03.23.
//

#include "EditBufferView.h"

// Some Notes:
// ! The ScreenCursor may be equal to the position (row or col)
// ! The ScreenCursor may not be equal to the position + size (row and col)
// ! The ScreenCursor may not be equal to the fileBuffer->size() (row)

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
        viewModified();
    }
}

void EditBufferView::moveViewDown(uint16_t repeat) {
    for (uint16_t i = 0; i < repeat; ++i) {
        if (fileBuffer->size() <= size.row || position.row + size.row == fileBuffer->size()) {
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

// TODO: Implement horizontal scrolling
void EditBufferView::fixView() {
    // Fix after cursor movement
    if (fileCursor->row < position.row) {
        moveViewUp(position.row - fileCursor->row);
    } else if (fileCursor->row >= position.row + size.row) {
        moveViewDown((fileCursor->row + 1) - (position.row + size.row));
    }

    // Fix after file length decrease
    if (position.row + size.row >= fileBuffer->size()) {
        moveViewUp(position.row + size.row - fileBuffer->size());
    }
}

Util::Graphic::Ansi::CursorPosition EditBufferView::getScreenCursor() const {
    Util::Graphic::Ansi::CursorPosition screenCursor = {static_cast<uint16_t>(fileCursor->column - position.column),
                                                        static_cast<uint16_t>(fileCursor->row - position.row)};

    if (screenCursor.column < position.column || screenCursor.row < position.row
        || screenCursor.column >= position.column + size.column || screenCursor.row >= position.row + size.row) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Screen cursor not in view!");
    }

    return screenCursor;
}

bool EditBufferView::requiresRedraw() const {
    return redraw;
}

void EditBufferView::drew() {
    redraw = false;
}

Util::Graphic::Ansi::CursorPosition EditBufferView::dimensions() const {
    return size;
}

void EditBufferView::getWindow(Util::Array<Util::String> &window) const {
    uint16_t minRow = position.row; // Inclusive
    uint16_t maxRow = position.row + size.row; // Exclusive
    if (maxRow > fileBuffer->size()) {
        maxRow = fileBuffer->size();
    }

    for (uint16_t row = minRow; row < maxRow; ++row) {
        fileBuffer->rowContent({0, row}, position.column, position.column + size.column, window[row - minRow]);
    }
}

void EditBufferView::viewModified() {
    redraw = true;
}