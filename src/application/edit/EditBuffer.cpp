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
    if (fileBuffer->size() == 0) {
        return;
    }

    modified = true;
    if (fileCursor.column == 0 && fileCursor.row != 0) {
        // Merge current with previous line
        const Util::String rest = fileBuffer->rowContent(fileCursor.row);
        const uint16_t length = fileBuffer->rowSize(fileCursor.row - 1);

        if (!rest.isEmpty()) {
            fileBuffer->insertString(fileCursor.row - 1, length, rest);
        }
        fileBuffer->deleteRow(fileCursor.row);
        moveCursorUp();
        moveCursorRight(length);
    } else if (fileCursor.column != 0) {
        fileBuffer->deleteCharacter(fileCursor.row, fileCursor.column - 1);
        moveCursorLeft();
    }
}

void EditBuffer::deleteCharacterAtCursor() {
    if (fileBuffer->size() == 0) {
        return;
    }

    modified = true;
    if (fileCursor.column == fileBuffer->rowSize(fileCursor.row) && fileCursor.row != fileBuffer->size() - 1) {
        // Merge next with current line
        const Util::String rest = fileBuffer->rowContent(fileCursor.row + 1);
        const uint16_t length = fileBuffer->rowSize(fileCursor.row);

        if (!rest.isEmpty()) {
            fileBuffer->insertString(fileCursor.row, length, rest);
        }
        fileBuffer->deleteRow(fileCursor.row + 1);
    } else if (fileCursor.column != fileBuffer->rowSize(fileCursor.row)) {
        fileBuffer->deleteCharacter(fileCursor.row, fileCursor.column);
    }
}

// TODO: Split line (just newline when at end)
void EditBuffer::insertRowAtCursor() {
    modified = true;
    if (fileBuffer->size() == 0 || fileCursor.column == fileBuffer->rowSize(fileCursor.row)) {
        // Create empty newline
        fileBuffer->insertRow(fileCursor.row + 1);
    } else {
        // Split line
        const Util::String row = static_cast<Util::String>(fileBuffer->rowContent(fileCursor.row));
        fileBuffer->deleteRow(fileCursor.row);
        fileBuffer->insertRow(fileCursor.row, row.substring(fileCursor.column)); // New line
        fileBuffer->insertRow(fileCursor.row, row.substring(0, fileCursor.column)); // Old line
        moveCursorStart();
    }
    moveCursorDown();
}

void EditBuffer::insertRowBeforeCursor() {
    moveCursorStart();
    fileBuffer->insertRow(fileCursor.row);
    modified = true;
}

void EditBuffer::insertRowAfterCursor() {
    fileBuffer->insertRow(fileCursor.row + 1);
    moveCursorStart();
    moveCursorDown();
    modified = true;
}

void EditBuffer::deleteRowAtCursor() {
    if (fileBuffer->size() == 0) {
        return;
    }

    Util::Graphic::Ansi::CursorPosition newCursor = {0, static_cast<uint16_t>(fileCursor.row - 1)};
    if (fileCursor.row != fileBuffer->size() - 1) {
        // Cursor at the end of the file
        newCursor = {getValidCursor(fileCursor.row + 1).column, fileCursor.row};
    } else if (fileCursor.row == 0) {
        // Cursor at the beginning of the file
        newCursor = {0, 0};
    }
    fileBuffer->deleteRow(fileCursor.row);
    fileCursor = newCursor;
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
    fileBuffer->appendRow(); // Have at least one row

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
