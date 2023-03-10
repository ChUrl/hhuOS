//
// Created by christoph on 06.03.23.
//

#include "FileBuffer.h"
#include "lib/util/base/Address.h"
#include "FileBufferRow.h"

FileBuffer::FileBuffer() : capacity(INITIAL_ROWS), rows(new FileBufferRow *[capacity]) {
    for (uint16_t i = 0; i < capacity; ++i) {
        rows[i] = nullptr;
    }
}

FileBuffer::FileBuffer(const Util::String &string) : FileBuffer() {
    // TODO: This doesn't read multiple contiguous newlines
    // for (const auto &row : string.split("\n")) {
    //     appendRow(row);
    // }
    Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "Not implemented");
}

FileBuffer::~FileBuffer() {
    delete[] rows;
}

void FileBuffer::insertCharacter(Util::Graphic::Ansi::CursorPosition cursor, char character) {
    ensureAdjacentToBuffer(cursor.row);
    if (rows[cursor.row] == nullptr) {
        insertRow(cursor);
    }
    rows[cursor.row]->insertCharacter(cursor.column, character);
}

void FileBuffer::insertString(Util::Graphic::Ansi::CursorPosition cursor, const Util::String &string) {
    ensureAdjacentToBuffer(cursor.row);
    if (rows[cursor.row] == nullptr) {
        insertRow(cursor);
    }
    rows[cursor.row]->insertString(cursor.column, string);
}

void FileBuffer::deleteCharacter(Util::Graphic::Ansi::CursorPosition cursor) {
    ensureInBuffer(cursor.row);
    rows[cursor.row]->deleteCharacter(cursor.column);
}

void FileBuffer::insertRow(Util::Graphic::Ansi::CursorPosition cursor, const Util::String &row) {
    ensureAdjacentToBuffer(cursor.row);
    makeSpace(cursor.row); // Increases length and ensures capacity
    rows[cursor.row] = new FileBufferRow(row); // Clear out previous row
}

void FileBuffer::appendRow(const Util::String &row) {
    insertRow({0, length}, row);
}

// TODO: Allow removing the last line?
void FileBuffer::deleteRow(Util::Graphic::Ansi::CursorPosition cursor) {
    ensureInBuffer(cursor.row);
    if (length == 1) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Can't remove last line!");
    }
    removeSpace(cursor.row); // Decreases length
}

// Note: This does not work for the EOF row!
uint16_t FileBuffer::rowSize(Util::Graphic::Ansi::CursorPosition cursor) const {
    ensureInBuffer(cursor.row);
    return rows[cursor.row]->size();
}

Util::String FileBuffer::rowContent(Util::Graphic::Ansi::CursorPosition cursor) const {
    ensureInBuffer(cursor.row);
    if (rows[cursor.row] == nullptr) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, static_cast<const char *>(Util::String::format("Row [%d] not initialized!", cursor.row)));
    }
    return static_cast<Util::String>(*rows[cursor.row]);
}

uint16_t FileBuffer::size() const {
    return length;
}

bool FileBuffer::isLastColumn(Util::Graphic::Ansi::CursorPosition cursor) const {
    ensureInBuffer(cursor.row);
    return rows[cursor.row]->isLastColumn(cursor.column);
}

bool FileBuffer::isLastRow(Util::Graphic::Ansi::CursorPosition cursor) const {
    ensureInBuffer(cursor.row);
    return cursor.row + 1 == length;
}

bool FileBuffer::isEof(Util::Graphic::Ansi::CursorPosition cursor) const {
    ensureAdjacentToBuffer(cursor.row);
    return cursor.row == length;
}

FileBuffer::operator Util::String() const {
    Util::String string = "";
    for (uint16_t row = 0; row < length; ++row) {
        string += static_cast<Util::String>(*rows[row]);
        string += "\n";
    }
    return string;
}

void FileBuffer::ensureInBuffer(uint16_t rowIndex) const {
    if (rowIndex >= length) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Row out of bounds!");
    }
}

void FileBuffer::ensureAdjacentToBuffer(uint16_t rowIndex) const {
    if (rowIndex > length) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Row out of bounds!");
    }
}

void FileBuffer::ensureCapacity(uint16_t insertSize) {
    if (capacity <= length + insertSize) {
        while (capacity <= length + insertSize) {
            capacity *= 2;
        }
        auto **newRows = new FileBufferRow *[capacity]; // TODO: Why NullPointerException?
        for (uint16_t i = 0; i < capacity; ++i) {
            newRows[i] = nullptr;
        }

        // Copy contents from the old buffer
        auto sourceAddress = Util::Address(reinterpret_cast<uint32_t>(rows));
        auto targetAddress = Util::Address(reinterpret_cast<uint32_t>(newRows));
        targetAddress.copyRange(sourceAddress, length);

        delete[] rows;
        rows = newRows;
    }
}

void FileBuffer::makeSpace(uint16_t rowIndex) {
    ensureAdjacentToBuffer(rowIndex);

    // The length should never be 0

    ensureCapacity();
    // Use signed integer to allow decrementing after 0
    for (int32_t pos = length - 1; pos >= rowIndex; --pos) {
        rows[pos + 1] = rows[pos];
    }
    rows[rowIndex] = nullptr;
    length++;
}

void FileBuffer::removeSpace(uint16_t rowIndex) {
    ensureInBuffer(rowIndex);

    delete rows[rowIndex];
    for (uint16_t pos = rowIndex; pos < length; ++pos) {
        rows[pos] = rows[pos + 1];
    }
    length--;
}
