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

    void deleteAtCursor();

    // [[nodiscard]] auto getRowsFromCursor(uint32_t length) const -> Util::Pair<Util::Iterator<char>, Util::Iterator<char>>;

    [[nodiscard]] auto getView() const -> Util::Pair<Util::Iterator<char>, Util::Iterator<char>>;

    /**
     * @brief Determine the two-dimensional representation of the current cursor.
     */
    [[nodiscard]] auto getViewCursor() const -> Util::Graphic::Ansi::CursorPosition;

private:
    void fixView();

private:
    uint32_t cursor = 0;

    uint32_t viewAnchor = 0; // TODO: Horizontal scrolling
    uint32_t viewSize = Util::Graphic::Ansi::getCursorLimits().row + 1;
};

#endif //HHUOS_CURSORBUFFER_H
