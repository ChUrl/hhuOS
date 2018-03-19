#ifndef __RamFsNode_include__
#define __RamFsNode_include__

#include "../FsNode.h"
#include "VirtualNode.h"
#include <stdint.h>

extern "C" {
#include "lib/libc/string.h"
}

/**
 * An implementation of FsNode for the RamFs
 * (A filesystem, that exists completely in memory).
 */
class RamFsNode : public FsNode {

public:

    RamFsNode(VirtualNode *arg_node) : node(arg_node) {}

    ~RamFsNode() = default;

    String getName();

    uint8_t getFileType();

    uint64_t getLength();

    String getChild(uint32_t pos);

    char *readData(char *buf, uint64_t pos, uint32_t numBytes);

    int32_t writeData(char *buf, uint64_t pos, uint32_t numBytes);

private:

    VirtualNode *node;
};

#endif