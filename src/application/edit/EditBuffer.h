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

    void moveCursorStart();

    void moveCursorEnd();

    void moveCursorTop();

    void moveCursorBottom();

    [[nodiscard]] Util::Graphic::Ansi::CursorPosition getFileCursor() const;

    // Editor I/O

    void loadFromFile();

    void saveToFile();

    // Helper

    /**
     * @brief Determine a valid cursor position for a line while moving the cursor as little as possible.
     *
     * If the line is long enough, keep the horizontal cursor position, otherwise set the cursor to the
     * end of the line.
     */
    [[nodiscard]] Util::Graphic::Ansi::CursorPosition getValidCursor(uint16_t rowIndex) const;

private:
    Util::String path;
    FileBuffer *fileBuffer = nullptr;
    bool modified = false;
    Util::Graphic::Ansi::CursorPosition fileCursor = {0, 0}; // This is the file-cursor
};

#endif //HHUOS_EDITBUFFER_H
