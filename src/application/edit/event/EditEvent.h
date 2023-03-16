//
// Created by christoph on 16.03.23.
//

#ifndef HHUOS_EDITEVENT_H
#define HHUOS_EDITEVENT_H

#include "lib/util/graphic/Ansi.h"
#include "lib/util/time/Timestamp.h"
#include "application/edit/buffer/FileBuffer.h"

class EditEvent {
public:
    explicit EditEvent(Util::Graphic::Ansi::CursorPosition cursor);

    EditEvent(const EditEvent &copy) = delete;

    EditEvent &operator=(const EditEvent &copy) = delete;

    EditEvent(EditEvent &&move) = delete;

    EditEvent &operator=(EditEvent &&move) = delete;

    ~EditEvent() = default;

    void apply(FileBuffer &fileBuffer);

    virtual void applyAction(FileBuffer &fileBuffer) = 0;

    [[nodiscard]] virtual bool valid(const FileBuffer &fileBuffer) const = 0;

    virtual void revert(FileBuffer &fileBuffer) = 0;

private:
    Util::Graphic::Ansi::CursorPosition eventSourcePosition;
    Util::Time::Timestamp eventCreationTime;
};

#endif //HHUOS_EDITEVENT_H
