//
// Created by christoph on 05.03.23.
//

#include <cstdint>

#include "Edit.h"
#include "lib/util/base/ArgumentParser.h"
#include "lib/util/base/System.h"
#include "lib/util/io/file/File.h"

int32_t main(int32_t argc, char *argv[]) {
    auto argumentParser = Util::ArgumentParser();
    argumentParser.setHelpText("Edit a text file.\n"
                               "Usage: edit [FILE]...\n"
                               "Options:\n"
                               "  -h, --help: Show this help message");

    if (!argumentParser.parse(argc, argv)) {
        Util::System::error << argumentParser.getErrorString() << Util::Io::PrintStream::endl << Util::Io::PrintStream::flush;
        return -1;
    }

    auto arguments = argumentParser.getUnnamedArguments();
    if (arguments.length() == 0) {
        Util::System::error << "edit: No arguments provided!" << Util::Io::PrintStream::endl << Util::Io::PrintStream::flush;
        return -1;
    }
    if (arguments.length() > 1) {
        Util::System::error << "edit: Expects exactly one argument!" << Util::Io::PrintStream::endl << Util::Io::PrintStream::flush;
        return -1;
    }

    // TODO: Only create file on saving
    const auto &path = arguments[0];
    auto file = Util::Io::File(path);
    if (!file.exists() && !file.create(Util::Io::File::REGULAR)) {
        Util::System::error << "edit: Failed to create file '" << path << "'!" << Util::Io::PrintStream::endl << Util::Io::PrintStream::flush;
        return -1;
    }

    auto lfbFile = Util::Io::File("/device/lfb");
    auto lfb = Util::Graphic::LinearFrameBuffer(lfbFile);
    Edit edit = Edit(path, lfb);
    edit.run();

    return 0;
}
