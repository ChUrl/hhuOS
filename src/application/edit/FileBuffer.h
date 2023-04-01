//
// Created by christoph on 06.03.23.
//

#ifndef HHUOS_FILEBUFFER_H
#define HHUOS_FILEBUFFER_H

#include <cstdint>
#include "lib/util/collection/ArrayList.h"
#include "lib/util/collection/Pair.h"

namespace Util {
class String;

template<typename T>
class Iterator;
}

#define ENABLE_EDIT_DEBUG 0
#define DEBUG_EXCEPTION(msg) Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, msg)

// TODO: Explicit types for RowIndex/CharIndex
//       Cursor: CharIndex
//       WindowAnchor: RowIndex

class FileBuffer {
#if ENABLE_EDIT_DEBUG == 1
    friend class Edit;
#endif

public:
    FileBuffer() = delete;

    /**
     * @brief Load a file from the disk into the buffer.
     *
     * @param path The path to the file
     */
    explicit FileBuffer(const Util::String &path);

    ~FileBuffer() = default;

    void save();

    void insertString(uint32_t charindex, const Util::String &string);

    void deleteString(uint32_t charindex, uint32_t length);

    [[nodiscard]] auto getNumberOfRows() const -> uint32_t;

    [[nodiscard]] auto getRowIterators(uint32_t rowindex) const -> Util::Pair<Util::Iterator<char>, Util::Iterator<char>>;

    [[nodiscard]] auto getFileIterators() const -> Util::Pair<Util::Iterator<char>, Util::Iterator<char>>;

protected:
    struct Row : public Util::Pair<uint32_t, uint32_t> {
        Row() = default;

        Row(uint32_t begin, uint32_t end);

        // TODO
        [[nodiscard]] auto start() const -> uint32_t;

        [[nodiscard]] auto end() const -> uint32_t;

        [[nodiscard]] auto length() const -> uint32_t;

        [[nodiscard]] auto isEmpty() const -> bool;
    };

protected:
    void prepareRowsNewCharacter(uint32_t charindex);

    void prepareRowsDeleteCharacter(uint32_t charindex);

    void prepareRowsNewLine(uint32_t charindex);

    void prepareRowsDeleteLine(uint32_t charindex);

    [[nodiscard]] auto getRow(uint32_t rowindex) const -> Row;

    /**
     * @brief Determine the index of the row containing the character at index.
     */
    [[nodiscard]] auto getRowByChar(uint32_t charindex) const -> Util::Pair<uint32_t, Row>;

protected:
    Util::String path;

    // This approach is very simple to implement, in comparison to the line-based buffer approach
    // that I used before. Drawback: On each line manipulation, the whole part of the file after
    // the cursor has to be moved in memory. When surpassing file sizes of 250kB, it's unusable.
    // If large files are of concern, it could probably be accelerated significantly by using
    // some kind of tree (rope?). For now, I don't care about large files.
    // I tried replacing the internal ArrayList copy with an SSE/MMX version, but that did not
    // yield a noticeable improvement for some reason. Would like to profile this...
    Util::ArrayList<char> buffer = Util::ArrayList<char>();

    Util::ArrayList<Row> rows = Util::ArrayList<Row>();
};

#endif //HHUOS_FILEBUFFER_H
