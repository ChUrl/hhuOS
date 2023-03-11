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

// Some Notes:
// ! I don't allow the file to be empty, so this doesn't have to be checked!
// ! The cursor column may be equal to the FileBufferRow length (the cursor can be after the last character)
// ! The cursor row may not be equal to the FileBuffer length (the cursor cannot be after the last line)

// No special cases
void EditBuffer::insertCharacterAtCursor(char character) {
    bufferModified();
    fileBuffer->insertCharacter(fileCursor, character);
    fileCursor = cursorRight();
}

// Special cases:
// ! The cursor is at column 0:
//   Previous linebreak has to be removed, except if there is no line before
void EditBuffer::deleteCharacterBeforeCursor() {
    bufferModified();
    if (fileCursor.column == 0 && fileCursor.row != 0) {
        // Cursor is in column 0 after the first line: Merge current with the previous line
        const Util::String rest = fileBuffer->rowContent(fileCursor);

        fileBuffer->deleteRow(fileCursor);
        fileCursor = cursorUp();
        fileCursor = cursorToLineEnd();

        if (!rest.isEmpty()) {
            fileBuffer->insertString(fileCursor, rest);
        }
    } else if (fileCursor.column != 0) {
        // Cursor is anywhere except in column 0
        fileCursor = cursorLeft();
        fileBuffer->deleteCharacter(fileCursor);
    }
}

// Special cases:
// ! The cursor is at the end of the line:
//   The next linebreak has to be removed, except if there is no line after
void EditBuffer::deleteCharacterAtCursor() {
    bufferModified();
    if (fileBuffer->isLastColumn(fileCursor) && !fileBuffer->isLastRow(fileCursor)) {
        // Merge next with current line
        const Util::String rest = fileBuffer->rowContent(cursorDown());

        if (!rest.isEmpty()) {
            // Cursor is at the insert position
            fileBuffer->insertString(fileCursor, rest);
        }
        fileBuffer->deleteRow(cursorDown());
    } else if (!fileBuffer->isLastColumn(fileCursor)) {
        fileBuffer->deleteCharacter(fileCursor);
    }
}

// Special cases:
// ! Cursor is at the line start:
//   A line is inserted before the current line
// ! Cursor is at the line end:
//   A line is inserted after the current line
// ! Cursor is in the middle of the line:
//   The line is split, the part after the cursor is inserted after the current line.
//   If the cursor is in the middle of the first line, this line can't be removed, so insert line first.
void EditBuffer::insertRowAtCursor() {
    bufferModified();
    if (fileCursor.column == 0) {
        fileBuffer->insertRow(fileCursor);
    } else if (fileCursor.column == fileBuffer->rowSize(fileCursor)) {
        // Create empty newline
        fileBuffer->insertRow({fileCursor.column, static_cast<uint16_t>(fileCursor.row + 1)});
    } else {
        // Split line
        const Util::String row = static_cast<Util::String>(fileBuffer->rowContent(fileCursor));
        fileBuffer->insertRow(fileCursor, row.substring(fileCursor.column)); // New line
        fileBuffer->insertRow(fileCursor, row.substring(0, fileCursor.column)); // Old line
        fileBuffer->deleteRow(cursorDown(2)); // Remove the old line last
        fileCursor = cursorToLineStart();
    }
    fileCursor = cursorDown();
}

// No special cases
void EditBuffer::insertRowBeforeCursor() {
    bufferModified();
    fileCursor = cursorToLineStart();
    fileBuffer->insertRow(fileCursor);
}

// No special cases
void EditBuffer::insertRowAfterCursor() {
    bufferModified();
    fileBuffer->insertRow({fileCursor.column, static_cast<uint16_t>(fileCursor.row + 1)});
    fileCursor = cursorDown();
}

// TODO: Test this
// Special cases:
// ! The cursor is in the last line
//   The cursor is moved up after deletion, column should stay if possible
// ! The cursor is anywhere else
//   The cursor doesn't move vertically after deletion, column should stay if possible
void EditBuffer::deleteRowAtCursor() {
    bufferModified();
    fileBuffer->deleteRow(fileCursor);
    if (fileCursor.row == fileBuffer->size()) {
        // Cursor currently not in a line
        fileCursor = cursorUp();
    } else {
        // Line length has changed
        fileCursor = getValidCursor(fileCursor.row);
    }
}

void EditBuffer::moveCursorUp(uint16_t repeat) {
    fileCursor = cursorUp(repeat);
}

void EditBuffer::moveCursorDown(uint16_t repeat) {
    fileCursor = cursorDown(repeat);
}

void EditBuffer::moveCursorLeft(uint16_t repeat) {
    fileCursor = cursorLeft(repeat);
}

void EditBuffer::moveCursorRight(uint16_t repeat) {
    fileCursor = cursorRight(repeat);
}

Util::Graphic::Ansi::CursorPosition EditBuffer::getFileCursor() const {
    return fileCursor;
}

void EditBuffer::loadFromFile() {
    int32_t fileDescriptor = openFile(path);
    uint32_t fileLength = getFileLength(fileDescriptor);

    // TODO: I don't know how to do this efficiently. Could load incrementally?
    if (fileLength == 0) {
        // Always start with at least a single line to remove the need
        // to handle the fileBuffer->size() == 0 case
        fileBuffer->appendRow();
    } else {
        auto *fileContents = new uint8_t[fileLength];
        if (readFile(fileDescriptor, fileContents, 0, fileLength) != fileLength) {
            Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Failed to fully load file!");
        }

        Util::String line = "";
        for (uint32_t i = 0; i < fileLength; ++i) {
            // TODO: Find the next \n and determine the complete line directly,
            //       to remove all the small Util::String &operator+=(...) calls
            const char character = static_cast<char>(fileContents[i]);
            if (character != '\n') {
                line += character;
            } else {
                fileBuffer->appendRow(line);
                line = "";
            }
        }

        delete[] fileContents;
    }

    closeFile(fileDescriptor);
}

void EditBuffer::saveToFile() {
    if (modified) {
        // TODO: Find a better way for this (overwrite the whole file in case it gets shorter...)
        if (!deleteFile(path)) {
            Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Failed to recreate file for saving!");
        }
        if (!createFile(path, Util::Io::File::REGULAR)) {
            Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Failed to recreate file for saving!");
        }

        Util::Array<Util::String> lines(fileBuffer->size());
        fileBuffer->getRows(lines);
        const auto fileContents = Util::String::join("\n", lines);

        int32_t fileDescriptor = openFile(path);
        writeFile(fileDescriptor, static_cast<const uint8_t *>(fileContents), 0, fileContents.length());
        closeFile(fileDescriptor);

        modified = false;
    }
}

bool EditBuffer::requiresRedraw() const {
    return redraw;
}

void EditBuffer::drew() {
    redraw = false;
}

// TODO: This function should take a full CursorPosition as argument
// Special cases:
// ! The cursor is behind the line end:
//   Return the cursor at the end of the line
Util::Graphic::Ansi::CursorPosition EditBuffer::getValidCursor(uint16_t rowIndex) const {
    if (rowIndex > fileBuffer->size()) {
        // Cursor can only move inside existing rows and the extra EOF row
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Row out of bounds!");
    }

    Util::Graphic::Ansi::CursorPosition newCursor = {fileCursor.column, rowIndex};
    if (newCursor.column > fileBuffer->rowSize(newCursor)) {
        // Cursor is outside the line
        return {fileBuffer->rowSize(newCursor), newCursor.row};
    }
    return newCursor;
}

// ! Private functions ============================================================================

void EditBuffer::bufferModified() {
    modified = true;
    redraw = true;
}

Util::Graphic::Ansi::CursorPosition EditBuffer::cursorUp(uint16_t repeat) {
    Util::Graphic::Ansi::CursorPosition newCursor = fileCursor;
    for (uint16_t i = 0; i < repeat; ++i) {
        if (newCursor.row == 0) {
            // Can't move further up
            return newCursor;
        }
        newCursor = getValidCursor(newCursor.row - 1);
    }
    return newCursor;
}

Util::Graphic::Ansi::CursorPosition EditBuffer::cursorDown(uint16_t repeat) {
    Util::Graphic::Ansi::CursorPosition newCursor = fileCursor;
    for (uint16_t i = 0; i < repeat; ++i) {
        if (fileBuffer->isLastRow(newCursor)) {
            // Can't move further down
            return newCursor;
        }
        newCursor = getValidCursor(newCursor.row + 1);
    }
    return newCursor;
}

Util::Graphic::Ansi::CursorPosition EditBuffer::cursorLeft(uint16_t repeat) {
    Util::Graphic::Ansi::CursorPosition newCursor = fileCursor;
    for (uint16_t i = 0; i < repeat; ++i) {
        if (newCursor.column == 0) {
            // Can't move further left
            return newCursor;
        }
        newCursor = {static_cast<uint16_t>(newCursor.column - 1), newCursor.row};
    }
    return newCursor;
}

Util::Graphic::Ansi::CursorPosition EditBuffer::cursorRight(uint16_t repeat) {
    Util::Graphic::Ansi::CursorPosition newCursor = fileCursor;
    for (uint16_t i = 0; i < repeat; ++i) {
        if (fileBuffer->isLastColumn(newCursor)) {
            // Can't move further right
            return newCursor;
        }
        newCursor = {static_cast<uint16_t>(newCursor.column + 1), newCursor.row};
    }
    return newCursor;
}

Util::Graphic::Ansi::CursorPosition EditBuffer::cursorToLineStart() {
    return {0, fileCursor.row};
}

Util::Graphic::Ansi::CursorPosition EditBuffer::cursorToLineEnd() {
    return {static_cast<uint16_t>(fileBuffer->rowSize(fileCursor)), fileCursor.row};
}

Util::Graphic::Ansi::CursorPosition EditBuffer::cursorToFileStart() {
    return getValidCursor(0);
}

Util::Graphic::Ansi::CursorPosition EditBuffer::cursorToFileEnd() {
    return getValidCursor(fileBuffer->size());
}
