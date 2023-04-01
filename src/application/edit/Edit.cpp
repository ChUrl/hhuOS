//
// Created by christoph on 05.03.23.
//

#include "Edit.h"
#include "application/edit/event/EditEvent.h"
#include "application/edit/userinterface/Compositor.h"
#include "application/edit/userinterface/component/TextView.h"
#include "lib/util/base/System.h"
#include "lib/util/graphic/Fonts.h"

Edit::Edit(const Util::String &path, Util::Graphic::LinearFrameBuffer &lfb)
        : file(CursorBuffer(path)), userinterface(Compositor(lfb)) {
    // Init UI
    Util::Graphic::Ansi::CursorPosition limits = Util::Graphic::Ansi::getCursorLimits();
    Component *view = new TextView(limits.column + 1, limits.row + 1, Util::Graphic::Fonts::TERMINAL_FONT, file);
    userinterface.setRoot(view);
}

void Edit::run() {
    Util::Graphic::Ansi::prepareGraphicalApplication(false);

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

    // TODO: Remove char bindings, how do Ctrl-combinations work?
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
            // TODO: Check if save was successful
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
        // Util::Graphic::Ansi::clearScreen();
        // Util::Graphic::Ansi::setPosition({0, 0});

        // const auto [begin, end] = file.getViewIterators();
        // for (auto it = begin; it != end; ++it) {
        //     Util::System::out << *it;
        // }

        // Util::System::out << Util::Io::PrintStream::flush;

        userinterface.update();
        userinterface.draw();
        reprint = false;
    }

    // Util::Graphic::Ansi::setPosition(file.getRelativeViewCursor());
}
