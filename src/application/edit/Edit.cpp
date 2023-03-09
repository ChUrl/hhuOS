//
// Created by christoph on 05.03.23.
//

#include "Edit.h"
#include "lib/util/base/System.h"
#include "lib/util/io/stream/PrintStream.h"
#include "lib/util/base/Address.h"

Edit::Edit(const Util::String &path) : buffer(EditBuffer(path)), view(EditBufferView(buffer)) {}

void Edit::run() {
    buffer.loadFromFile();

    // TODO: Remove this once the cursor is movable
    buffer.moveCursorBottom();
    buffer.moveCursorEnd();

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
    // TODO: ? for help
    // TODO: How do CTRL keybindings work?
    const int16_t input = Util::Graphic::Ansi::readChar();
    switch (input) {
        case Util::Graphic::Ansi::KEY_UP:
            buffer.moveCursorUp();
            break;
        case Util::Graphic::Ansi::KEY_DOWN:
            buffer.moveCursorDown();
            break;
        case Util::Graphic::Ansi::KEY_LEFT:
            buffer.moveCursorLeft();
            break;
        case Util::Graphic::Ansi::KEY_RIGHT:
            buffer.moveCursorRight();
            break;
        case 'S':
            buffer.saveToFile();
            break;
        case 'Q':
            running = false;
            break;
        case 0x08:
            // Backspace
            buffer.deleteCharacterBeforeCursor();
            break;
        default:
            // Write text
            handleTextInput(static_cast<char>(input));
    }

    // Need to be in canonical mode for printing
    Util::Graphic::Ansi::enableCanonicalMode();
}

void Edit::handleTextInput(char input) {
    switch (input) {
        case '\n':
            buffer.insertRowAfterCursor();
            break;
        default:
            buffer.insertCharacterAtCursor(input);
    }
}

// TODO: Do not refresh the whole screen on text input, just print single char?
void Edit::updateView() {
    Util::Graphic::Ansi::clearScreen();
    Util::Graphic::Ansi::setPosition({0, 0});

    Util::System::out << static_cast<const Util::String>(view) << Util::Io::PrintStream::flush;

    Util::Graphic::Ansi::setPosition(view.getScreenCursor());
}
