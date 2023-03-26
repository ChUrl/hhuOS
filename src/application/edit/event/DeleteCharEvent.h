//
// Created by christoph on 26.03.23.
//

#ifndef HHUOS_DELETECHAREVENT_H
#define HHUOS_DELETECHAREVENT_H

#include "EditEvent.h"

class DeleteCharEvent : public EditEvent {
public:
    DeleteCharEvent(uint32_t cursor, char character);

    void apply(CursorBuffer &cursorBuffer) override;

    void revert(CursorBuffer &cursorBuffer) override;

private:
    char character;
};

#endif //HHUOS_DELETECHAREVENT_H
