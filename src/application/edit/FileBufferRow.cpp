//
// Created by christoph on 06.03.23.
//

#include "FileBufferRow.h"
#include "lib/util/base/Address.h"

// TODO: All the stitching using String::join(...) requires multiple memory allocations,
//       does that matter to me?

// TODO: Check string for \n
FileBufferRow::FileBufferRow(const Util::String &row) : FileBufferRow() {
    columns = Util::String(row);
}

void FileBufferRow::insertCharacter(uint16_t colIndex, char character) {
    ensureAdjacentOrInBuffer(colIndex);

    columns = Util::String::join("", Util::Array<Util::String>({columns.substring(0, colIndex),
                                                                character,
                                                                columns.substring(colIndex)}));
}

void FileBufferRow::appendCharacter(char character) {
    columns += character;
}

// TODO: Check string for \n
void FileBufferRow::insertString(uint16_t colIndex, const Util::String &string) {
    ensureAdjacentOrInBuffer(colIndex);

    columns = Util::String::join("", Util::Array<Util::String>({columns.substring(0, colIndex),
                                                                string,
                                                                columns.substring(colIndex)}));
}

void FileBufferRow::appendString(const Util::String &string) {
    columns += string;
}

void FileBufferRow::deleteCharacter(uint16_t colIndex) {
    ensureInBuffer(colIndex);

    columns = Util::String::join("", Util::Array<Util::String>({columns.substring(0, colIndex),
                                                                columns.substring(colIndex + 1)}));
}

uint16_t FileBufferRow::size() const {
    return columns.length();
}

bool FileBufferRow::isLastColumn(uint16_t colIndex) const {
    ensureAdjacentOrInBuffer(colIndex);
    return colIndex == columns.length();
}

void FileBufferRow::getColumns(uint16_t start, uint16_t end, Util::String &string) const {
    string = columns.substring(start, end); // Copies String
}

void FileBufferRow::getColumns(Util::String &string) const {
    string = columns; // Copies String
}

void FileBufferRow::ensureInBuffer(uint16_t colIndex) const {
    if (colIndex >= columns.length()) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Column out of bounds!");
    }
}

void FileBufferRow::ensureAdjacentOrInBuffer(uint16_t colIndex) const {
    if (colIndex > columns.length()) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Column out of bounds!");
    }
}
