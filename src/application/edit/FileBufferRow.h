//
// Created by christoph on 06.03.23.
//

#ifndef HHUOS_FILEBUFFERROW_H
#define HHUOS_FILEBUFFERROW_H

#include <cstdint>
#include "lib/util/base/String.h"

/**
 * @brief This class is the in-memory representation of a single text line.
 *
 * It manages text input into a single line regarding the columns.
 */
class FileBufferRow {
public:
    /**
     * @brief Default constructor.
     */
    FileBufferRow() = default;

    /**
     * @brief Construct a FileBufferRow with content.
     *
     * @param row The content string's source buffer
     * @param length The length of the content string
     */
    explicit FileBufferRow(const Util::String &row);

    /**
     * @brief Default destructor.
     */
    ~FileBufferRow() = default;

    /**
     * @brief Insert a character into the FileBufferRow.
     *
     * @param colIndex The position where the character will be inserted
     * @param character The character to insert
     */
    void insertCharacter(uint16_t colIndex, char character);

    /**
     * @brief Append a character to the end of the FileBufferRow.
     *
     * @param character The character to append
     */
    void appendCharacter(char character);

    /**
     * @brief Insert a string of characters into the FileBufferRow.
     *
     * @param colIndex The starting position where the characters will be inserted
     * @param string The characters to insert
     */
    void insertString(uint16_t colIndex, const Util::String &string);

    /**
     * @brief Append a string of characters to the end of the FileBufferRow.
     *
     * @param string The characters to append
     */
    void appendString(const Util::String &string);

    /**
     * @brief Delete a character from the FileBufferRow.
     *
     * @param colIndex The position of the character that will be deleted
     */
    void deleteCharacter(uint16_t colIndex);

    /**
     * @brief Determine the size of the string contained in the FileBufferRow.
     */
    [[nodiscard]] uint16_t size() const;

    [[nodiscard]] bool isLastColumn(uint16_t colIndex) const;

    /**
     * @brief Convert the FileBufferRow to a hhuOS heap-allocated Util::String.
     */
    explicit operator Util::String() const;

private:
    /**
     * @brief Throw if the colIndex is not in the buffer.
     */
    void ensureInBuffer(uint16_t colIndex) const;

    /**
     * @brief Throw if the colIndex is not in the buffer or not the first line after the buffer.
     */
    void ensureAdjacentOrInBuffer(uint16_t colIndex) const;

    Util::String columns;
};

#endif //HHUOS_FILEBUFFERROW_H
