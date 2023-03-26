//
// Created by christoph on 16.03.23.
//

#ifndef HHUOS_EDITEVENT_H
#define HHUOS_EDITEVENT_H

#include "lib/util/graphic/Ansi.h"
#include "lib/util/time/Timestamp.h"

class CursorBuffer;

class EditEvent {
public:
    explicit EditEvent(uint32_t cursor);

    EditEvent(const EditEvent &copy) = delete;

    EditEvent &operator=(const EditEvent &copy) = delete;

    EditEvent(EditEvent &&move) = delete;

    EditEvent &operator=(EditEvent &&move) = delete;

    ~EditEvent() = default;

    virtual void apply(CursorBuffer &cursorBuffer) = 0;

    // virtual void applyAction(FileBuffer &fileBuffer) = 0;

    // [[nodiscard]] virtual bool valid(const FileBuffer &fileBuffer) const = 0;

    virtual void revert(CursorBuffer &cursorBuffer) = 0;

protected:
    uint32_t cursor;
    Util::Time::Timestamp creation;
};

#endif //HHUOS_EDITEVENT_H
