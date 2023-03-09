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
    for (const auto &row : string.split("\n")) {
        appendRow(row);
    }
}

FileBuffer::~FileBuffer() {
    delete[] rows;
}

void FileBuffer::insertCharacter(uint16_t rowIndex, uint16_t colIndex, char character) {
    if (rowIndex > length) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Row out of bounds!");
    }

    if (rows[rowIndex] == nullptr) {
        insertRow(rowIndex);
    }

    rows[rowIndex]->insertCharacter(colIndex, character);
}

void FileBuffer::insertString(uint16_t rowIndex, uint16_t colIndex, const Util::String &string) {
    if (rowIndex > length) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Row out of bounds!");
    }

    if (rows[rowIndex] == nullptr) {
        insertRow(rowIndex);
    }

    rows[rowIndex]->insertString(colIndex, string);
}

void FileBuffer::deleteCharacter(uint16_t rowIndex, uint16_t colIndex) {
    if (rowIndex >= length) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Row out of bounds!");
    }

    rows[rowIndex]->deleteCharacter(colIndex);
}

void FileBuffer::insertRow(uint16_t rowIndex, const Util::String &row) {
    if (rowIndex > length) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Row out of bounds!");
    }

    makeSpace(rowIndex); // Increases length and ensures capacity
    rows[rowIndex] = new FileBufferRow(row); // Clear out previous row
}

void FileBuffer::appendRow(const Util::String &row) {
    insertRow(length, row);
}

void FileBuffer::deleteRow(uint16_t rowIndex) {
    if (rowIndex >= length) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Row out of bounds!");
    }

    removeSpace(rowIndex); // Decreases length
}

uint16_t FileBuffer::rowSize(uint16_t rowIndex) const {
    if (rowIndex >= length) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Row out of bounds!");
    }

    return rows[rowIndex]->size();
}

Util::String FileBuffer::rowContent(uint16_t rowIndex) const {
    if (rowIndex >= length) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Row out of bounds!");
    }

    return static_cast<Util::String>(*rows[rowIndex]);
}

uint16_t FileBuffer::size() const {
    return length;
}

FileBuffer::operator Util::String() const {
    auto string = Util::String();
    for (uint16_t row = 0; row < length; ++row) {
        string += static_cast<Util::String>(*rows[row]);
        if (row + 1 < length) {
            // Skip last line
            string += "\n";
        }
    }

    return string;
}

void FileBuffer::ensureCapacity(uint16_t insertSize) {
    if (capacity <= length + insertSize) {
        while (capacity <= length + insertSize) {
            capacity *= 2;
        }
        auto *newRows = new FileBufferRow *[capacity];
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
    if (rowIndex > length) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Row out of bounds!");
    }

    ensureCapacity();
    if (length == 0) {
        // Nothing to do without elements
        length++;
        return;
    }
    // Use signed integer to allow decrementing after 0
    for (int32_t pos = length - 1; pos >= rowIndex; --pos) {
        rows[pos + 1] = rows[pos];
    }
    length++;
}

void FileBuffer::removeSpace(uint16_t rowIndex) {
    if (rowIndex >= length) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Row out of bounds!");
    }

    if (length == 0) {
        return;
    }
    delete rows[rowIndex];
    for (uint16_t pos = rowIndex; pos < length; ++pos) {
        rows[pos] = rows[pos + 1];
    }
    length--;
}
