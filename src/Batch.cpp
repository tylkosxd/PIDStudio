#include "PIDStudio.h"
#include "formats/PIDFile.h"
#include "Batch.h"
#include "String.h"
#include "ImGuiExtensions.h"
#include "gui/Dialogs.h"
#include "formats/PIDPalette.h"
#include "formats/PNGFile.h"
#include "PaletteLUV.h"
#include "Filesystem.h"

#include "AssetLibrary.h"
#include "Project.h"

#include "assets/IconsLucide.h"
#include "assets/lucide.ttf.h"

#include <libintl.h>

#define _(String) gettext(String)

#define PROGRESS_BAR_COLOR 0xFF0010E0

static int FILES_PER_CALL = 4;

enum POPUP_STATE {
    KEEP_CLOSED,
    OPEN,
    KEEP_OPENED,
    CLOSE
};

namespace fs = std::filesystem;

size_t      l_baseDirLen;
fs::path    l_exportDir;
fs::path    l_pathOut;
fs::path    l_pathIn;

int l_processed     = 0;
int l_totalAmount   = 0;
int l_failed        = 0;

BATCH_TYPE l_batchType;

std::vector<fs::path> l_batchFiles;

std::shared_ptr<PIDPalette> l_inPalette;
std::shared_ptr<PIDPalette> l_outPalette;
std::unique_ptr<PaletteLUV> l_outPaletteLUV;

bool l_hasNonPIDS = false;

POPUP_STATE l_batchPopup = KEEP_CLOSED;
POPUP_STATE l_batchCreatorPopup = KEEP_CLOSED;
POPUP_STATE l_batchImportPNGSPopup = KEEP_CLOSED;

char l_srcPathBuffer[260] = {0};
char l_dstPathBuffer[260] = {0};
bool l_paletteCheckbox = false;
char l_inPaletteComboBuffer[128] = {0};
char l_outPaletteComboBuffer[128] = {0};

struct FLAGS_CB {
    bool transparency;
    bool compression;
    bool savePalette;
    bool videoMem;
    bool sysMem;
    bool optimization;
    bool lockOptimization;
    bool grayscale;
};

static FLAGS_CB FLAGS_CB_DEFAULTS   = {true, false, false, false, false, true, false, false};
FLAGS_CB l_flagsCb                  = {true, false, false, false, false, true, false, false};

void directoriesSelection(PIDStudio* app) {
    using namespace ImGui;
    if (Button(ICON_LC_FOLDER_DOWN)) {
        char* selectedFolder = UI::selectFolderDialog();
        if (selectedFolder) {
            fs::path path = selectedFolder;
            copyCharArray(l_srcPathBuffer, app->shortenFilePath(path), 259);
            l_pathIn = path;
        }
    }
    SameLine();
    InputText(_("Source"), l_srcPathBuffer, 260, ImGuiInputTextFlags_ReadOnly);

    if (Button(ICON_LC_FOLDER_UP)) {
        char* selectedFolder = UI::selectFolderDialog();
        if (selectedFolder) {
            fs::path path = selectedFolder;
            copyCharArray(l_dstPathBuffer, app->shortenFilePath(path), 259);
            l_exportDir = path;
        }
    }
    SameLine();
    InputText(_("Destination"), l_dstPathBuffer, 260, ImGuiInputTextFlags_ReadOnly);
}

void outPaletteCombo(PIDStudio* app, const char* label) {
    using namespace ImGui;
    if (BeginCombo(label, l_outPaletteComboBuffer)) {

        for (const auto& palette : app->libPalettes) {
            if (Selectable((palette->getName()).c_str())) {
                copyCharArray(l_outPaletteComboBuffer, palette->getName(), 127);
                l_outPalette = palette;
            }
        }

        Separator();

        for (const auto& palette : app->customPalettes) {
            if (Selectable(palette->getName().c_str())) {
                copyCharArray(l_outPaletteComboBuffer, palette->getName(), 127);
                l_outPalette = palette;
            }
        }

        if (l_outPaletteComboBuffer == app->defaultPalette->getName()) {
            l_flagsCb.grayscale = true;
        }
        else {
            l_flagsCb.grayscale = false;
        }
        EndCombo();
    }
}

void inPaletteCombo(PIDStudio* app, const char* label) {
    using namespace ImGui;
    if (BeginCombo(label, l_inPaletteComboBuffer)) {

        for (const auto& palette : app->libPalettes) {
            if (Selectable((palette->getName()).c_str())) {
                copyCharArray(l_inPaletteComboBuffer, palette->getName(), 127);
                l_inPalette = palette;
            }
        }

        Separator();

        for (const auto& palette : app->customPalettes) {
            if (Selectable(palette->getName().c_str())) {
                copyCharArray(l_inPaletteComboBuffer, palette->getName(), 127);
                l_inPalette = palette;
            }
        }

        EndCombo();
    }
}

void flagsCheckboxes() {
    using namespace ImGui;
    Checkbox(_("Use transparency"), &l_flagsCb.transparency);
    Checkbox(_("PID-RLE2 compression"), &l_flagsCb.compression);
    Checkbox(_("Save with palette"), &l_flagsCb.savePalette);
    Checkbox(_("Use video memory"), &l_flagsCb.videoMem);
    if (l_flagsCb.videoMem)
        l_flagsCb.sysMem = false;
    Checkbox(_("Use system memory"), &l_flagsCb.sysMem);
    if (l_flagsCb.sysMem)
        l_flagsCb.videoMem = false;
    Checkbox(_("Optimize"), &l_flagsCb.optimization);
    if (l_flagsCb.optimization)
        l_flagsCb.lockOptimization = false;
    Checkbox(_("Disallow future optimization"), &l_flagsCb.lockOptimization);
    if (l_flagsCb.lockOptimization)
        l_flagsCb.optimization = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////// OUTSIDE SETTINGS ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Batch::setInPalette(const std::shared_ptr<PIDPalette>& palette) {
    l_inPalette = palette;
}

void Batch::setOutPalette(const std::shared_ptr<PIDPalette>& palette) {
    l_outPalette = palette;
}

void Batch::setExportDirectory(const fs::path& path) {
    l_exportDir = path;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////// IMPORT PNGS SETTINGS /////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void Batch::openImportPNGSSettings(PIDStudio* app, const fs::path& pathOut) {
    l_exportDir = pathOut;
    std::string shortenedPath = app->shortenFilePath(pathOut);
    memset(l_srcPathBuffer, 0, 259);
    copyCharArray(l_dstPathBuffer, shortenedPath, 259);
    if (app->currentProject && app->currentProject->palette) {
        copyCharArray(l_outPaletteComboBuffer, app->currentProject->palette->getName(), 127);
        l_outPalette = app->currentProject->palette;
    }
    else {
        copyCharArray(l_outPaletteComboBuffer, app->defaultPalette->getName(), 127);
        l_outPalette = app->defaultPalette;
    }
    l_batchImportPNGSPopup = OPEN;
}

void Batch::importPNGSSettings(PIDStudio* app) {
    static float popupWidth = 528.0f, popupHeight = 330.0f;
    bool canStart = false;

    using namespace ImGui;

    switch(l_batchImportPNGSPopup) {
    
    case KEEP_CLOSED:
        return;

    case OPEN:
        l_flagsCb = FLAGS_CB_DEFAULTS;
        ImGuiEx::CenterNextWindow(popupWidth, popupHeight);
        OpenPopup(_("Import PNGS"));
        l_batchImportPNGSPopup = KEEP_OPENED;
        // fall through

    case KEEP_OPENED:
        if (BeginPopupModal(_("Import PNGS"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

            directoriesSelection(app);

            outPaletteCombo(app, _("Transform to palette"));

            flagsCheckboxes();

            if (Button("Cancel")) {
                l_exportDir.clear();
                l_outPalette = nullptr;
                l_batchImportPNGSPopup = KEEP_CLOSED;
                CloseCurrentPopup();
            }

            canStart = l_srcPathBuffer[0] != NULL && l_dstPathBuffer[0] != NULL;

            if (!canStart)
                BeginDisabled();

            SameLine();
            if (Button(_("Start"))) {
                start(l_pathIn, BATCH_IMPORT_PNGS);
                l_batchImportPNGSPopup = KEEP_CLOSED;
                CloseCurrentPopup();
            }

            if (!canStart)
                EndDisabled();

            EndPopup();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////// BATCH CREATOR /////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void Batch::openCreator(PIDStudio* app, const fs::path& path) {
    l_pathIn = path;
    std::string shortenedPath = app->shortenFilePath(path);
    copyCharArray(l_srcPathBuffer, shortenedPath, 259);
    copyCharArray(l_dstPathBuffer, shortenedPath, 259);
    if (app->currentProject && app->currentProject->palette)
        copyCharArray(l_inPaletteComboBuffer, app->currentProject->palette->getName(), 127);
    l_batchCreatorPopup = OPEN;
}

void Batch::creator(PIDStudio* app) {
    static float creatorWidth = 608.0f, creatorHeight = 380.0f;
    bool canStart = false;

    using namespace ImGui;

    switch(l_batchCreatorPopup) {
    
    case KEEP_CLOSED:
        return;

    case OPEN:
        l_paletteCheckbox = false;
        l_flagsCb = FLAGS_CB_DEFAULTS;
        ImGuiEx::CenterNextWindow(creatorWidth, creatorHeight);
        OpenPopup(_("Batch creator"));
        l_batchCreatorPopup = KEEP_OPENED;

    case KEEP_OPENED:
        if (BeginPopupModal(_("Batch creator"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

            directoriesSelection(app);

            Checkbox(_("Transform to palette"), &l_paletteCheckbox);

            if (!l_paletteCheckbox)
                BeginDisabled();

            inPaletteCombo(app, _("From"));
            outPaletteCombo(app, _("To"));
            
            if (!l_paletteCheckbox)
                EndDisabled();

            flagsCheckboxes();
            
            if (Button("Cancel")) {
                l_exportDir.clear();
                l_outPalette = nullptr;
                l_batchCreatorPopup = KEEP_CLOSED;
                CloseCurrentPopup();
            }

            canStart = l_srcPathBuffer[0] != NULL && l_dstPathBuffer[0] != NULL;

            if (!canStart)
                BeginDisabled();

            SameLine();
            if (Button(_("Start"))) {
                if (!l_paletteCheckbox)
                    l_outPalette = nullptr;
                start(l_pathIn, BATCH_CREATOR_PROCESS);
                l_batchCreatorPopup = KEEP_CLOSED;
                CloseCurrentPopup();
            }

            if (!canStart)
                EndDisabled();

            EndPopup();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// BATCH START /////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Batch::start(const fs::path& path, BATCH_TYPE t) {
    if (!fs::is_directory(path))
        return;

    if (l_batchFiles.capacity() < 12500)
        l_batchFiles.reserve(12500); // roughly the entire Claw library

    l_baseDirLen = path.string().length();
    l_baseDirLen++;
    l_totalAmount = 0;
    l_processed = 0;
    l_failed = 0;
    l_batchType = t;
    l_hasNonPIDS = false;

    if (l_batchType == BATCH_IMPORT_PNGS) {
        for (auto const& entry : fs::recursive_directory_iterator(path)) {
            std::string ext = getFileExtension((fs::path)entry);
            if (ext == ".png") {
                l_totalAmount++;
                l_batchFiles.push_back((fs::path)entry);
            }
        }
        l_outPaletteLUV = std::make_unique<PaletteLUV>(l_outPalette->getData());
        FILES_PER_CALL = 1;
    }
    else {
        for (auto const& entry : fs::recursive_directory_iterator(path)) {
            std::string ext = getFileExtension((fs::path)entry);
            if (ext == ".pid") {
                l_totalAmount++;
                l_batchFiles.push_back((fs::path)entry);
                continue;
            }
            if (ext == ".bmp" || ext == ".pcx") {
                l_totalAmount++;
                l_batchFiles.push_back((fs::path)entry);
                l_hasNonPIDS = true;
            }
        }
        FILES_PER_CALL = 3;
    }

    if (l_totalAmount == 0) {
        UI::errorMessageBox(ERROR_BATCH_DIR_EMPTY);
        return;
    }

    if (l_exportDir == l_pathIn)
        l_exportDir.clear();

    l_batchPopup = OPEN;
}

void Batch::start(const std::vector<fs::path>& paths, BATCH_TYPE t) {
    if (paths.empty())
        return;

    l_totalAmount = paths.size();
    l_processed = 0;
    l_failed = 0;
    l_batchType = t;
    l_batchFiles = paths;

    for (auto const& path : paths) {
        std::string ext = getFileExtension(path);
        if (ext == ".bmp" || ext == ".pcx")
            l_hasNonPIDS = true;
    }

    FILES_PER_CALL = 4;

    if (l_outPalette == l_inPalette) {
        l_outPalette = nullptr;
        l_inPalette = nullptr;
    }

    if (!l_outPalette && l_batchType == BATCH_TRANSFORM_PALETTE)
        return;

    if (l_exportDir == l_pathIn)
        l_exportDir.clear();

    l_batchPopup = OPEN;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////// BATCH /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Batch::batch(PIDStudio* app) {
    static float width = 208.0f, height = 130.0f; // keeping the golden ratio
    static const char* buttonLabel;

    using namespace ImGui;

    switch (l_batchPopup) {
    
    case KEEP_CLOSED:
        return;

    case OPEN:
        if (l_totalAmount == 0) {
            l_batchPopup = CLOSE;
            return;
        }

        if (l_hasNonPIDS && (l_batchType == BATCH_TRANSFORM_PALETTE || l_batchType == BATCH_CREATOR_PROCESS) && l_exportDir.empty()) {
            if (!UI::warningMessageBox(WARNING_REQUIRES_PID_CONVERSION)) {
                l_batchPopup = CLOSE;
                return;
            }
        }

        ImGuiEx::CenterNextWindow(width, height);
        OpenPopup(_("Batch processing"));
        l_batchPopup = KEEP_OPENED;
        // continue;

    case KEEP_OPENED: 
        if (BeginPopupModal(_("Batch processing"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

            ImGuiEx::ProgressBar((float)l_processed/(float)l_totalAmount, PROGRESS_BAR_COLOR);

            for (int file = 0; file < FILES_PER_CALL; file++) {
                processSingleFile(app);
            }

            if (l_failed == 0)
                Text("%s%d/%d", "Processed: ", l_processed, l_totalAmount);
            else
                Text("%s: %d/%d, %s: %d", "Processed", l_processed, l_totalAmount, "Failed", l_failed);
            
            if (l_processed == l_totalAmount)
                buttonLabel = _("Ok");
            else
                buttonLabel = _("Cancel");

            if (Button(buttonLabel)) {
                l_batchPopup = CLOSE;
                CloseCurrentPopup();
            }

            EndPopup();
        }
        break;
    
    case CLOSE:
        if (l_failed == l_totalAmount)
            UI::errorMessageBox(ERROR_BATCH_FULL_FAILURE);

        if (!l_batchFiles.empty())
            l_batchFiles.clear();

        l_batchPopup = KEEP_CLOSED;
        l_processed = 0;
        l_totalAmount = 0;
        l_baseDirLen = 0;
        l_failed = 0;
        l_exportDir.clear();
        l_pathOut.clear();
        l_inPalette = nullptr;
        l_outPalette = nullptr;
        break;

    default:
        break;
    }
}

void Batch::processSingleFile(PIDStudio* app) {
    if (l_batchFiles.empty())
        return;

    bool success = true;
    fs::path& pathIn = l_batchFiles.back();

    // make path out
    if (l_exportDir.empty()) {
        l_pathOut = pathIn;
    }
    else {
        l_pathOut = l_exportDir / (fs::path)pathIn.string().substr(l_baseDirLen);
        if (!fs::exists(l_pathOut.parent_path()))
            fs::create_directories(l_pathOut.parent_path());
    }

    auto endProcessing = [&]() {
        if (!success)
            l_failed++;
        l_processed++;
        l_batchFiles.pop_back();
        app->currentBackgroundFile.reset();
    };

    if (l_batchType == BATCH_IMPORT_PNGS) {
        success = importSinglePNG(app, pathIn);
        endProcessing();
        return;
    }

    app->openImageFile(pathIn, l_inPalette, true);

    const std::shared_ptr<PIDFile>& file = app->currentBackgroundFile;

    if (!file) {
        success = false;
        endProcessing();
        return;
    }

    switch (l_batchType) {

    case BATCH_SAVE_AS_PNG:
        l_pathOut.replace_extension(".png");
        success = file->exportToPNG(l_pathOut);
        endProcessing();
        return;

    case BATCH_SAVE_AS_PID:
        l_pathOut.replace_extension(".pid");
        success = file->saveToFile(l_pathOut);
        endProcessing();
        return;

    case BATCH_CREATOR_PROCESS:
        file->setFlag(PID_Flag_Transparency, l_flagsCb.transparency);
        file->setFlag(PID_Flag_VideoMemory, l_flagsCb.videoMem);
        file->setFlag(PID_Flag_SystemMemory, l_flagsCb.sysMem);
        file->setFlag(PID_Flag_Compression, l_flagsCb.compression);
        file->setFlag(PID_Flag_OwnPalette, l_flagsCb.savePalette);
        file->shouldOptimize = l_flagsCb.optimization;
        file->setFlag(PID_Flag_Never_Optimize, l_flagsCb.lockOptimization);
        file->setFlag(PID_Flag_Grayscale, l_flagsCb.grayscale);
        // fallthrough

    case BATCH_TRANSFORM_PALETTE:
        if (l_outPalette)
            file->transformToPalette(l_outPalette);
        l_pathOut.replace_extension(".pid");
        success = file->saveToFile(l_pathOut);
        if (success && !(file->isPID()) && l_exportDir.empty())
            std::filesystem::remove(pathIn);
        break;

    default:
        break;

    }

    endProcessing();
}

bool Batch::importSinglePNG(PIDStudio* app, const fs::path& path) {
    if (getFileExtension(path) != ".png")
        return false;

    auto pngFile = std::make_unique<PNGFile>();
    auto pidFile = std::make_unique<PIDFile>(app);

    bool success = pngFile->loadFromFile(path);
    if (!success)
        return false;

    success = pngFile->indexToPalette(l_outPalette, l_outPaletteLUV);
    if (!success)
        return false;

    success = pidFile->getFromPNG(pngFile);
    if (!success)
        return false;

    pidFile->setFlag(PID_Flag_Transparency, l_flagsCb.transparency);
    pidFile->setFlag(PID_Flag_VideoMemory, l_flagsCb.videoMem);
    pidFile->setFlag(PID_Flag_SystemMemory, l_flagsCb.sysMem);
    pidFile->setFlag(PID_Flag_Compression, l_flagsCb.compression);
    pidFile->setFlag(PID_Flag_OwnPalette, l_flagsCb.savePalette);
    pidFile->shouldOptimize = l_flagsCb.optimization;
    pidFile->setFlag(PID_Flag_Never_Optimize, l_flagsCb.lockOptimization);
    pidFile->setFlag(PID_Flag_Grayscale, l_flagsCb.grayscale);

    l_pathOut.replace_extension(".pid");
    return pidFile->saveToFile(l_pathOut);
}