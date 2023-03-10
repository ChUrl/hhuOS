//
// Created by christoph on 05.03.23.
//

#ifndef HHUOS_EDITBUFFER_H
#define HHUOS_EDITBUFFER_H

#include <cstdint>
#include "lib/util/graphic/Ansi.h"
#include "FileBuffer.h"

/**
 * @brief This class represents a "buffer" in the context of a text editor, not C++.
 *
 * It is responsible for managing text input depending on the text cursor.
 */
class EditBuffer {
    friend class EditBufferView;

public:
    EditBuffer() = delete;

    explicit EditBuffer(const Util::String& path);

    ~EditBuffer();

    // Manipulate text

    void insertCharacterAtCursor(char character);

    void deleteCharacterAtCursor();

    void deleteCharacterBeforeCursor();

    void insertRowAtCursor();

    void insertRowAfterCursor();

    void insertRowBeforeCursor();

    void deleteRowAtCursor();

    // Manipulate cursor

    void moveCursorUp(uint16_t repeat = 1);

    void moveCursorDown(uint16_t repeat = 1);

    void moveCursorLeft(uint16_t repeat = 1);

    void moveCursorRight(uint16_t repeat = 1);

    [[nodiscard]] Util::Graphic::Ansi::CursorPosition getFileCursor() const;

    // Editor I/O

    void loadFromFile();

    void saveToFile();

    // Helper

    [[nodiscard]] bool requiresRedraw() const;

    void drew();

    void bufferModified();

    /**
     * @brief Determine a valid cursor position for a line while moving the cursor as little as possible.
     *
     * If the line is long enough, keep the horizontal cursor position, otherwise set the cursor to the
     * end of the line.
     */
    [[nodiscard]] Util::Graphic::Ansi::CursorPosition getValidCursor(uint16_t rowIndex) const;

private:
    [[nodiscard]] Util::Graphic::Ansi::CursorPosition cursorUp(uint16_t repeat = 1);

    [[nodiscard]] Util::Graphic::Ansi::CursorPosition cursorDown(uint16_t repeat = 1);

    [[nodiscard]] Util::Graphic::Ansi::CursorPosition cursorLeft(uint16_t repeat = 1);

    [[nodiscard]] Util::Graphic::Ansi::CursorPosition cursorRight(uint16_t repeat = 1);

    [[nodiscard]] Util::Graphic::Ansi::CursorPosition cursorToLineStart();

    [[nodiscard]] Util::Graphic::Ansi::CursorPosition cursorToLineEnd();

    [[nodiscard]] Util::Graphic::Ansi::CursorPosition cursorToFileStart();

    [[nodiscard]] Util::Graphic::Ansi::CursorPosition cursorToFileEnd();

    Util::String path;
    FileBuffer *fileBuffer = nullptr;
    bool modified = false; ///< @brief Indicates if the buffer has been modified since the last save.
    bool redraw = true; ///< @brief Indicates if the buffer has been modified since the last draw.
    Util::Graphic::Ansi::CursorPosition fileCursor = {0, 0}; // This is the file-cursor
};

#endif //HHUOS_EDITBUFFER_H
