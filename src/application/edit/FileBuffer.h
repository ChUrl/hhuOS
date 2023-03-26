//
// Created by christoph on 06.03.23.
//

#ifndef HHUOS_FILEBUFFER_H
#define HHUOS_FILEBUFFER_H

#include "lib/util/graphic/Ansi.h"
#include "lib/util/base/Constants.h"
#include "lib/util/base/Address.h"
#include "lib/util/collection/ArrayList.h"
#include "lib/util/collection/Pair.h"

#define ENABLE_EDIT_DEBUG 0
#define DEBUG_EXCEPTION(msg) Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, msg)

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

    [[nodiscard]] auto getSingleRow(uint32_t rowindex) const -> Util::Pair<Util::Iterator<char>, Util::Iterator<char>>;

    [[nodiscard]] auto getAllRows() const -> Util::Pair<Util::Iterator<char>, Util::Iterator<char>>;

protected:
    struct Row : public Util::Pair<uint32_t, uint32_t> {
        Row() = default;

        Row(uint32_t begin, uint32_t end);
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
    [[nodiscard]] auto getCharacterRow(uint32_t charindex) const -> Util::Pair<uint32_t, Row>;

protected:
    Util::String path;

    // This approach is very simple to implement, in comparison to the line-based buffer approach
    // that I used before. Drawback: On each line manipulation, the whole part of the file after
    // the cursor has to be moved in memory. We'll see how larger files handle...
    Util::ArrayList<char> buffer = Util::ArrayList<char>();

    Util::ArrayList<Row> rows = Util::ArrayList<Row>();
};

#endif //HHUOS_FILEBUFFER_H
