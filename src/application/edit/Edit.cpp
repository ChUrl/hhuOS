//
// Created by christoph on 05.03.23.
//

#include "Edit.h"
#include "application/edit/event/EditEvent.h"
#include "lib/util/base/System.h"

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
            // TODO: Check if save was successfull
            if (resave) {
                file.save();
                resave = false;
            }
            break;
        case 'Q':
            running = false;
            break;
        case 'U':
            undoEvent();
            break;
        case 'R':
            redoEvent();
            break;
        case 0x08:
            // Backspace
            saveEvent(file.deleteBeforeCursor());
            break;
        default:
            // Write text
            saveEvent(file.insertAtCursor(static_cast<char>(input)));
    }

    // Need to be in canonical mode for printing
    Util::Graphic::Ansi::enableCanonicalMode();
}

void Edit::saveEvent(EditEvent *event) {
    if (event == nullptr) {
        return;
    }

    events.add(++lastAppliedEvent, event);
    lastEvent = lastAppliedEvent;
    reprint = true;
    resave = true;
}

void Edit::undoEvent() {
    if (lastAppliedEvent == -1) {
        return;
    }

    events.get(lastAppliedEvent--)->revert(file);
    reprint = true;
    resave = true;
}

void Edit::redoEvent() {
    if (lastAppliedEvent == lastEvent) {
        return;
    }

    events.get(++lastAppliedEvent)->apply(file);
    reprint = true;
    resave = true;
}

void Edit::updateView() {
    // TODO: Only repaint changed line(s)
    if (reprint) {
        Util::Graphic::Ansi::clearScreen();
        Util::Graphic::Ansi::setPosition({0, 0});

        const auto [begin, end] = file.getViewIterators();
        for (auto it = begin; it != end; ++it) {
            Util::System::out << *it;
        }

        Util::System::out << Util::Io::PrintStream::flush;
        reprint = false;
    }

    Util::Graphic::Ansi::setPosition(file.getRelativeViewCursor());
}
