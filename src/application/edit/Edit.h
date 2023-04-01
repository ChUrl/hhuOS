//
// Created by christoph on 05.03.23.
//

#ifndef HHUOS_EDIT_H
#define HHUOS_EDIT_H

#include <cstdint>
#include "application/edit/userinterface/Compositor.h"
#include "application/edit/CursorBuffer.h"
#include "lib/util/async/Runnable.h"
#include "lib/util/graphic/LinearFrameBuffer.h"
#include "lib/util/io/file/File.h"

namespace Util {
class String;

template<typename T>
class ArrayList;
}

// TODO: Switch to CursorRunnable
class Edit : public Util::Async::Runnable {
public:
    explicit Edit(const Util::String &path, Util::Graphic::LinearFrameBuffer &lfb);

    ~Edit() override = default;

    /**
     * @brief Override function from Util::Async::Runnable.
     */
    void run() override;

private:
    void handleUserInput();

    void saveEvent(EditEvent *event);

    void undoEvent();

    void redoEvent();

    void updateView();

private:
    CursorBuffer file; // The file open for editing. Only a single file possible.
    Compositor userinterface;

    // TODO: Compress CharEvents to StringEvents for words (on events with ' ')?
    // TODO: Replace with RingBuffer
    Util::ArrayList<EditEvent *> events = Util::ArrayList<EditEvent *>();
    uint32_t lastEvent = -1;
    uint32_t lastAppliedEvent = -1;

    bool resave = true; // Indicates if the file requires saving
    bool reprint = true; // Indicates if the screen contents have changed
    bool running = true;
};

#endif //HHUOS_EDIT_H
