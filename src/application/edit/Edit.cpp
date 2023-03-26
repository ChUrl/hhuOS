//
// Created by christoph on 05.03.23.
//

#include "Edit.h"
#include "lib/util/base/System.h"
#include "lib/util/io/stream/PrintStream.h"

Edit::Edit(const Util::String &path) : file(CursorBuffer(path)) {}

void Edit::run() {
    // Main edit loop
    while (running) {
        updateView();
        handleUserInput();
    }

    // Cleanup display
    Util::Graphic::Ansi::setPosition({0, 0});
    Util::Graphic::Ansi::clearScreen();
    Util::Graphic::Ansi::cleanupGraphicalApplication();
}

void Edit::handleUserInput() {
    // Canonical mode accepts input after enter, raw mode processes every keypress
    Util::Graphic::Ansi::enableRawMode();

    // Handle user input
    const int16_t input = Util::Graphic::Ansi::readChar();
    switch (input) {
        case Util::Graphic::Ansi::KEY_UP:
            file.cursorUp();
            break;
        case Util::Graphic::Ansi::KEY_DOWN:
            file.cursorDown();
            break;
        case Util::Graphic::Ansi::KEY_LEFT:
            file.cursorLeft();
            break;
        case Util::Graphic::Ansi::KEY_RIGHT:
            file.cursorRight();
            break;
        case 'S':
            file.save();
            break;
        case 'Q':
            running = false;
            break;
        case 0x08:
            // Backspace
            file.deleteBeforeCursor();
            break;
        default:
            // Write text
            file.insertAtCursor(static_cast<char>(input));
    }

    // Need to be in canonical mode for printing
    Util::Graphic::Ansi::enableCanonicalMode();
}

// TODO: Only reprint if necessary (not on cursor change!)
void Edit::updateView() {
    Util::Graphic::Ansi::clearScreen();
    Util::Graphic::Ansi::setPosition({0, 0});


#if ENABLE_EDIT_DEBUG == 1
    Util::System::out << "Line Based: ==============================\n";
    for (uint32_t i = 0; i < file.getNumberOfRows(); ++i) {
        const FileBuffer::Row row = file.getRow(i);
        Util::System::out << Util::String::format("[%u, %u]: ", row.first, row.second);
        const auto [begin, end] = file.getSingleRow(i);
        for (auto it = begin; it != end; ++it) {
            Util::System::out << *it;
        }
    }
    Util::System::out << "Whole File: ==============================\n";
#endif

    const auto [begin, end] = file.getView();
    for (auto it = begin; it != end; ++it) {
        Util::System::out << *it;
    }

    Util::System::out << Util::Io::PrintStream::flush;

#if ENABLE_EDIT_DEBUG == 1
    Util::Graphic::Ansi::setPosition({file.getScreenCursor().column, static_cast<uint16_t>(file.getScreenCursor().row + file.rows.size() + 2)});
#else
    Util::Graphic::Ansi::setPosition(file.getViewCursor());
#endif
}
