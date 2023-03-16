//
// Created by christoph on 06.03.23.
//

#include "FileBuffer.h"
#include "lib/util/base/Address.h"
#include "FileBufferRow.h"

void FileBuffer::insertCharacter(Util::Graphic::Ansi::CursorPosition cursor, char character) {
    rows.get(cursor.row)->insertCharacter(cursor.column, character);
}

void FileBuffer::insertString(Util::Graphic::Ansi::CursorPosition cursor, const Util::String &string) {
    rows.get(cursor.row)->insertString(cursor.column, string);
}

void FileBuffer::deleteCharacter(Util::Graphic::Ansi::CursorPosition cursor) {
    rows.get(cursor.row)->deleteCharacter(cursor.column);
}

void FileBuffer::insertRow(Util::Graphic::Ansi::CursorPosition cursor, const Util::String &row) {
    if (cursor.row > rows.size()) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Row out of bounds!");
    }
    rows.add(cursor.row, new FileBufferRow(row));
}

void FileBuffer::appendRow(const Util::String &row) {
    rows.add(new FileBufferRow(row));
}

// TODO: Allow removing the last line?
void FileBuffer::deleteRow(Util::Graphic::Ansi::CursorPosition cursor) {
    if (rows.size() == 1) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Can't remove last line!");
    }
    rows.removeIndex(cursor.row);
}

uint16_t FileBuffer::rowSize(Util::Graphic::Ansi::CursorPosition cursor) const {
    return rows.get(cursor.row)->size();
}

void FileBuffer::rowContent(Util::Graphic::Ansi::CursorPosition cursor, uint16_t start, uint16_t end, Util::String &string) const {
    rows.get(cursor.row)->getColumns(start, end, string);
}

void FileBuffer::rowContent(Util::Graphic::Ansi::CursorPosition cursor, Util::String &string) const {
    rows.get(cursor.row)->getColumns(string);
}

uint16_t FileBuffer::size() const {
    return rows.size();
}

bool FileBuffer::isLastColumn(Util::Graphic::Ansi::CursorPosition cursor) const {
    return rows.get(cursor.row)->isLastColumn(cursor.column);
}

bool FileBuffer::isLastRow(Util::Graphic::Ansi::CursorPosition cursor) const {
    return cursor.row + 1 == rows.size();
}

void FileBuffer::getRows(Util::Array<Util::String> &rowStrings) const {
    if (rowStrings.length() != rows.size()) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Array length doesn't match FileBuffer length!");
    }

    for (uint16_t i = 0; i < rows.size(); ++i) {
        rows.get(i)->getColumns(rowStrings[i]);
    }
}
