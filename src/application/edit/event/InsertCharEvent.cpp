//
// Created by christoph on 26.03.23.
//

#include "InsertCharEvent.h"
#include "application/edit/CursorBuffer.h"

InsertCharEvent::InsertCharEvent(uint32_t cursor, char character) : EditEvent(cursor), character(character) {}

void InsertCharEvent::apply(CursorBuffer &cursorBuffer) {
    cursorBuffer.insertString(cursor, character);
    cursorBuffer.cursorRight();
}

void InsertCharEvent::revert(CursorBuffer &cursorBuffer) {
    cursorBuffer.deleteString(cursor, 1);
    cursorBuffer.cursorLeft();
}
