//
// Created by christoph on 16.03.23.
//

#include "EditEvent.h"

EditEvent::EditEvent(uint32_t cursor) : cursor(cursor), creation(Util::Time::getSystemTime()) {}

// void EditEvent::apply(FileBuffer &fileBuffer) {
//     if (!valid(fileBuffer)) {
//         Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Can't apply invalid EditEvent!");
//     }
//
//     applyAction(fileBuffer);
// }