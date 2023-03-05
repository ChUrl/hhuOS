//
// Created by christoph on 05.03.23.
//

#ifndef HHUOS_EDIT_H
#define HHUOS_EDIT_H

#include "lib/util/async/Runnable.h"
#include "lib/util/base/String.h"
#include "lib/util/io/file/File.h"
#include "lib/util/io/stream/FileInputStream.h"
#include "lib/util/io/stream/BufferedInputStream.h"
#include "lib/util/io/stream/BufferedOutputStream.h"
#include "lib/util/io/stream/FileOutputStream.h"
#include "lib/util/collection/ArrayList.h"

class Edit : public Util::Async::Runnable {
public:
    explicit Edit(const Util::String &path);

    ~Edit() override;

    /**
     * @brief Override function from Util::Async::Runnable.
     */
    void run() override;

private:
    // UI related ===============================
    void handleUserInput();

    void handleTextChar(int16_t input);

    void handleBackspace();

    void updateDisplay();

    // File related =============================
    void loadFile();

    void saveFile();

    // Memory related ===========================
    void ensureBufferCapacity(uint32_t insert = 1);

    // Editing related ==========================
    void ensureNewLine();

    void trimWhitespace();

private:
    static const constexpr uint32_t INITIAL_BUFFER_CAPACITY = 512;
    bool isRunning = true;
    bool isModified = false; ///< @brief Indicates if the buffer was changed since the last save.

    // Could have used some FileStream for this, but wouldn't make so much sense since
    // either the complete file is read or written.
    const int32_t fileDescriptor; ///< @brief The file opened for editing.
    uint32_t fileLength = 0; ///< @brief The length of the buffered file, NOT the actual file.

    // TODO: Replace by some kind of row array
    char *buffer = new char[INITIAL_BUFFER_CAPACITY]; ///< @brief The buffer used for editing.
    uint32_t bufferCapacity = INITIAL_BUFFER_CAPACITY; ///< @brief The current storage capacity of the buffer.

};

#endif //HHUOS_EDIT_H
