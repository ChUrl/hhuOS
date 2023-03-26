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

    fixView();
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

    fixView();
}

void CursorBuffer::cursorLeft() {
    const auto [rowindex, row] = getCharacterRow(cursor);
    if (cursor == row.first) {
        return;
    }

    cursor--;
    fixView();
}

void CursorBuffer::cursorRight() {
    const auto [rowindex, row] = getCharacterRow(cursor);
    if (cursor == row.second) {
        return;
    }

    cursor++;
    fixView();
}

void CursorBuffer::insertAtCursor(char character) {
    insertString(cursor++, character);
    fixView();
}

void CursorBuffer::deleteBeforeCursor() {
    if (cursor == 0) {
        return;
    }

    deleteString(--cursor, 1);
    fixView();
}

void CursorBuffer::deleteAtCursor() {
    if (cursor == buffer.size()) {
        return;
    }

    deleteString(cursor--, 1);
    fixView();
}

// auto CursorBuffer::getRowsFromCursor(uint32_t length) const -> Util::Pair<Util::Iterator<char>, Util::Iterator<char>> {
//     auto [rowindex, row] = getCharacterRow(cursor);
//     auto [firstBegin, firstEnd] = getSingleRow(rowindex);
//     auto [lastBegin, lastEnd] = getSingleRow(rows.size() > rowindex + length
//                                              ? rowindex + length
//                                              : rows.size() - 1);
//     return {firstBegin, lastEnd};
// }

auto CursorBuffer::getView() const -> Util::Pair<Util::Iterator<char>, Util::Iterator<char>> {
    auto [firstBegin, firstEnd] = getSingleRow(viewAnchor);
    auto [lastBegin, lastEnd] = getSingleRow(rows.size() > viewAnchor + viewSize - 1
                                             ? viewAnchor + viewSize - 1
                                             : rows.size() - 1);

    // TODO: Hack, the last \n may not be printed if the view is "full"
    auto lastNoNewline = lastBegin;
    auto last = getRow(rows.size() > viewAnchor + viewSize - 1
                       ? viewAnchor + viewSize - 1
                       : rows.size() - 1);
    for (uint32_t i = 0; i < last.second - last.first; ++i) {
        ++lastNoNewline;
    }

    return {firstBegin, lastNoNewline};
}

auto CursorBuffer::getViewCursor() const -> Util::Graphic::Ansi::CursorPosition {
    const auto [rowindex, row] = getCharacterRow(cursor);
    const uint32_t rowOffset = cursor - row.first;
    return {static_cast<uint16_t>(rowOffset), static_cast<uint16_t>(rowindex - viewAnchor)};
}

// TODO: Configurable scrolloff (lines always visible before/after cursor)
void CursorBuffer::fixView() {
    auto [rowindex, row] = getCharacterRow(cursor);
    if (rowindex < viewAnchor) {
        // Scroll view up
        viewAnchor = rowindex;
    } else if (rowindex >= viewAnchor + viewSize) {
        // Scroll view down
        viewAnchor = rowindex - viewSize + 1;
    }
}