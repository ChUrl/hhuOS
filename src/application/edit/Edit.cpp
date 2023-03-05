//
// Created by christoph on 05.03.23.
//

#include "Edit.h"
#include "lib/util/base/System.h"
#include "lib/util/io/stream/PrintStream.h"
#include "lib/util/graphic/Ansi.h"
#include "lib/util/base/Address.h"
#include "lib/interface.h"

Edit::Edit(const Util::String &path) : fileDescriptor(Util::Io::File::open(path)),
                                       fileLength(getFileLength(fileDescriptor)) {}

Edit::~Edit() {
    Util::Io::File::close(fileDescriptor);
    delete[] buffer;
}

void Edit::run() {
    // Read initial file to buffer
    loadFile();

    // Initialize display
    Util::Graphic::Ansi::setPosition({0, 0});
    Util::Graphic::Ansi::clearScreen();

    // Main edit loop
    while (isRunning) {
        updateDisplay();
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
    const int16_t input = Util::Graphic::Ansi::readChar();
    switch (input) {
        case Util::Graphic::Ansi::KEY_UP:
            // Close editor
            // TODO: Saved? Message
            if (!isModified) {
                isRunning = false;
            }
            break;
        case Util::Graphic::Ansi::KEY_DOWN:
            // Save file
            saveFile();
            break;
        case 0x08:
            // Backspace
            handleBackspace();
            break;
        default:
            // Write text
            handleTextChar(input);
    }

    // Need to be in canonical mode for printing
    Util::Graphic::Ansi::enableCanonicalMode();
}

void Edit::handleTextChar(int16_t input) {
    ensureBufferCapacity();
    buffer[fileLength++] = static_cast<char>(input);
    isModified = true;
}

void Edit::handleBackspace() {
    if (fileLength == 0) {
        return;
    }

    fileLength--;
    isModified = true;
}

// TODO: Do not refresh the whole screen every time, just print the input
void Edit::updateDisplay() {
    Util::Graphic::Ansi::clearScreen();
    Util::Graphic::Ansi::setPosition({0, 0});

    for (uint32_t pos = 0; pos < fileLength; ++pos) {
        Util::System::out << buffer[pos];
    }
    Util::System::out << Util::Io::PrintStream::flush;
}

void Edit::loadFile() {
    ensureBufferCapacity();
    readFile(fileDescriptor, reinterpret_cast<uint8_t *>(buffer), 0, fileLength);
}

void Edit::saveFile() {
    if (!isModified) {
        return;
    }

    // TODO: Allow configuration
    ensureNewLine();
    trimWhitespace();

    writeFile(fileDescriptor, reinterpret_cast<uint8_t *>(buffer), 0, fileLength);
    isModified = false;
}

void Edit::ensureBufferCapacity(uint32_t insert) {
    if (bufferCapacity > fileLength + insert) {
        return;
    }

    // Double size until it fits
    while (bufferCapacity <= fileLength + insert) {
        bufferCapacity *= 2;
    }
    auto *newBuffer = new char[bufferCapacity];

    auto sourceAddress = Util::Address(reinterpret_cast<uint32_t>(buffer));
    auto targetAddress = Util::Address(reinterpret_cast<uint32_t>(newBuffer));
    targetAddress.copyRange(sourceAddress, fileLength);

    delete[] buffer;
    buffer = newBuffer;
}

void Edit::ensureNewLine() {
    if (fileLength > 1 && buffer[fileLength - 1] != '\n') {
        ensureBufferCapacity();
        buffer[fileLength++] = '\n';
    }
}

void Edit::trimWhitespace() {
    // TODO
}
