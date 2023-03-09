//
// Created by christoph on 05.03.23.
//

#include "EditBuffer.h"
#include "lib/interface.h"
#include "lib/util/base/Address.h"

EditBuffer::EditBuffer(const Util::String &path) : path(path), fileBuffer(new FileBuffer) {}

EditBuffer::~EditBuffer() {
    delete fileBuffer;
}

void EditBuffer::insertCharacterAtCursor(char character) {
    fileBuffer->insertCharacter(fileCursor.row, fileCursor.column, character);
    moveCursorRight();
    modified = true;
}

void EditBuffer::deleteCharacterBeforeCursor() {
    if (fileCursor.column == 0 && fileCursor.row != 0) {
        // Merge current with previous line
        const Util::String rest = fileBuffer->rowContent(fileCursor.row);
        const uint16_t length = fileBuffer->rowSize(fileCursor.row - 1);

        fileBuffer->insertString(fileCursor.row - 1, length, rest);
        fileBuffer->deleteRow(fileCursor.row);
        moveCursorUp();
        moveCursorRight(length);
        modified = true;
        return;
    }

    fileBuffer->deleteCharacter(fileCursor.row, fileCursor.column - 1);
    moveCursorLeft();
    modified = true;
}

void EditBuffer::deleteCharacterAtCursor() {
    // TODO
}

void EditBuffer::insertRowBeforeCursor() {
    // TODO
}

void EditBuffer::insertRowAfterCursor() {
    fileBuffer->insertRow(fileCursor.row + 1);
    moveCursorStart();
    moveCursorDown();
    modified = true;
}

void EditBuffer::deleteRowAtCursor() {
    // TODO
}

void EditBuffer::moveCursorUp(uint16_t repeat) {
    for (uint16_t i = 0; i < repeat; ++i) {
        if (fileCursor.row == 0) {
            return;
        }
        fileCursor = getValidCursor(fileCursor.row - 1);
    }
}

void EditBuffer::moveCursorDown(uint16_t repeat) {
    for (uint16_t i = 0; i < repeat; ++i) {
        if (fileBuffer->size() == 0 || fileCursor.row == fileBuffer->size() - 1) {
            return;
        }
        fileCursor = getValidCursor(fileCursor.row + 1);
    }
}

void EditBuffer::moveCursorLeft(uint16_t repeat) {
    for (uint16_t i = 0; i < repeat; ++i) {
        if (fileCursor.column == 0) {
            return;
        }
        fileCursor = {static_cast<uint16_t>(fileCursor.column - 1), fileCursor.row};
    }
}

void EditBuffer::moveCursorRight(uint16_t repeat) {
    for (uint16_t i = 0; i < repeat; ++i) {
        if (fileBuffer->size() == 0 || fileCursor.column == fileBuffer->rowSize(fileCursor.row)) {
            return;
        }
        fileCursor = {static_cast<uint16_t>(fileCursor.column + 1), fileCursor.row};
    }
}

void EditBuffer::moveCursorStart() {
    fileCursor = {0, fileCursor.row};
}

void EditBuffer::moveCursorEnd() {
    if (fileBuffer->size() == 0) {
        return;
    }
    fileCursor = {static_cast<uint16_t>(fileBuffer->rowSize(fileCursor.row)), fileCursor.row};
}

void EditBuffer::moveCursorTop() {
    if (fileBuffer->size() == 0) {
        return;
    }
    fileCursor = getValidCursor(0);
}

void EditBuffer::moveCursorBottom() {
    if (fileBuffer->size() == 0) {
        return;
    }
    fileCursor = getValidCursor(fileBuffer->size() - 1);
}

Util::Graphic::Ansi::CursorPosition EditBuffer::getFileCursor() const {
    return fileCursor;
}

void EditBuffer::loadFromFile() {
    int32_t fileDescriptor = openFile(path);
    uint32_t fileLength = getFileLength(fileDescriptor);
    auto *fileContentsBuffer = new uint8_t[fileLength];
    readFile(fileDescriptor, fileContentsBuffer, 0, fileLength);

    auto fileContents = Util::String(fileContentsBuffer, fileLength);
    delete[] fileContentsBuffer;
    // TODO: This doesn't read multiple contiguous newlines
    for (const auto &line : fileContents.split("\n")) {
        fileBuffer->appendRow(line);
    }

    closeFile(fileDescriptor);
}

void EditBuffer::saveToFile() {
    if (modified) {
        // TODO: Find a better way for this (overwrite the whole file in case it gets shorter...)
        deleteFile(path);
        createFile(path, Util::Io::File::REGULAR);

        // Append newline
        auto fileContents = static_cast<const Util::String>(*fileBuffer);
        if (!fileContents.endsWith('\n')) {
            fileContents += '\n';
        }

        int32_t fileDescriptor = openFile(path);
        writeFile(fileDescriptor, static_cast<const uint8_t *>(fileContents), 0, fileContents.length());
        closeFile(fileDescriptor);

        modified = false;
    }
}

Util::Graphic::Ansi::CursorPosition EditBuffer::getValidCursor(uint16_t rowIndex) const {
    if (rowIndex >= fileBuffer->size()) {
        // Cursor can only move inside existing rows
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Row out of bounds!");
    }

    uint16_t column = fileCursor.column <= fileBuffer->rowSize(rowIndex)
                      ? fileCursor.column
                      : fileBuffer->rowSize(rowIndex);

    return {column, rowIndex};
}
