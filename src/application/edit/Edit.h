//
// Created by christoph on 05.03.23.
//

#ifndef HHUOS_EDIT_H
#define HHUOS_EDIT_H

#include "lib/util/async/Runnable.h"
#include "lib/util/base/String.h"
#include "lib/util/io/file/File.h"
#include "lib/util/graphic/Ansi.h"
#include "EditBuffer.h"
#include "EditBufferView.h"

class Edit : public Util::Async::Runnable {
public:
    explicit Edit(const Util::String &path);

    ~Edit() override = default;

    /**
     * @brief Override function from Util::Async::Runnable.
     */
    void run() override;

private:
    // UI related ===============================
    void handleUserInput();

    void updateView();

private:
    bool running = true;

    EditBuffer buffer;
    EditBufferView view;

    // TODO: Apparently it is not possible to use local statics (atexit), that's why this is defined
    //       here once, awkwardly
    Util::Array<Util::String> printWindow;
};

#endif //HHUOS_EDIT_H
