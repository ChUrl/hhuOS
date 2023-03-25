//
// Created by christoph on 05.03.23.
//

#ifndef HHUOS_EDIT_H
#define HHUOS_EDIT_H

#include "lib/util/async/Runnable.h"
#include "lib/util/base/String.h"
#include "lib/util/io/file/File.h"
#include "lib/util/graphic/Ansi.h"
#include "application/edit/CursorBuffer.h"

class Edit : public Util::Async::Runnable {
public:
    explicit Edit(const Util::String &path);

    ~Edit() override = default;

    /**
     * @brief Override function from Util::Async::Runnable.
     */
    void run() override;

private:
    void handleUserInput();

    void updateView();

private:
    CursorBuffer file; // The file open for editing. Only a single file possible.

    bool running = true;
};

#endif //HHUOS_EDIT_H
