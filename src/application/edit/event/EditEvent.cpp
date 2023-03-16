//
// Created by christoph on 16.03.23.
//

#include "EditEvent.h"

EditEvent::EditEvent(Util::Graphic::Ansi::CursorPosition cursor)
        : eventSourcePosition(cursor), eventCreationTime(Util::Time::getSystemTime()) {}

void EditEvent::apply(FileBuffer &fileBuffer) {
    if (!valid(fileBuffer)) {
        // TODO: Make this less destructive
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Can't apply invalid EditEvent!");
    }

    applyAction(fileBuffer);
}