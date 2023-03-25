//
// Created by christoph on 17.03.23.
//

#ifndef HHUOS_CURSORBUFFER_H
#define HHUOS_CURSORBUFFER_H

#include "FileBuffer.h"
#include "lib/util/graphic/Ansi.h"

class CursorBuffer : public FileBuffer {
public:
    CursorBuffer() = delete;

    explicit CursorBuffer(const Util::String &path);

    ~CursorBuffer() = default;

    void cursorUp();

    void cursorDown();

    void cursorLeft();

    void cursorRight();

    void insertAtCursor(char character);

    void deleteBeforeCursor();

    /**
     * @brief Determine the two-dimensional representation of the current cursor.
     */
    [[nodiscard]] auto getScreenCursor() const -> Util::Graphic::Ansi::CursorPosition;

private:
    uint32_t cursor = 0;
};

#endif //HHUOS_CURSORBUFFER_H
