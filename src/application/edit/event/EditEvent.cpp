//
// Created by christoph on 16.03.23.
//

#include "EditEvent.h"

EditEvent::EditEvent(uint32_t cursor) : cursor(cursor), creation(Util::Time::getSystemTime()) {}