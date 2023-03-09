//
// Created by christoph on 06.03.23.
//

#ifndef HHUOS_FILEBUFFER_H
#define HHUOS_FILEBUFFER_H

#include "FileBufferRow.h"

constexpr uint16_t INITIAL_ROWS = 32; ///< @brief Initial number of rows in a buffer.

/**
 * @brief This class is the in-memory representation of a file opened for editing.
 *
 * It is responsible for managing text input regarding the file rows.
 * A file is represented as an FileBufferRow array.
 */
class FileBuffer {
public:
    /**
     * @brief Default constructor.
     */
    FileBuffer();

    /**
     * @brief Construct a FileBuffer with content.
     *
     * @param string The content string
     */
    explicit FileBuffer(const Util::String &string);

    /**
     * @brief Destructor.
     *
     * Frees the row buffer.
     */
    ~FileBuffer();

    /**
     * @brief Insert a character into the FileBuffer (in an existing line).
     *
     * @param rowIndex The line where the character will be inserted
     * @param colIndex The position where the character will be inserted into the line
     * @param character The character that will be inserted
     */
    void insertCharacter(uint16_t rowIndex, uint16_t colIndex, char character);

    /**
     * @brief Insert a string of characters into the FileBuffer (in an existing line).
     *
     * @param rowIndex The line where the characters will be inserted
     * @param colIndex The starting position where the characters will be inserted
     * @param string The characters to insert
     */
    void insertString(uint16_t rowIndex, uint16_t colIndex, const Util::String &string);

    /**
     * @brief Delete a character from the FileBuffer.
     *
     * @param rowIndex The line from which the character should be deleted
     * @param colIndex The position where the character will be deleted from the line
     */
    void deleteCharacter(uint16_t rowIndex, uint16_t colIndex);

    /**
     * @brief Insert a line into the FileBuffer.
     *
     * @param rowIndex The position where the line should be inserted
     * @param row The contents of the new line
     */
    void insertRow(uint16_t rowIndex, const Util::String &row = "");

    /**
     * @brief Append a line to the end of the FileBuffer.
     *
     * @param row The contents of the new line
     */
    void appendRow(const Util::String &row = "");

    /**
     * @brief Remove a line from the FileBuffer.
     *
     * @param rowIndex The position of the line that should be removed
     */
    void deleteRow(uint16_t rowIndex);

    /**
     * @brief Determine the length of a line.
     */
    [[nodiscard]] uint16_t rowSize(uint16_t rowIndex) const;

    /**
     * @brief Determine the contents of a row.
     */
    [[nodiscard]] Util::String rowContent(uint16_t rowIndex) const;

    /**
     * @brief Determine the number of lines contained in the FileBuffer.
     */
    [[nodiscard]] uint16_t size() const;

    /**
     * @brief Convert the FileBuffer to a hhuOS heap-allocated Util::String.
     */
    explicit operator Util::String() const;

private:
    /**
     * @brief Make sure that the allocated buffer is large enough.
     *
     * If the allocated memory is too small, the buffer capacity will be doubled.
     *
     * @param insertSize The amount of space that needs to be available in the buffer
     */
    void ensureCapacity(uint16_t insertSize = 1);

    /**
     * @brief Create an empty line "slot" in the FileBuffer.
     *
     * A slot is created by moving each element (starting at rowIndex) to the right.
     * The length will be increased by this function.
     * After making space, the element at rowIndex is invalid and has to be initialized afterwards.
     *
     * @param colIndex The position where the line "slot" will be created
     */
    void makeSpace(uint16_t rowIndex);

    /**
     * @brief Remove a line "slot" from the FileBuffer.
     *
     * A slot is removed by moving each element (starting after rowIndex) to the left.
     * The length will be decreased by this function.
     *
     * @param colIndex The position of the line "slot" that will be removed
     */
    void removeSpace(uint16_t rowIndex);

    uint16_t length = 0;
    uint16_t capacity = 0;
    FileBufferRow **rows; ///< @brief The array of pointers to the row data
};

#endif //HHUOS_FILEBUFFER_H
