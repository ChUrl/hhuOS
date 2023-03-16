//
// Created by christoph on 06.03.23.
//

#ifndef HHUOS_EDITBUFFERVIEW_H
#define HHUOS_EDITBUFFERVIEW_H

#include "EditBuffer.h"
#include "FileBuffer.h"

/**
 * @brief This struct represents the file viewed through the "terminal window".
 */
class EditBufferView {
public:
    EditBufferView() = delete;

    explicit EditBufferView(const EditBuffer &buffer);

    void moveViewUp(uint16_t repeat = 1);

    void moveViewDown(uint16_t repeat = 1);

    void moveViewLeft(uint16_t repeat = 1);

    void moveViewRight(uint16_t repeat = 1);

    void fixView();

    /**
     * @brief Determine the screen cursor depending on the file cursor and the window position.
     */
    [[nodiscard]] Util::Graphic::Ansi::CursorPosition getScreenCursor() const;

    [[nodiscard]] bool requiresRedraw() const;

    void drew();

    [[nodiscard]] Util::Graphic::Ansi::CursorPosition dimensions() const;

    void getWindow(Util::Array<Util::String> &window) const;

private:
    void viewModified();

    const FileBuffer *fileBuffer;
    const Util::Graphic::Ansi::CursorPosition *fileCursor;

    bool redraw = true;

    Util::Graphic::Ansi::CursorPosition position = {0, 0}; // This is the top-left view coordinate
    Util::Graphic::Ansi::CursorPosition size = {0, 0}; // This is the view's width and height
};

#endif //HHUOS_EDITBUFFERVIEW_H
