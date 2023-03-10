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
//! I don't allow the file to be empty, so this doesn't have to be checked!
//! The cursor column may be equal to the FileBufferRow length (the cursor can be after the last character)
//! The cursor row may be equal to the FileBuffer length (the cursor can be after the last line)
// TODO: I need to refactor this EOF line shit: Why should the cursor be able to leave the lines at all?

// No special cases
void EditBuffer::insertCharacterAtCursor(char character) {
    bufferModified();
    fileBuffer->insertCharacter(fileCursor, character);
    fileCursor = cursorRight();
}

// Special cases:
//! The cursor is at column 0:
//  Previous linebreak has to be removed, except if there is no line before
//! The cursor is in the EOF row:
//  Just move the cursor to the end of the previous line if it exists
void EditBuffer::deleteCharacterBeforeCursor() {
    if (fileBuffer->isEof(fileCursor)) {
        // Note: As soon as the last row contains characters, it is no longer the EOF.
        //       The EOF is always an empty line after all other lines.
        //       This also means that the first row can never be the EOF.
        fileCursor = cursorUp();
        fileCursor = cursorToLineEnd();
        return;
    }

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
//! The cursor is at the end of the line:
//  The next linebreak has to be removed, except if there is no line after
//! The cursor is in the EOF row:
//  Do nothing
void EditBuffer::deleteCharacterAtCursor() {
    if (fileBuffer->isEof(fileCursor)) {
        return;
    }

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
//! Cursor is at the line start:
//  A line is inserted before the current line
//! Cursor is in the EOF row:
//  A line is inserted before the current line
//! Cursor is at the line end:
//  A line is inserted after the current line
//! Cursor is in the middle of the line:
//  The line is split, the part after the cursor is inserted after the current line.
//  If the cursor is in the middle of the first line, this line can't be removed, so insert line first.
void EditBuffer::insertRowAtCursor() {
    bufferModified();
    if (fileCursor.column == 0 || fileBuffer->isEof(fileCursor)) {
        fileBuffer->insertRow(fileCursor);
    } else if (fileCursor.column == fileBuffer->rowSize(fileCursor)) {
        // Create empty newline
        fileBuffer->insertRow(cursorDown());
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

// Special cases:
//! The cursor is in the EOF row:
//  Do nothing
void EditBuffer::insertRowAfterCursor() {
    if (fileBuffer->isEof(fileCursor)) {
        return;
    }

    bufferModified();
    fileCursor = cursorDown();
    insertRowBeforeCursor();
}

// TODO: Test this
// Special cases:
//! The cursor is in the last line
//  The cursor is moved up after deletion, column should stay if possible
//! The cursor is anywhere else
//  The cursor doesn't move vertically after deletion, column should stay if possible
//! The cursor is in the EOF row
//  Do nothing
void EditBuffer::deleteRowAtCursor() {
    if (fileBuffer->isEof(fileCursor)) {
        return;
    }

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

// TODO: NullPointerException when loading a file
void EditBuffer::loadFromFile() {
    int32_t fileDescriptor = openFile(path);
    uint32_t fileLength = getFileLength(fileDescriptor);

    // TODO: Loading the "short" file does not work
    //       Print using the correct window and check if that works
    // TODO: I don't know how to do this efficiently
    //       - At least load the file incrementally
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

        const auto fileContents = static_cast<Util::String>(*fileBuffer);

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

void EditBuffer::bufferModified() {
    modified = true;
    redraw = true;
}

// TODO: This function should take a full CursorPosition as argument
// Special cases:
//! The cursor is in the EOF row:
//  Return the cursor at the start of the line
//! The cursor is behind the line end:
//  Return the cursor at the end of the line
Util::Graphic::Ansi::CursorPosition EditBuffer::getValidCursor(uint16_t rowIndex) const {
    if (rowIndex > fileBuffer->size()) {
        // Cursor can only move inside existing rows and the extra EOF row
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Row out of bounds!");
    }

    Util::Graphic::Ansi::CursorPosition newCursor = {fileCursor.column, rowIndex};
    if (fileBuffer->isEof(newCursor)) {
        return {0, newCursor.row};
    } else if (newCursor.column > fileBuffer->rowSize(newCursor)) {
        // Cursor is outside the line
        return {fileBuffer->rowSize(newCursor), newCursor.row};
    }
    return newCursor;
}

//! Private functions =============================================================================

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
        if (fileBuffer->isEof(newCursor)) {
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

// Special cases:
//! The cursor is in the EOF row:
//  Cursor can't move further right
Util::Graphic::Ansi::CursorPosition EditBuffer::cursorRight(uint16_t repeat) {
    Util::Graphic::Ansi::CursorPosition newCursor = fileCursor;
    for (uint16_t i = 0; i < repeat; ++i) {
        if (fileBuffer->isEof(newCursor) || fileBuffer->isLastColumn(newCursor)) {
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

// Special cases:
//! The cursor is in the EOF row:
//  Cursor can't move further right
Util::Graphic::Ansi::CursorPosition EditBuffer::cursorToLineEnd() {
    if (fileBuffer->isEof(fileCursor)) {
        return {0, fileCursor.row};
    }
    return {static_cast<uint16_t>(fileBuffer->rowSize(fileCursor)), fileCursor.row};
}

Util::Graphic::Ansi::CursorPosition EditBuffer::cursorToFileStart() {
    return getValidCursor(0);
}

Util::Graphic::Ansi::CursorPosition EditBuffer::cursorToFileEnd() {
    return getValidCursor(fileBuffer->size());
}
