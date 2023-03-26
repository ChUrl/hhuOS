//
// Created by christoph on 26.03.23.
//

#ifndef HHUOS_INSERTCHAREVENT_H
#define HHUOS_INSERTCHAREVENT_H

#include "EditEvent.h"

class InsertCharEvent : public EditEvent {
public:
    InsertCharEvent(uint32_t cursor, char character);

    void apply(CursorBuffer &cursorBuffer) override;

    void revert(CursorBuffer &cursorBuffer) override;

private:
    char character;
};

#endif //HHUOS_INSERTCHAREVENT_H
