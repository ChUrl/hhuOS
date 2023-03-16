//
// Created by christoph on 06.03.23.
//

#ifndef HHUOS_FILEBUFFER_H
#define HHUOS_FILEBUFFER_H

#include "FileBufferRow.h"
#include "lib/util/graphic/Ansi.h"
#include "lib/util/collection/ArrayList.h"

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
    FileBuffer() = default;

    /**
     * @brief Default destructor.
     */
    ~FileBuffer() = default;

    /**
     * @brief Insert a character into the FileBuffer (in an existing line).
     *
     * @param rowIndex The line where the character will be inserted
     * @param cursor The position where the character will be inserted into the line
     * @param character The character that will be inserted
     */
    void insertCharacter(Util::Graphic::Ansi::CursorPosition cursor, char character);

    /**
     * @brief Insert a string of characters into the FileBuffer (in an existing line).
     *
     * @param rowIndex The line where the characters will be inserted
     * @param colIndex The starting position where the characters will be inserted
     * @param string The characters to insert
     */
    void insertString(Util::Graphic::Ansi::CursorPosition cursor, const Util::String &string);

    /**
     * @brief Delete a character from the FileBuffer.
     *
     * @param rowIndex The line from which the character should be deleted
     * @param cursor The position where the character will be deleted from the line
     */
    void deleteCharacter(Util::Graphic::Ansi::CursorPosition cursor);

    /**
     * @brief Insert a line into the FileBuffer.
     *
     * @param cursor The position where the line should be inserted
     * @param row The contents of the new line
     */
    void insertRow(Util::Graphic::Ansi::CursorPosition cursor, const Util::String &row = "");

    /**
     * @brief Append a line to the end of the FileBuffer.
     *
     * @param row The contents of the new line
     */
    void appendRow(const Util::String &row = "");

    /**
     * @brief Remove a line from the FileBuffer.
     *
     * @param cursor The position of the line that should be removed
     */
    void deleteRow(Util::Graphic::Ansi::CursorPosition cursor);

    /**
     * @brief Determine the length of a line.
     */
    [[nodiscard]] uint16_t rowSize(Util::Graphic::Ansi::CursorPosition cursor) const;

    /**
     * @brief Determine the contents of a row.
     */
    void rowContent(Util::Graphic::Ansi::CursorPosition cursor, uint16_t start, uint16_t end, Util::String &string) const;
    void rowContent(Util::Graphic::Ansi::CursorPosition cursor, Util::String &string) const;

    /**
     * @brief Determine the number of lines contained in the FileBuffer.
     */
    [[nodiscard]] uint16_t size() const;

    [[nodiscard]] bool isLastColumn(Util::Graphic::Ansi::CursorPosition cursor) const;

    [[nodiscard]] bool isLastRow(Util::Graphic::Ansi::CursorPosition cursor) const;

    void getRows(Util::Array<Util::String> &rowStrings) const;

private:
    Util::ArrayList<FileBufferRow *> rows;
};

#endif //HHUOS_FILEBUFFER_H
