//
// Created by christoph on 17.03.23.
//

#include "CursorBuffer.h"

CursorBuffer::CursorBuffer(const Util::String &path) : FileBuffer(path) {}

void CursorBuffer::cursorUp() {
    const auto [rowindex, row] = getCharacterRow(cursor);
    if (rowindex == 0) {
        return;
    }

    const uint32_t rowOffset = cursor - row.first;
    const Row targetRow = getRow(rowindex - 1);
    if (rowOffset <= targetRow.second - targetRow.first) {
        // Cursor fits in target row
        cursor = targetRow.first + rowOffset;
    } else {
        cursor = targetRow.second;
    }
}

void CursorBuffer::cursorDown() {
    const auto [rowindex, row] = getCharacterRow(cursor);
    if (rowindex + 1 == rows.size()) {
        return;
    }

    const uint32_t rowOffset = cursor - row.first;
    const Row targetRow = getRow(rowindex + 1);
    if (rowOffset < targetRow.second - targetRow.first) {
        // Cursor fits in target row
        cursor = targetRow.first + rowOffset;
    } else {
        cursor = targetRow.second;
    }
}

void CursorBuffer::cursorLeft() {
    const auto [rowindex, row] = getCharacterRow(cursor);
    if (cursor == row.first) {
        return;
    }

    cursor--;
}

void CursorBuffer::cursorRight() {
    const auto [rowindex, row] = getCharacterRow(cursor);
    if (cursor == row.second) {
        return;
    }

    cursor++;
}

void CursorBuffer::insertAtCursor(char character) {
    insertString(cursor++, character);
}

void CursorBuffer::deleteBeforeCursor() {
    if (cursor == 0) {
        return;
    }

    deleteString(cursor--, 1);
}

auto CursorBuffer::getScreenCursor() const -> Util::Graphic::Ansi::CursorPosition {
    const auto [rowindex, row] = getCharacterRow(cursor);
    const uint32_t rowOffset = cursor - row.first;
    return {static_cast<uint16_t>(rowOffset), static_cast<uint16_t>(rowindex)};
}

