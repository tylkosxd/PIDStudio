#include "../PIDStudio.h"
#include "../formats/PIDFile.h"
#include "../formats/PIDPalette.h"
#include "GUI.h"
#include "../SupportedGame.h"
#include "../ImGuiExtensions.h"
#include "../String.h"
#include "../Filesystem.h"
#include "Dialogs.h"

#include "../AssetLibrary.h"
#include "../Project.h"

#include "../assets/IconsLucide.h"
#include "../assets/lucide.ttf.h"
#include "../assets/icon.png.h"

#include <tinyfiledialogs/tinyfiledialogs.h>
#include <libintl.h>
#include <SFML/Window/Event.hpp>
#include <imgui_internal.h>

#define _(String) gettext(String)

enum POPUP_STATE {
    KEEP_CLOSED,
    OPEN,
    KEEP_OPENED,
    CLOSE
};

POPUP_STATE l_createProjectPopup = KEEP_CLOSED;

POPUP_STATE l_renamePopup = KEEP_CLOSED;

POPUP_STATE l_addImagesPopup = KEEP_CLOSED;
std::shared_ptr<PIDPalette> l_addImagesPalette;
std::filesystem::path l_addImagesPath;

POPUP_STATE l_addFolderPopup = KEEP_CLOSED;

std::filesystem::path l_projectPath;

using namespace ImGui;

void offsetsWindow(const std::shared_ptr<PIDFile>& currentFile) {
    static int inputOffsetX, inputOffsetY;
    static int imageWidth, imageHeight;
    Begin(_("Offsets"));

    if (currentFile) {
        imageWidth = currentFile->width;
        imageHeight = currentFile->height;
        inputOffsetX = currentFile->offsetX;
        inputOffsetY = currentFile->offsetY;

        if (InputInt(_("Off. X"), &inputOffsetX, 1, 5)) {
            ImClamp(inputOffsetX, -imageWidth, imageWidth);
            currentFile->offsetX = inputOffsetX;
            currentFile->resetTexture();
        }

        if (InputInt(_("Off. Y"), &inputOffsetY, 1, 5)) {
            ImClamp(inputOffsetY, -imageHeight, imageHeight);
            currentFile->offsetY = inputOffsetY;
            currentFile->resetTexture();
        }

        if (Button(ICON_LC_ROTATE_CCW)) {
            currentFile->restoreOriginalOffsets();
            currentFile->resetTexture();
        }
        SameLine();
        Text("%s", _("Reset"));
    }
    else {
        Text("%s", _("No opened files."));
    }

    End();
}

void flagsWindow(const std::shared_ptr<PIDFile>& currentFile) {
    static bool cbTransparency, cbVideoMem, cbSysMem, cbCompression, cbOwnPalette;

    Begin(_("Flags"));

    if (currentFile) {

        int flags = currentFile->getFlags();
        cbTransparency = flags & PID_Flag_Transparency;
        cbVideoMem = flags & PID_Flag_VideoMemory;
        cbSysMem = flags & PID_Flag_SystemMemory;
        cbCompression = flags & PID_Flag_Compression;
        cbOwnPalette = flags & PID_Flag_OwnPalette;

        if (Checkbox(_("Use transparency"), &cbTransparency))
            currentFile -> setFlag(PID_Flag_Transparency, cbTransparency);

            /* the 2 below are muttualy exclusive */
        if (Checkbox(_("Use video memory"), &cbVideoMem)) {
            currentFile -> setFlag(PID_Flag_VideoMemory, cbVideoMem);
            if (cbVideoMem) 
                currentFile -> setFlag(PID_Flag_SystemMemory, false);
        }
        if (Checkbox(_("Use system memory"), &cbSysMem)) {
            currentFile -> setFlag(PID_Flag_SystemMemory, cbSysMem);
            if (cbSysMem)
                currentFile -> setFlag(PID_Flag_VideoMemory, false);
        }

        /* Mirror and Invert flags are not supported in Claw, so let's omit them */

        if (Checkbox(_("PID-RLE2 compression"), &cbCompression))
            currentFile -> setFlag(PID_Flag_Compression, cbCompression);

        if (Checkbox(_("Save with palette"), &cbOwnPalette))
            currentFile -> setFlag(PID_Flag_OwnPalette, cbOwnPalette);

        /* Grayscale flag is set in the palette dock */

        if (Button(ICON_LC_ROTATE_CCW))
            currentFile->restoreOriginalFlags();
        SameLine();
        Text("%s", _("Reset"));

    } else {
        Text("%s", _("No opened files."));
    }

    End();
}

void metadataWindow(const std::shared_ptr<PIDFile>& currentFile) {
    Begin(_("Data"));
    if (currentFile) {
        Text("%s: %dx%d\n%s: %d,%d\n",
            _("Size"),
            currentFile->width,
            currentFile->height,
            _("User values"),
            currentFile->userdata[0],
            currentFile->userdata[1]
        );
    } else {
        Text("%s", _("No opened files."));
    }
    End();
}

void paletteComboSelect(PIDStudio* app) {
    auto currentFile = app->currentFile;

    static std::string preview;
    if (currentFile && app->currentPalette == currentFile->getOwnPalette())
        preview = _("Own palette");
    else
        preview = app->currentPalette->getName();

    if (currentFile && currentFile -> isTransformedToPalette())
        BeginDisabled();

    if (BeginCombo(_("From"), preview.c_str(), ImGuiComboFlags_HeightLarge)) {

        for (const auto& palette : app->libPalettes) {
            const std::string& name = palette->getName();
            if (Selectable(name.c_str())) {
                if (currentFile && name == app->defaultPalette->getName()) {
                    currentFile->setFlag(PID_Flag_Grayscale, true);
                    currentFile->resetTexture();
                } else {
                    if (currentFile) {
                        currentFile->setFlag(PID_Flag_Grayscale, false);
                        currentFile->setPalette(palette);
                    }
                }
                app->currentPalette = palette;
            }
        }

        Separator();

        if (currentFile && currentFile->getOwnPalette()) {
            if (Selectable(_("Own palette"))) {
                currentFile->setPalette(currentFile->getOwnPalette());
                currentFile->setFlag(PID_Flag_Grayscale, false);
                app->currentPalette = currentFile->getOwnPalette();
            }
        }

        for (const auto& palette : app->customPalettes) {
            if (Selectable(palette->getName().c_str())) {
                if (currentFile) {
                    currentFile->setFlag(PID_Flag_Grayscale, false);
                    currentFile->setPalette(palette);
                }
                app->currentPalette = palette;
            }
        }

        Separator();

        if (Selectable(_("Load from file"))) 
            app->loadPaletteFromFile();

        EndCombo();
    }

    if (currentFile && currentFile -> isTransformedToPalette()) EndDisabled();

}

void paletteComboTransform(PIDStudio* app, std::shared_ptr<PIDPalette>& outPalette) {
    static std::string preview = " ";

    const auto& currentFile = app->currentFile;

    if (BeginCombo(_("To"), preview.c_str(), ImGuiComboFlags_HeightLarge)) {

        for (const auto& palette : app->libPalettes) {
            if (Selectable(palette->getName().c_str())) {
                outPalette = palette;
                preview = palette->getName();
            }
        }

        Separator();

        for (const auto& palette : app->customPalettes) {
            if (Selectable(palette->getName().c_str())) {
                outPalette = palette;
                preview = palette->getName();
            }
        }

        Separator();

        if (Selectable(_("Load from file")))
            app->loadPaletteFromFile();

        EndCombo();
    }
}

void paletteWindow(PIDStudio* app) {
    Begin(_("Palette"));

    auto palette = app->defaultPalette;
    static std::shared_ptr<PIDPalette> outPalette;
    const auto& currentFile = app->currentFile;
    const auto& currentPalette = app->currentPalette;

    bool noLightsFlag = !(currentFile && (currentFile -> getFlags() & PID_Flag_Grayscale));
    if (noLightsFlag)
        palette = currentPalette;

    if (!palette) {
        Text("%s", _("Couldn't find any matching palette."));
        End();
        return;
    }

    ImGuiEx::CenteredImageHorizontal(palette->getTexture());

    paletteComboSelect(app);

    if (!currentFile) 
        BeginDisabled();

    paletteComboTransform(app, outPalette);

    if (Button(ICON_LC_SHUFFLE)) {
        if (currentFile && outPalette) {
            currentFile -> transformToPalette(outPalette);
            app->currentPalette = outPalette;
        }
    }
    SameLine();
    Text("%s", _("Transform "));
    SameLine();
    if (Button(ICON_LC_ROTATE_CCW)) {
        if (currentFile) {
            currentFile->resetTransformationToPalette();
            app->currentPalette = currentFile -> getPalette();
        }
    }
    SameLine();
    Text("%s", _("Reset"));

    if (!currentFile)
        EndDisabled();

    if (ImGuiEx::BeginPopupForLastItem("Palette")) {
        if (MenuItem(_("Save to file"))) app->savePaletteToFile();
        EndPopup();
    }
    
    End();
}

void libraryWindow(PIDStudio* app) {
    ImGuiWindowFlags wndFlags = app->contextMenuOpened ? ImGuiWindowFlags_NoScrollWithMouse : 0;
    Begin(_("Library"), wndFlags);

    if ((app->assetLibraries).empty()) {
        const char* label = _("Add library");

        ImVec2 windowSize = GetWindowSize();
        ImVec2 labelSize = CalcTextSize(label);

        /* Centering the button */
        SetCursorPos(ImVec2(
            (windowSize.x - labelSize.x) * 0.5f,
            (windowSize.y - labelSize.y) * 0.5f + 15.0f
        ));

        if (Button(label))
            app->addLibrary();
    } else {
        for (const std::shared_ptr<AssetLibrary>& library : (app->assetLibraries))
            library->displayTree();
    }

    End();
}

void projectWindow(PIDStudio* app) {
    ImGuiWindowFlags wndFlags = app->contextMenuOpened ? ImGuiWindowFlags_NoScrollWithMouse : 0;
    Begin(_("Project"), wndFlags);

    if (!(app->currentProject)) {
        const char* label = _("Open/create project");

        ImVec2 windowSize = GetWindowSize();
        ImVec2 labelSize = CalcTextSize(label);

        /* Centering the button */
        SetCursorPos(ImVec2(
            (windowSize.x - labelSize.x) * 0.5f,
            (windowSize.y - labelSize.y) * 0.5f + 15.0f
        ));

        if (Button(label))
            UI::openProjectCreator(app);

    } else {
        (app->currentProject)->displayTree();
    }

    End();
}

void UI::menuBar(PIDStudio* app) {
    BeginMainMenuBar();

    if (BeginMenu(_("File"))) {

        if (ImGui::MenuItemEx(_("Open File..."), ICON_LC_IMAGE_UP, "Ctrl+O"))
            app->openImageFileDialog();

        if (MenuItemEx(_("Open/create project..."), ICON_LC_FOLDER_UP, nullptr))
            openProjectCreator(app);

        Separator();

        bool isAnyTabOpen = app->currentFile ? true : false;
        std::string nextshortenedPath = _("Close ");
        if (app->currentFile)
            nextshortenedPath += app->currentFile->getName();

        if (MenuItemEx(nextshortenedPath.c_str(), ICON_LC_X, "Ctrl+W", false, isAnyTabOpen)) 
            (app->filesToClose).insert(app->currentFile);

        if (MenuItemEx(_("Close all"), ICON_LC_COPY_X ,"Ctrl+Shift+W", false, isAnyTabOpen)) 
            app->closeAllFiles();

        Separator();

        if (MenuItemEx(_("Save"), ICON_LC_SAVE, "Ctrl+S", false, (currentFileModified(app))))
            app->saveOpenedFile(app->currentFile);
        
        if (MenuItemEx(_("Save as..."), ICON_LC_IMAGE_DOWN, "Ctrl+E"))
            app->saveCurrentFileAs();

        if (MenuItemEx(_("Save all"), ICON_LC_SAVE_ALL, "Ctrl+Shift+S", false, anyFileModified(app)))
            app->saveAllOpenedFiles();

        Separator();

        if (BeginMenuEx(_("Recently opened files"), ICON_LC_HISTORY, (app->settings).has("RecentFiles"))) {
            recentlyOpenedContextMenu(app);
            EndMenu();
        }

        Separator();

        if (MenuItemEx(_("Exit"), ICON_LC_LOG_OUT, "Alt+F4"))
            (app->mainWindow).close();

        EndMenu();
    }

    if (BeginMenu(_("Tools"))) {

        if (MenuItemEx(_("Batch"), ICON_LC_FILE_STACK, "Ctrl+B")) {
            app->openBatchCreator("");
        }

        if (MenuItemEx(_("Import PNGs"), ICON_LC_IMAGE_PLUS, "Ctrl+I")) {
            app->importPNGs("");
        }

        if (MenuItemEx(_("Palette manager"), ICON_LC_PALETTE, "Ctrl+P")) {
            app->openPaletteManager();
        }

        EndMenu();
    }

    EndMainMenuBar();
}

void UI::recentlyOpenedContextMenu(PIDStudio* app) {
    // for 10 last opened files
    for (int i = 0; i < 10; i++) {
        std::filesystem::path path = (app->settings)[RECENT_FILES_INI_KEY][std::to_string(i)];
        if (path.empty())
            continue;

        std::filesystem::path parentPath = path.parent_path();
        std::string shortenedPath = app->shortenFilePath(path);

        if (shortenedPath.empty())
            shortenedPath = std::to_string(i+1) + ". " + path.string();
        if (MenuItem(shortenedPath.c_str(), nullptr, false))
            app->openImageFile(path.string());
    }
}

void UI::toolBar(PIDStudio* app) {

    bool anyFileOpen = !!(app->currentFile);
    bool canSaveCurrent = currentFileModified(app);
    bool canSaveAll = anyFileModified(app);

    PushStyleColor(ImGuiCol_Button, 0);

    BeginMenuBar();

    if (Button(ICON_LC_IMAGE_UP))
        app->openImageFileDialog();
    ImGuiEx::ToolTip("Open...");

    if (!anyFileOpen)
        BeginDisabled();

    if (Button(ICON_LC_IMAGE_DOWN))
        app->saveCurrentFileAs();
    ImGuiEx::ToolTip(_("Save as..."));

    if (!canSaveCurrent)
        BeginDisabled();

    if (Button(ICON_LC_SAVE))
        app->saveOpenedFile(app->currentFile);
    ImGuiEx::ToolTip(_("Save"));

    if (!canSaveCurrent)
        EndDisabled();

    if (!canSaveAll)
        BeginDisabled();

    if (Button(ICON_LC_SAVE_ALL))
        app->saveAllOpenedFiles();
    ImGuiEx::ToolTip(_("Save all"));

    if (!canSaveAll)
        EndDisabled();

    if (Button(ICON_LC_COPY_X))
        app->closeAllFiles();
    ImGuiEx::ToolTip(_("Close all"));

    if (!anyFileOpen)
        EndDisabled();

    if (!app->keepAllFilesOpen) {
        if (Button(ICON_LC_PIN)) {
            app->keepAllFilesOpen = true;
            app->pinOpenedFile();
        }
        ImGuiEx::ToolTip(_("Keep all tabs open"));
    }
    else {
        if (Button(ICON_LC_PIN_OFF))
            app->keepAllFilesOpen = false;
        ImGuiEx::ToolTip(_("Don't keep all tabs open"));
    }
    
    EndMenuBar();

    PopStyleColor();
}

bool UI::anyFileModified(PIDStudio* app) {
    if (!(app->currentFile))
        return false;

    for (const auto& file : (app->openedFiles)) {
        if (file -> isModified()) 
            return true;
    }

    return currentFileModified(app);
}

bool UI::currentFileModified(PIDStudio* app) {
    if (!(app->currentFile))
        return false;
    return app->currentFile->isModified();
}

void UI::preDockedWindows(PIDStudio* app) {
    static bool shouldPrepareDockspace = true; // pre-dock windows on launch, but allow user to move them freely later
    static ImGuiID dockspaceIDLeft{},
        dockspaceIDRight{},
        dockspaceIDRightTop{},
        dockspaceIDRightBottom{};

    app->dockspaceId = DockSpace(GetID(APPLICATION_NAME));

    if (shouldPrepareDockspace) {
        DockBuilderRemoveNode(app->dockspaceId);
        DockBuilderAddNode(
            app->dockspaceId,
            ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_NoResizeX | ImGuiDockNodeFlags_NoResizeY
        );
        DockBuilderSetNodeSize(app->dockspaceId, GetWindowSize());

        // split initial UI layout to specific dockable areas
        dockspaceIDRight = DockBuilderSplitNode(app->dockspaceId, ImGuiDir_Right, 0.25f, nullptr, &dockspaceIDLeft);
        dockspaceIDRightTop = DockBuilderSplitNode(dockspaceIDRight, ImGuiDir_Up, 0.33f, nullptr, &dockspaceIDRightBottom);

        SetNextWindowDockID(dockspaceIDRightTop, ImGuiDir_Up);
    }
    paletteWindow(app);

    if (shouldPrepareDockspace) {
        SetNextWindowDockID(dockspaceIDRightTop, ImGuiDir_Up);
    }
    offsetsWindow(app->currentFile);

    if (shouldPrepareDockspace) {
        SetNextWindowDockID(dockspaceIDRightTop, ImGuiDir_Up);
    }
    flagsWindow(app->currentFile);

    if (shouldPrepareDockspace) {
        SetNextWindowDockID(dockspaceIDRightTop, ImGuiDir_Up);
    }
    metadataWindow(app->currentFile);

    if (shouldPrepareDockspace) {
        SetNextWindowDockID(dockspaceIDRightBottom, ImGuiDir_Down);
    }
    projectWindow(app);
    
    if (shouldPrepareDockspace) {
        SetNextWindowDockID(dockspaceIDRightBottom, ImGuiDir_Down);
    }
    libraryWindow(app);

    if (shouldPrepareDockspace) {
        shouldPrepareDockspace = false;
        DockBuilderFinish(app->dockspaceId);
    }
}

void UI::libraryEntryContextMenu(
    PIDStudio* app,
    const std::shared_ptr<AssetLibrary>& library,
    const std::shared_ptr<TreeNodeBase>& node
) {
    if (!(node->isRoot) && !(node->parent->isRoot) && !(node->parent->parent->isRoot) && !(node->isLeaf)){
        if (MenuItem(_("Open all")))
            app->openAllImageFiles(node->path, library->inferPalette(node));
        Separator();
        bool projectIsClosed = !app->currentProject;
        if (MenuItem(_("Add to current project"), nullptr, false, !projectIsClosed)) {
            l_addImagesPalette = library->inferPalette(node);
            l_addImagesPath = node->path;
            l_addImagesPopup = OPEN;
        }
        Separator();
    }
    if (!node->isLeaf)
        if (MenuItem(_("Export to PNG")))
            app->saveAllFilesAs(node->path, library->inferPalette(node), ".png");

}

void UI::projectEntryContextMenu(
    PIDStudio* app,
    const std::shared_ptr<Project>& project,
    const std::shared_ptr<TreeNodeBase>& node
) {
    if (node->isRoot) {
        if (MenuItem(_("Close project")))
            app->projectToClose = project;
        if (BeginMenu(_("Set palette..."))) {
            for (const auto& palette : app->libPalettes) {
                if (MenuItem(palette->getName().c_str()))
                    project->setRootPalette(palette);
            }
            Separator();
            for (const auto& palette : app->customPalettes) {
                if (MenuItem(palette->getName().c_str()))
                    project->setRootPalette(palette);
            }
            EndMenu();
        }
    }
    else {
        if (MenuItem(_("Batch")))
            app->openBatchCreator(node->path);
        Separator();
        if (MenuItem(_("Export to PNGs")))
            app->saveAllFilesAs(node->path, app->currentProject->palette, ".png");
        if (MenuItem(_("Import PNGs")))
            app->importPNGs(node->path);
    }
    if (!node->isRoot || !node->isLeaf) 
        Separator();
    if (!node->isRoot) {
        if (MenuItem(_("Copy")))
            app->fileClipboard = {node->path, false};
        if (MenuItem(_("Cut")))
            app->fileClipboard = {node->path, true};
    }
    if (!node->isLeaf) {
        bool clipboardEmpty = app->fileClipboard.first.empty();
        if (clipboardEmpty)
            BeginDisabled();
        if (MenuItem(_("Paste")))
            app->pasteFileFromClipboard(node->path);
        if (clipboardEmpty)
            EndDisabled();
        Separator();
        if (MenuItem(_("New folder...")))
            l_addFolderPopup = OPEN;
        Separator();
        if (MenuItem(_("Open all")))
            app->openAllImageFiles(node->path, project->inferPalette(node));
    }
    if (!node->isRoot) {
        Separator();
        if (MenuItem(_("Rename...")))
            l_renamePopup = OPEN;
        if (MenuItem(_("Delete")))
            project->deleteNode(node);
    }
}

void UI::hotkeyPress(PIDStudio* app, sf::Event event) {
    if (event.key.control) {
        auto currentFile = app->currentFile;

        switch (event.key.code) {
        case sf::Keyboard::O:
            /* CTRL + O */
            app->openImageFileDialog();
            break;
        case sf::Keyboard::W:
            /* CTRL + W */
            if (currentFile && !event.key.shift)
                (app->filesToClose).insert(currentFile);
            /* CTRL + SHIFT + W */
            if (event.key.shift)
                app->closeAllFiles();
            break;
        case sf::Keyboard::S:
            /* CTRL + S */
            if (currentFileModified(app) && !event.key.shift)
                app->saveOpenedFile(currentFile);
            /* CTRL + SHIFT + S */
            if (anyFileModified(app) && event.key.shift)
                app->saveAllOpenedFiles();
            break;
        case sf::Keyboard::E:
            /* CTRL + E (Export)*/
            if (currentFile)
                app->saveCurrentFileAs();
            break;
        case sf::Keyboard::B:
            /* CTRL + B */
            app->openBatchCreator("");
            break;
        case sf::Keyboard::I:
            /* CTRL + I */
            app->importPNGs("");
            break;
        case sf::Keyboard::P:
            /* CTRL + P */
            app->openPaletteManager();
            break;
        default:
            break;
        }
    }
}

void UI::newFolderPopup(PIDStudio* app) {
    static char* inputLabel;
    static char inputBuffer[256] = {0};
    static std::filesystem::path filename;

    switch (l_addFolderPopup) {

    case KEEP_CLOSED:
        return;
    
    case OPEN:
        filename = app->lastOpenedTreeNode.name;
        memset(inputBuffer, 0, 256);
        app->lastOpenedTreeNode.pos.y += 16.0f;
        SetNextWindowPos(app->lastOpenedTreeNode.pos);
        SetNextWindowSize(app->lastOpenedTreeNode.size);
        OpenPopup(_("New folder"));
        l_addFolderPopup = KEEP_OPENED;
    // fall-through

    case KEEP_OPENED:
        if (BeginPopup(_("New folder"), ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            Text("%s", _("New folder"));
            SetKeyboardFocusHere();
            if (InputText(_(""), inputBuffer, 255, ImGuiInputTextFlags_EnterReturnsTrue)) {
                CloseCurrentPopup();
                l_addFolderPopup = CLOSE;
            }
            if (IsKeyPressed(ImGuiKey_Escape)) {
                CloseCurrentPopup();
                l_addFolderPopup = KEEP_CLOSED;
            }
            EndPopup();
        }

        if (!IsPopupOpen(_("New folder")))
            l_addFolderPopup = CLOSE;
        break;

    case CLOSE: {
        std::filesystem::path newFilename = inputBuffer;
        std::filesystem::path newFilenameFull = app->lastOpenedTreeNode.path / newFilename;
        
        if (!newFilename.empty() && newFilename != filename) {
            if (std::filesystem::exists(newFilenameFull))
                UI::errorMessageBox(ERROR_FILE_ALREADY_EXISTS);
            else if (!isValidFileName(newFilename))
                UI::errorMessageBox(ERROR_INVALID_FILENAME);
            else
                std::filesystem::create_directory(newFilenameFull);
        }
        l_addFolderPopup = KEEP_CLOSED;
        break;
    }
    }
}

void UI::renameNodePopup(PIDStudio* app) {
    static char* inputLabel;
    static char inputBuffer[256] = {0};
    static std::filesystem::path filename, extension;

    switch (l_renamePopup) {

    case KEEP_CLOSED:
        return;
    
    case OPEN:
        filename = app->lastOpenedTreeNode.name;
        extension = filename.extension();
        filename.replace_extension("");
        memset(inputBuffer, 0, 256);
        copyCharArray(inputBuffer, filename.string(), 255);
        SetNextWindowPos(app->lastOpenedTreeNode.pos);
        SetNextWindowSize(app->lastOpenedTreeNode.size);
        OpenPopup(_("Rename node"));
        l_renamePopup = KEEP_OPENED;
    // fall-through

    case KEEP_OPENED:
        if (BeginPopup(_("Rename node"), ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            SetKeyboardFocusHere();
            if (InputText(extension.string().c_str(), inputBuffer, 255, ImGuiInputTextFlags_EnterReturnsTrue)) {
                CloseCurrentPopup();
                l_renamePopup = CLOSE;
            }
            if (IsKeyPressed(ImGuiKey_Escape)) {
                CloseCurrentPopup();
                l_renamePopup = KEEP_CLOSED;
            }
            EndPopup();
        }

        if (!IsPopupOpen(_("Rename node")))
            l_renamePopup = CLOSE;
        break;

    case CLOSE: {
        std::filesystem::path newFilename = inputBuffer;
        std::filesystem::path newFilenameFull = app->lastOpenedTreeNode.path.parent_path() / newFilename;
        
        if (!newFilename.empty() && newFilename != filename) {
            if (std::filesystem::exists(newFilenameFull)) {
                UI::errorMessageBox(ERROR_FILE_ALREADY_EXISTS);
            }
            else if (!isValidFileName(newFilename)) {
                UI::errorMessageBox(ERROR_INVALID_FILENAME);
            }
            else {
                newFilename.replace_extension(extension); // bring back the file extension
                std::filesystem::rename(
                    app->lastOpenedTreeNode.path,
                    newFilenameFull
                );
            }
        }
        l_renamePopup = KEEP_CLOSED;
        break;
    }
    }
}

void UI::openProjectCreator(PIDStudio* app) {
    auto selectedFolder = tinyfd_selectFolderDialog(_("Select folder"), nullptr);

    if (!selectedFolder) return;

    namespace fs = std::filesystem;
    fs::path path = selectedFolder;
    fs::path projectIniPath = path / PROJECT_INI_FOLDER;
    fs::path projectIniFile = projectIniPath / PROJECT_INI_FILENAME;

    /* open project if the project's ini file exists */
    if (fs::is_directory(projectIniPath) && fs::is_regular_file(projectIniFile)) {
        app->openProject(path);
    }
    /* else open project creator popup */
    else {
        l_createProjectPopup = OPEN;
        l_projectPath = path;
    }
}

void UI::projectCreator(PIDStudio* app) {

    static float width = 320.0f, height = 120.0f;
    static char newProjectName[64];
    static std::shared_ptr<PIDPalette> newProjectPalette = app->defaultPalette;
    static std::string palettePreview = app->defaultPalette->getName();

    switch (l_createProjectPopup) {

    case KEEP_CLOSED:
        return;
    
    case OPEN:
        copyCharArray(newProjectName, l_projectPath.filename().string(), 64);
        ImGuiEx::CenterNextWindow(width, height);
        OpenPopup(_("Create project"));
        l_createProjectPopup = KEEP_OPENED;
        // fall through

    case KEEP_OPENED:
        if (BeginPopupModal(_("Create project"), nullptr, ImGuiWindowFlags_NoResize)) {

            InputText(_("Name"), newProjectName, 64);

            if (BeginCombo(_("Default palette"), palettePreview.c_str())) {

                if (Selectable(_("Load from file")))
                    app->loadPaletteFromFile();

                Separator();

                for (const auto& palette : app->libPalettes) {
                    if (Selectable(palette->getName().c_str())) {
                        newProjectPalette = palette;
                        palettePreview = palette->getName();
                    }
                }

                Separator();

                for (const auto& palette : app->customPalettes) {
                    if (Selectable(palette->getName().c_str())) {
                        newProjectPalette = palette;
                        palettePreview = palette->getName();
                    }
                }
                EndCombo();
            }

            if (Button(_("Cancel"))) {
                l_createProjectPopup = KEEP_CLOSED;
                CloseCurrentPopup();
            }

            SameLine();

            if (!newProjectPalette || newProjectName[0] == 0)
                BeginDisabled();

            if (ButtonEx(_("Create"))) {
                l_createProjectPopup = CLOSE;
            }

            if (!newProjectPalette || newProjectName[0] == 0)
                EndDisabled();

            EndPopup();
        }
        break;

    case CLOSE:
        app->createProject(
            newProjectName,
            l_projectPath,
            newProjectPalette
        );
        CloseCurrentPopup();
        l_createProjectPopup = KEEP_CLOSED;
        break;

    }
}

void UI::addImagesToProjectDialog(PIDStudio* app) {
    static float width = 240.0f, height = 200.0f;
    char comboBuffer[64];
    const char* addOptions[5] = {
        _("Add to IMAGES folder"),
        _("Add to LEVEL folder"),
        _("Add as tiles to ACTION"),
        _("Add as tiles to BACK"),
        _("Add as tiles to FRONT")
    };
    ADD_IMAGES_OPTION selectedOption;

    switch (l_addImagesPopup) {

    case KEEP_CLOSED:
        return;
    
    case OPEN:
        l_createProjectPopup = KEEP_OPENED;
        ImGuiEx::CenterNextWindow(width, height);
        OpenPopup(_("Add images"));
        // fall through

    case KEEP_OPENED:
        if (BeginPopupModal(_("Add images"), nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {

            for (int option = 0; option < 5; option++) {
                if (Button(addOptions[option])) {
                    app->addImagesToProject(l_addImagesPath, l_addImagesPalette, (ADD_IMAGES_OPTION)option);
                    l_addImagesPopup = KEEP_CLOSED;
                    CloseCurrentPopup();
                }
            }

            if (Button(_("Cancel"))) {
                l_addImagesPopup = KEEP_CLOSED;
                CloseCurrentPopup();
            }

            EndPopup();
        }
        break;
    }
}