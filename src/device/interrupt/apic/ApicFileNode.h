#ifndef HHUOS_APICFILENODE_H
#define HHUOS_APICFILENODE_H

#include <cstdint>

#include "filesystem/memory/MemoryNode.h"
#include "device/interrupt/apic/IoApic.h"
#include "lib/util/collection/Array.h"
#include "lib/util/base/String.h"
#include "lib/util/io/file/File.h"

namespace Filesystem::Memory {

class ApicFileNode : public MemoryNode {

public:
    /**
     * Constructor.
     */
    ApicFileNode(const Util::String &name, void (*updateCallback)(Util::String &string));

    /**
     * Copy Constructor.
     */
    ApicFileNode(const ApicFileNode &copy) = delete;

    /**
     * Assignment operator.
     */
    ApicFileNode & operator=(const ApicFileNode &other) = delete;

    /**
     * Destructor.
     */
    ~ApicFileNode() override = default;

    /**
     * Overriding function from Node.
     */
    Util::Io::File::Type getType() override;

    /**
     * Overriding function from Node.
     */
    uint64_t getLength() override;

    /**
     * Overriding function from Node.
     */
    Util::Array<Util::String> getChildren() override;

    /**
     * Overriding function from Node.
     */
    uint64_t readData(uint8_t *targetBuffer, uint64_t pos, uint64_t numBytes) override;

    /**
     * Overriding function from Node.
     */
    uint64_t writeData(const uint8_t *sourceBuffer, uint64_t pos, uint64_t numBytes) override;

private:

    uint64_t length = 0;
    uint8_t *data = nullptr;
    void (*updateCallback)(Util::String &string);
};

}

#endif
