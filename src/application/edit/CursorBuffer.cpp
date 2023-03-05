//
// Created by christoph on 17.03.23.
//

#include "CursorBuffer.h"
#include "application/edit/event/InsertCharEvent.h"
#include "application/edit/event/DeleteCharEvent.h"

CursorBuffer::CursorBuffer(const Util::String &path)
        : FileBuffer(path), viewSize(Util::Graphic::Ansi::getCursorLimits().row + 1) {}

auto CursorBuffer::cursorUp() -> bool {
    const auto [rowindex, row] = getRowByChar(cursor);
    if (rowindex == 0) {
        return false;
    }

    const uint32_t rowOffset = cursor - row.first;
    const Row targetRow = getRow(rowindex - 1);
    if (rowOffset <= targetRow.second - targetRow.first) {
        // Cursor fits in target row
        cursor = targetRow.first + rowOffset;
    } else {
        cursor = targetRow.second;
    }

    return alignViewToCursor();
}

auto CursorBuffer::cursorDown() -> bool {
    const auto [rowindex, row] = getRowByChar(cursor);
    if (rowindex + 1 == rows.size()) {
        return false;
    }

    const uint32_t rowOffset = cursor - row.first;
    const Row targetRow = getRow(rowindex + 1);
    if (rowOffset < targetRow.second - targetRow.first) {
        // Cursor fits in target row
        cursor = targetRow.first + rowOffset;
    } else {
        cursor = targetRow.second;
    }

    return alignViewToCursor();
}

auto CursorBuffer::cursorLeft() -> bool {
    if (cursor == 0) {
        return false;
    }

    cursor--;
    return alignViewToCursor();
}

auto CursorBuffer::cursorRight() -> bool {
    if (cursor + 1 == buffer.size()) {
        return false;
    }

    cursor++;
    return alignViewToCursor();
}

auto CursorBuffer::insertAtCursor(char character) -> EditEvent * {
    EditEvent *event = new InsertCharEvent(cursor, character);
    event->apply(*this);
    return event;
}

auto CursorBuffer::deleteBeforeCursor() -> EditEvent * {
    if (cursor == 0) {
        return nullptr;
    }

    EditEvent *event = new DeleteCharEvent(cursor - 1, buffer.get(cursor - 1));
    event->apply(*this);
    return event;
}

auto CursorBuffer::getViewIterators() const -> Util::Pair<Util::Iterator<char>, Util::Iterator<char>> {
    auto [firstBegin, firstEnd] = getRowIterators(viewAnchor);
    auto [lastBegin, lastEnd] = getRowIterators(rows.size() > viewAnchor + viewSize - 1
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

auto CursorBuffer::getRelativeViewCursor() const -> Util::Graphic::Ansi::CursorPosition {
    const auto [rowindex, row] = getRowByChar(cursor);
    const uint32_t rowOffset = cursor - row.first;
    return {static_cast<uint16_t>(rowOffset), static_cast<uint16_t>(rowindex - viewAnchor)};
}

// TODO: Configurable scrolloff (lines always visible before/after cursor)
auto CursorBuffer::alignViewToCursor() -> bool {
    auto [rowindex, row] = getRowByChar(cursor);
    if (rowindex < viewAnchor) {
        // Scroll view up
        viewAnchor = rowindex;
        return true;
    } else if (rowindex >= viewAnchor + viewSize) {
        // Scroll view down
        viewAnchor = rowindex - viewSize + 1;
        return true;
    }

    return false;
}