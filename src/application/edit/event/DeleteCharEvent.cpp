//
// Created by christoph on 26.03.23.
//

#include "DeleteCharEvent.h"
#include "application/edit/CursorBuffer.h"

DeleteCharEvent::DeleteCharEvent(uint32_t cursor, char character) : EditEvent(cursor), character(character) {}

void DeleteCharEvent::apply(CursorBuffer &cursorBuffer) {
    cursorBuffer.deleteString(cursor, 1);
    cursorBuffer.cursorLeft();
}

void DeleteCharEvent::revert(CursorBuffer &cursorBuffer) {
    cursorBuffer.insertString(cursor, character);
    cursorBuffer.cursorRight();
}
