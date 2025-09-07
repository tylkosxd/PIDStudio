#pragma once

#include <filesystem>

enum ERROR_MESSAGE {
    ERROR_UNSUPPORTED_GAME,
    ERROR_OLD_CLAW_VERSION,
    ERROR_BATCH_FULL_FAILURE,
    ERROR_BATCH_DIR_EMPTY,
    ERROR_PASTE_NO_SOURCE,
    ERROR_PASTE_NO_DESTINATION,
    ERROR_PASTE_UNKNOWN,
    ERROR_FILE_OVERWRITING_NOT_ALLOWED,
    ERROR_FILE_ALREADY_EXISTS,
    ERROR_INVALID_FILENAME,
    ERROR_INVALID_PCX,
    ERROR_INVALID_BMP,
    ERROR_NOT_8_BIT_IMAGE
};

enum WARNING_MESSAGE {
    WARNING_REQUIRES_PID_CONVERSION,
    WARNING_ASK_TO_DELETE_SINGLE,
    WARNING_ASK_TO_DELETE_MULTIPLE,
    WARNING_ASK_TO_OVERWRITE
};

namespace UI {
    char* selectFolderDialog();
    char* openFileDialog(const char* const* filters, size_t numOfFilters, const char* description, bool multiSelect = false);
    char* saveFileDialog(const char* const* filters, size_t numOfFilters, const char* description);
    bool askToSave(const std::filesystem::path& filePath);
    bool warningMessageBox(WARNING_MESSAGE warningMessage, const std::string& messageExtension = "");
    void errorMessageBox(ERROR_MESSAGE errorMessage, const std::string& messageExtension = "");
    void messageBox(const char* message);
}