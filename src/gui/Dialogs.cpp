#include "Dialogs.h"

#include <libintl.h>
#include <tinyfiledialogs/tinyfiledialogs.h>

#define _(String) gettext(String)

#define YES 1
#define NO 0

char* UI::selectFolderDialog() {
    return tinyfd_selectFolderDialog( _("Select folder"), nullptr);
}

char* UI::openFileDialog(const char* const* filters, size_t numOfFilters, const char* description, bool multiSelect) {
    return tinyfd_openFileDialog(
        multiSelect ? _("Open file(s)") : _("Open file"),
        nullptr,
        numOfFilters,
        filters,
        description,
        multiSelect
    );
}

char* UI::saveFileDialog(const char* const* filters, size_t numOfFilters, const char* description) {
    return tinyfd_saveFileDialog(
        _("Save file"),
        nullptr,
        numOfFilters, 
        filters,
        description
    );
}

bool UI::askToSave(const std::filesystem::path& path) {
    static std::string message = _("Save the file before closing?\n");
    return tinyfd_messageBox(_("Save file?"), (message + path.string()).c_str(), "yesno", "question", 1) == YES;
}

bool UI::warningMessageBox(WARNING_MESSAGE warningMessage, const std::string& messageExtension) {
    const char* title;
    std::string message;
    switch (warningMessage) {
    
    case (WARNING_REQUIRES_PID_CONVERSION):
        title = _("Requires PID conversion");
        message = _("This operation requires conversion to PID format. All the BMP and/or PCX files will be replace. Do you wish to continue?");
        break;

    case (WARNING_ASK_TO_DELETE_SINGLE):
        title = _("Delete the node?");
        message = _("Are you sure you want to permanently delete the file?\n");
        message += messageExtension;
        break;

    case (WARNING_ASK_TO_DELETE_MULTIPLE):
        title = _("Delete the node?");
        message = _("Are you sure you want to permanently delete the folder and all its content?\n");
        message += messageExtension;
        break;

    case (WARNING_ASK_TO_OVERWRITE):
        title = _("Overwrite the file?");
        message = _("The file or folder already exists in the destination directory. Do you want to replace it?\n");
        message += messageExtension;
        break;

    default:
        return false;
    }

    return tinyfd_messageBox(title, message.c_str(), "yesno", "warning", 1) == YES;
}

void UI::errorMessageBox(ERROR_MESSAGE errorMessage, const std::string& messageExtension) {
    const char* title;
    std::string message;
    switch (errorMessage) {

    case ERROR_UNSUPPORTED_GAME:
        title = _("Game not recognized");
        message = _("Selected game is not supported.");
        break;
    
    case ERROR_OLD_CLAW_VERSION:
        title = _("No Assets directory");
        message = _("Looks like you are trying to use older version of Claw where assets were packed inside CLAW.REZ file. Please update to CrazyHook version.");
        break;

    case ERROR_BATCH_FULL_FAILURE:
        title = _("Batch error");
        message = _("The operation could not be completed.");
        break;

    case ERROR_BATCH_DIR_EMPTY:
        title = _("Batch error");
        message = _("The directory is empty.");
        break;

    case ERROR_PASTE_NO_SOURCE:
        title = _("Paste error");
        message = _("Cannot paste - source does not exist.");
        break;

    case ERROR_PASTE_NO_DESTINATION:
        title = _("Paste error");
        message = _("Cannot paste - destination does not exist.");
        break;

    case ERROR_PASTE_UNKNOWN:
        title = _("Paste error");
        message = _("Cannot paste - unknown reason.");
        break;

    case ERROR_FILE_OVERWRITING_NOT_ALLOWED:
        title = _("Write error");
        message = _("Overwriting is not allowed.");
        break;

    case ERROR_FILE_ALREADY_EXISTS:
        title = _("Error");
        message = _("The file or folder already exists!");
        break;

    case ERROR_INVALID_FILENAME:
        title = _("Error");
        message = _("The name is not valid as a file or folder name!");
        break;

    case ERROR_INVALID_PCX:
        title = _("PCX open error");
        message = _("Invalid or unsupported PCX file!");
        break;

    case ERROR_INVALID_BMP:
        title = _("BMP open error");
        message = _("Invalid or unsupported BMP file!");
        break;

    case ERROR_NOT_8_BIT_IMAGE:
        title = _("Image open error");
        message = _("The file is not a supported 8-bit image.");
        break;

    default:
        title = _("Error");
        message = _("Unknown error");
        break;
    }

    tinyfd_messageBox(title, message.c_str(), "ok", "error", 1);
}

void UI::messageBox(const char* message) {
    tinyfd_messageBox("Message", message, "ok", "info", 1);    
}
