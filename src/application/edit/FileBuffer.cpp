//
// Created by christoph on 06.03.23.
//

#include "FileBuffer.h"
#include "lib/interface.h"

FileBuffer::FileBuffer(const Util::String &path) : path(path) {
    int32_t fileDescriptor = openFile(path);
    if (fileDescriptor == -1) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Failed to open file!");
    }

    uint32_t fileLength = getFileLength(fileDescriptor);

    if (fileLength > 0) {
        auto *fileContents = new uint8_t[fileLength];
        if (readFile(fileDescriptor, fileContents, 0, fileLength) != fileLength) {
            Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Failed to load file!");
        }

        // TODO: Is there no better way to fill the ArrayList?
        //       I would like to just tell the ArrayList to use fileContents as its buffer...
        //       Or initialize the ArrayList with Array<char>::wrap(fileContents)...
        //       It is not even possible to reserve space like in std::vector?
        uint32_t linestart = 0;
        for (uint32_t i = 0; i < fileLength; ++i) {
            buffer.add(static_cast<char>(fileContents[i]));
            if (static_cast<char>(fileContents[i]) == '\n') {
                rows.add(Row(linestart, i));
                linestart = i + 1;
            }
        }
        if (buffer.get(buffer.size() - 1) != '\n') {
            buffer.add('\n');
            rows.add(Row(linestart, buffer.size()));
        }

        delete[] fileContents;
    } else {
        rows.add(Row(0, 0)); // Start with a single, empty row
        buffer.add('\n'); // Each row has this, can't be empty
    }

    closeFile(fileDescriptor);
}

// TODO: Only save if modified (after events are implemented)
void FileBuffer::save() {
    // TODO: Save to temp first, then overwrite
    if (!deleteFile(path) || !createFile(path, Util::Io::File::REGULAR)) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Failed to save file!");
    }

    int32_t fileDescriptor = openFile(path);
    if (fileDescriptor == -1) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Failed to save file!");
    }

    // TODO: Is it not possible to use the ArrayList's buffer directly?
    //       Why can't I cast ArrayList<char>::toArray() to char *?
    auto *fileContents = new uint8_t[buffer.size()];
    for (uint32_t i = 0; i < buffer.size(); ++i) {
        fileContents[i] = buffer.get(i);
    }

    writeFile(fileDescriptor, fileContents, 0, buffer.size());

    delete[] fileContents;
    closeFile(fileDescriptor);
}

void FileBuffer::insertString(uint32_t charindex, const Util::String &string) {
    for (uint32_t i = 0; i < string.length(); ++i) {
        if (string[i] == '\n') {
            prepareRowsNewLine(charindex + i);
        } else {
            prepareRowsNewCharacter(charindex + i);
        }
        buffer.add(charindex + i, string[i]);
    }
}

void FileBuffer::deleteString(uint32_t charindex, uint32_t length) {
    for (uint32_t i = 0; i < length; ++i) {
        prepareRowsDeleteCharacter(charindex + i);
        buffer.removeIndex(charindex + i);
    }
}

auto FileBuffer::getNumberOfRows() const -> uint32_t {
    return rows.size();
}

auto FileBuffer::getRowIterators(uint32_t rowindex) const -> Util::Pair<Util::Iterator<char>, Util::Iterator<char>> {
    const Row row = rows.get(rowindex);
    const auto begin = Util::Iterator<char>(buffer.toArray(), row.first);
    const auto end = Util::Iterator<char>(buffer.toArray(), row.second + 1);
    return {begin, end};
}

auto FileBuffer::getFileIterators() const -> Util::Pair<Util::Iterator<char>, Util::Iterator<char>> {
    return {buffer.begin(), buffer.end()};
}

// Rows:
// [0, 1] -> [0, 2]
// [0, 5] -> [0, 6]
// [0, 5] [6, 9] -> [0, 6] [7, 10] (insert in first line)
void FileBuffer::prepareRowsNewCharacter(uint32_t charindex) {
    auto [rowindex, row] = getRowByChar(charindex);
    rows.set(rowindex, Row(row.first, row.second + 1));

    // Translate the following rows by 1
    for (uint32_t i = rowindex + 1; i < rows.size(); ++i) {
        row = rows.get(i);
        rows.set(i, Row(row.first + 1, row.second + 1));
    }
}

// Rows:
// [0, 1] -> [0, 0]
// [0, 5] -> [0, 4]
// [0, 5] [6, 9] -> [0, 4] [5, 8] (delete in first line)
// [0, 5] [6, 9] -> [0, 8] (delete at start of second line)
// [0, 5] [6, 9] [10, 16] -> [0, 8] [9, 15] (same as above)
void FileBuffer::prepareRowsDeleteCharacter(uint32_t charindex) {
    auto [rowindex, row] = getRowByChar(charindex);

    // NOTE: This does a forward deletion, also on backspace!
    if (charindex == row.second) {
        // Delete at start of line (except first line)
        rows.set(rowindex, Row(row.first, rows.get(rowindex + 1).second - 1));
        rows.removeIndex(rowindex + 1);
    } else {
        rows.set(rowindex, Row(row.first, row.second - 1));
    }

    // Translate the following rows by -1
    for (uint32_t i = rowindex + 1; i < rows.size(); ++i) {
        row = rows.get(i);
        rows.set(i, Row(row.first - 1, row.second - 1));
    }
}

// Rows:
// [0, 2] -> [0, 2] [3, 3] (insert at end of line)
// [0, 5] -> [0, 0], [1, 6] (insert at start of line)
// [0, 5] -> [0, 2] [3, 6] (insert inside of line (index 2))
// [0, 5] [6, 9] -> [0, 2] [3, 6] [7, 10] (same as above)
void FileBuffer::prepareRowsNewLine(uint32_t charindex) {
    auto [rowindex, row] = getRowByChar(charindex);
    if (charindex == row.second) {
        // Insert at end of line
        rows.add(rowindex + 1, Row(charindex + 1, charindex + 1));
    } else {
        rows.set(rowindex, Row(row.first, charindex));
        rows.add(rowindex + 1, Row(charindex + 1, row.second + 1));
    }

    // Translate the following rows by 1
    for (uint32_t i = rowindex + 2; i < rows.size(); ++i) {
        row = rows.get(i);
        rows.set(i, Row(row.first + 1, row.second + 1));
    }
}

void FileBuffer::prepareRowsDeleteLine(uint32_t charindex) {
    auto [rowindex, row] = getRowByChar(charindex);
    // TODO
}

auto FileBuffer::getRow(uint32_t rowindex) const -> FileBuffer::Row {
    return rows.get(rowindex);
}

auto FileBuffer::getRowByChar(uint32_t charindex) const -> Util::Pair<uint32_t, Row> {
    for (uint32_t i = 0; i < rows.size(); ++i) {
        const Row row = rows.get(i);
        if (charindex >= row.first && charindex <= row.second) {
            return {i, row};
        }
    }

    Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "getRowByChar(): Index outside of buffer!");
}

FileBuffer::Row::Row(uint32_t begin, uint32_t end) : Pair(begin, end) {}
