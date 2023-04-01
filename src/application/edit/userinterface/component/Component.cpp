//
// Created by christoph on 01.04.23.
//

#include "Component.h"

// ColorDepth is in bits per pixel: RGB24 => 24 bits per pixel (3 Bytes: R, G, B)
// Pitch is in bytes: Buffer width * 3 (RGB24)
Component::Component(uint16_t resolutionX, uint16_t resolutionY)
        : Util::Graphic::LinearFrameBuffer(new uint8_t[resolutionX * resolutionY * 3], resolutionX, resolutionY, 3 * 8, resolutionX * 3),
          pixelDrawer(*this), lineDrawer(pixelDrawer), stringDrawer(pixelDrawer), bufferScroller(*this) {}

Component::~Component() {
    Util::Graphic::LinearFrameBuffer::~LinearFrameBuffer();
}
