#include "ApicFileNode.h"
#include "lib/util/base/Address.h"

namespace Filesystem::Memory {

ApicFileNode::ApicFileNode(const Util::String &name, void (*updateCallback)(Util::String &string))
  : MemoryNode(name), updateCallback(updateCallback) {}

Util::Io::File::Type ApicFileNode::getType() {
    return Util::Io::File::REGULAR;
}

uint64_t ApicFileNode::getLength() {
    // Update this files contents on read. I am not completely sure if getLength() is called before every
    // possible read, but the FileInputStream calls it.
    Util::String string;
    updateCallback(string);
    writeData(static_cast<const uint8_t *>(string), 0, string.length());

    return length;
}

Util::Array<Util::String> ApicFileNode::getChildren() {
    return Util::Array<Util::String>(0);
}

uint64_t ApicFileNode::readData(uint8_t *targetBuffer, uint64_t pos, uint64_t numBytes) {
    if (pos >= length) {
        return 0;
    }

    if (pos + numBytes > length) {
        numBytes = (length - pos);
    }

    auto sourceAddress = Util::Address<uint32_t>(data).add(pos);
    auto targetAddress = Util::Address<uint32_t>(targetBuffer);
    targetAddress.copyRange(sourceAddress, numBytes);

    return numBytes;
}

uint64_t ApicFileNode::writeData(const uint8_t *sourceBuffer, uint64_t pos, uint64_t numBytes) {
    auto sourceAddress = Util::Address<uint32_t>(sourceBuffer);

    if(pos + numBytes >= length) {
        auto newLength = pos + numBytes;
        auto *newData = new uint8_t[newLength];
        auto oldAddress = Util::Address<uint32_t>(data);
        auto newAddress = Util::Address<uint32_t>(newData);

        newAddress.setRange(0, newLength);
        newAddress.copyRange(oldAddress, length);

        delete data;
        data = newData;
        length = newLength;
    }

    auto targetAddress = Util::Address<uint32_t>(data).add(pos);
    targetAddress.copyRange(sourceAddress, numBytes);

    return numBytes;
}

}