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
            reprint = file.cursorUp();
            break;
        case Util::Graphic::Ansi::KEY_DOWN:
            reprint = file.cursorDown();
            break;
        case Util::Graphic::Ansi::KEY_LEFT:
            reprint = file.cursorLeft();
            break;
        case Util::Graphic::Ansi::KEY_RIGHT:
            reprint = file.cursorRight();
            break;
        case 'S':
            file.save();
            break;
        case 'Q':
            running = false;
            break;
        case 'U':
            reprint = undoEvent();
            break;
        case 'R':
            reprint = redoEvent();
            break;
        case 0x08:
            // Backspace
            reprint = saveEvent(file.deleteBeforeCursor());
            break;
        default:
            // Write text
            reprint = saveEvent(file.insertAtCursor(static_cast<char>(input)));
    }

    // Need to be in canonical mode for printing
    Util::Graphic::Ansi::enableCanonicalMode();
}

auto Edit::saveEvent(EditEvent *event) -> bool {
    if (event == nullptr) {
        return false;
    }

    events.add(++lastAppliedEvent, event);
    lastEvent = lastAppliedEvent;
    return true;
}

auto Edit::undoEvent() -> bool {
    if (lastAppliedEvent == -1) {
        return false;
    }

    events.get(lastAppliedEvent--)->revert(file);
    return true;
}

auto Edit::redoEvent() -> bool {
    if (lastAppliedEvent == lastEvent) {
        return false;
    }

    events.get(++lastAppliedEvent)->apply(file);
    return true;
}

void Edit::updateView() {
    if (reprint) {
        Util::Graphic::Ansi::clearScreen();
        Util::Graphic::Ansi::setPosition({0, 0});

        const auto [begin, end] = file.getView();
        for (auto it = begin; it != end; ++it) {
            Util::System::out << *it;
        }

        Util::System::out << Util::Io::PrintStream::flush;
        reprint = false;
    }

    Util::Graphic::Ansi::setPosition(file.getViewCursor());
}
