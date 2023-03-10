//
// Created by christoph on 06.03.23.
//

#ifndef HHUOS_FILEBUFFERROW_H
#define HHUOS_FILEBUFFERROW_H

#include <cstdint>
#include "lib/util/base/String.h"

constexpr uint16_t INITIAL_COLS = 64; ///< @brief Initial number of characters in a row.

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
    FileBufferRow();

    /**
     * @brief Construct a FileBufferRow with content.
     *
     * @param row The content string's source buffer
     * @param length The length of the content string
     */
    explicit FileBufferRow(const Util::String &row);

    /**
     * @brief Destructor.
     *
     * Frees the content buffer.
     */
    ~FileBufferRow();

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
    void ensureAdjacentToBuffer(uint16_t colIndex) const;

    /**
     * @brief Make sure that the allocated buffer is large enough.
     *
     * If the allocated memory is too small, the buffer capacity will be doubled.
     *
     * @param insertSize The amount of space that needs to be available in the buffer
     */
    void ensureCapacity(uint16_t insertSize = 1);

    // TODO: Should I implement buffer shrinking or is this unnecessary?

    /**
     * @brief Create an empty character "slot" in the FileBufferRow.
     *
     * A slot is created by moving each element (starting at colIndex) to the right.
     * The length will be increased by this function.
     * After making space, the element at colIndex is invalid and has to be initialized afterwards.
     *
     * @param colIndex The position where the character "slot" will be created
     * @param insertSize The amount of space that needs to be made
     */
    void makeSpace(uint16_t colIndex, uint16_t insertSize = 1);

    /**
     * @brief Remove a character "slot" from the FileBufferRow.
     *
     * A slot is removed by moving each element (starting after colIndex) to the left.
     * The length will be decreased by this function.
     *
     * @param colIndex The position of the character "slot" that will be removed
     */
    void removeSpace(uint16_t colIndex);

    uint16_t length = 0; ///< @brief The length of the string contained in the FileBufferRow.
    uint16_t capacity; ///< @brief The capacity of the allocated buffer.
    char *columns; ///< @brief The buffer that stores the String/character data.
};

#endif //HHUOS_FILEBUFFERROW_H
