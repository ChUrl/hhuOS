//
// Created by christoph on 17.03.23.
//

#ifndef HHUOS_CURSORBUFFER_H
#define HHUOS_CURSORBUFFER_H

#include "FileBuffer.h"
#include "lib/util/graphic/Ansi.h"

class EditEvent;

class CursorBuffer : public FileBuffer {
public:
    CursorBuffer() = delete;

    explicit CursorBuffer(const Util::String &path);

    ~CursorBuffer() = default;

    [[nodiscard]] auto cursorUp() -> bool;

    [[nodiscard]] auto cursorDown() -> bool;

    [[nodiscard]] auto cursorLeft() -> bool;

    [[nodiscard]] auto cursorRight() -> bool;

    [[nodiscard]] auto insertAtCursor(char character) -> EditEvent *;

    [[nodiscard]] auto deleteBeforeCursor() -> EditEvent *;

    [[nodiscard]] auto getViewIterators() const -> Util::Pair<Util::Iterator<char>, Util::Iterator<char>>;

    /**
     * @brief Determine the two-dimensional representation of the current cursor.
     */
    [[nodiscard]] auto getRelativeViewCursor() const -> Util::Graphic::Ansi::CursorPosition;

    [[nodiscard]] auto alignViewToCursor() -> bool;

private:
    uint32_t cursor = 0;

    uint32_t viewAnchor = 0; // TODO: Horizontal scrolling
    uint32_t viewSize = Util::Graphic::Ansi::getCursorLimits().row + 1;
};

#endif //HHUOS_CURSORBUFFER_H
