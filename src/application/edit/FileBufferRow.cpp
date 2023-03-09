//
// Created by christoph on 06.03.23.
//

#include "FileBufferRow.h"
#include "lib/util/base/Address.h"

FileBufferRow::FileBufferRow() : capacity(INITIAL_COLS), columns(new char[capacity]) {
    for (uint16_t i = 0; i < capacity; ++i) {
        columns[i] = '\0';
    }
}

// TODO: Check string for \n
FileBufferRow::FileBufferRow(const Util::String &row) : FileBufferRow() {
    ensureCapacity(row.length());

    auto sourceAddress = Util::Address(reinterpret_cast<uint32_t>(static_cast<const char *>(row)));
    auto targetAddress = Util::Address(reinterpret_cast<uint32_t>(columns));
    targetAddress.copyRange(sourceAddress, row.length());

    length = row.length();
}

FileBufferRow::~FileBufferRow() {
    delete[] columns;
}

void FileBufferRow::insertCharacter(uint16_t colIndex, char character) {
    if (colIndex > length) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Column out of bounds!");
    }

    makeSpace(colIndex); // Increases length and ensures capacity
    columns[colIndex] = character;
}

void FileBufferRow::appendCharacter(char character) {
    insertCharacter(length, character);
}

// TODO: Check string for \n
void FileBufferRow::insertString(uint16_t colIndex, const Util::String &string) {
    if (colIndex > length) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Column out of bounds!");
    }

    makeSpace(colIndex, string.length());
    auto source = Util::Address(reinterpret_cast<uint32_t>(static_cast<const char*>(string)));
    auto target = Util::Address(reinterpret_cast<uint32_t>(&columns[colIndex]));
    target.copyRange(source, string.length());
}

void FileBufferRow::appendString(const Util::String &string) {
    insertString(length, string);
}

void FileBufferRow::deleteCharacter(uint16_t colIndex) {
    if (colIndex >= length) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Column out of bounds!");
    }

    removeSpace(colIndex); // Decreases length
}

uint16_t FileBufferRow::size() const {
    return length;
}

FileBufferRow::operator Util::String() const {
    return {reinterpret_cast<const uint8_t *>(columns), length};
}

void FileBufferRow::ensureCapacity(uint16_t insertSize) {
    if (capacity <= length + insertSize) {
        // Reallocate on larger size requirement
        while (capacity <= length + insertSize) {
            capacity *= 2;
        }
        auto *newColumns = new char[capacity];
        for (uint16_t i = 0; i < capacity; ++i) {
            newColumns[i] = '\0';
        }

        // Copy contents from the old buffer
        auto sourceAddress = Util::Address(reinterpret_cast<uint32_t>(columns));
        auto targetAddress = Util::Address(reinterpret_cast<uint32_t>(newColumns));
        targetAddress.copyRange(sourceAddress, length);

        delete[] columns;
        columns = newColumns;
    }
}

void FileBufferRow::makeSpace(uint16_t colIndex, uint16_t insertSize) {
    if (colIndex > length) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Column out of bounds!");
    }
    if (insertSize == 0) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Can't make space for zero elements!");
    }

    ensureCapacity(insertSize);
    if (length == 0) {
        // Nothing to do without elements
        length += insertSize;
        return;
    }
    // Use signed integer to allow decrementing after 0
    for (int32_t pos = length - 1; pos >= colIndex; --pos) {
        columns[pos + insertSize] = columns[pos];
    }
    length += insertSize;
}

void FileBufferRow::removeSpace(uint16_t colIndex) {
    if (colIndex >= length) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Column out of bounds!");
    }

    if (length == 0) {
        return;
    }
    for (uint16_t pos = colIndex; pos < length; ++pos) {
        columns[pos] = columns[pos + 1];
    }
    length--;
}
