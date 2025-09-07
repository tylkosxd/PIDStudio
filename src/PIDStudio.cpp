#include "PIDStudio.h"

#include "gui/GUI.h"
#include "gui/Dialogs.h"
#include "Batch.h"
#include "String.h"
#include "Filesystem.h"
#include "PaletteManager.h"

#include "AssetLibrary.h"
#include "Project.h"

#include "formats/PIDFile.h"
#include "formats/PIDPalette.h"
#include "formats/PCXFile.h"
#include "formats/BMPFile.h"
#include "formats/PNGFile.h"

#include "games/Claw.h"
#include "games/Gruntz.h"
#include "games/GetMedieval.h"

#include <stack>
#include <array>
#include <libintl.h>

#include "ImGuiExtensions.h"
#include <imgui-SFML.h>
#include <SFML/Graphics.hpp>
#include "imgui_internal.h"

#include "assets/icon.png.h"
#include "assets/font.ttf.h"
#include "assets/grayscale.pal.h"

#include "assets/IconsLucide.h"
#include "assets/lucide.ttf.h"

#define _(String) gettext(String)
#define _STRINGS_TO_TRANSLATE_ _("Claw") _("Gruntz") _("Get Medieval")

#define SCREEN_WIDTH 1200
#define SCREEN_HEIGHT 900

PIDStudio::PIDStudio() : mainWindow(sf::VideoMode(SCREEN_WIDTH, SCREEN_HEIGHT), APPLICATION_NAME) {
    // initialize localization
    setlocale(LC_ALL, "");
    bindtextdomain(APPLICATION_NAME, "locale");
    textdomain(APPLICATION_NAME);
    bind_textdomain_codeset(APPLICATION_NAME, "UTF-8");

    // initialize fallback palette
    defaultPalette = std::make_shared<PIDPalette>(GRAYSCALE_PAL);
    defaultPalette->setName("Grayscale");
    libPalettes.emplace_back(defaultPalette);
    currentPalette = defaultPalette;

    // list supported games
    claw = std::make_shared<Claw>(this, "Claw", "CLAW", "claw.exe");
    getMedieval = std::make_shared<GetMedieval>(this, "Get Medieval", "MEDIEVAL", "medieval.exe");
    gruntz = std::make_shared<Gruntz>(this, "Gruntz", "GRUNTZ", "gruntz.exe");
    supportedGames.push_back(claw);
    supportedGames.push_back(getMedieval);
    supportedGames.push_back(gruntz);

    // initialize window with a custom icon
	sf::Image applicationIcon;
	applicationIcon.loadFromMemory(ICON_PNG, sizeof(ICON_PNG));
	mainWindow.setIcon(ICON_PNG_WIDTH, ICON_PNG_HEIGHT, applicationIcon.getPixelsPtr());
	mainWindow.setFramerateLimit(60);

	ImGui::SFML::Init(mainWindow, false);

    // configure UI library styles and flags
    ImGuiStyle* style = &ImGui::GetStyle();
    style->WindowMenuButtonPosition = ImGuiDir_None;

    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigDockingWithShift = true;
    io.IniFilename = "PIDStudio-ImGui.ini";

    // change colors
    for (int col = 0; col < ImGuiCol_COUNT; col++) {
        
        float red = style->Colors[col].x;
        float green = style->Colors[col].y;
        float blue = style->Colors[col].z;

        // skip grey shades
        if (red == green && green == blue)
            continue;

        // swap blue and red
        style->Colors[col].x = blue;
        style->Colors[col].z = red;
        red = style->Colors[col].x;
        blue = style->Colors[col].z;
        
        // increase red
        style->Colors[col].x += 0.1f;
        ImClamp(style->Colors[col].x, 0.0f, 1.0f);
        // reduce green
        style->Colors[col].y -= 0.15f;
        ImClamp(style->Colors[col].y, 0.0f, 1.0f);
        // reduce blue
        style->Colors[col].z -= 0.15f;
        ImClamp(style->Colors[col].z, 0.0f, 1.0f);
    }

    style->Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);

    // load default font
    float baseFontSize = 16.0f;
    float iconFontSize = 14.0f;

    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    static const ImWchar fontRanges[] =
    {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x0100, 0x017F, // Extended Latin A
        0,
    };
    io.Fonts->AddFontFromMemoryTTF(
        (void*)FONT_TTF,
        FONT_TTF_SIZE,
        baseFontSize,
        &fontConfig,
        fontRanges
    );

    // load icons font, merging to default font
    ImFontConfig iconsConfig;
    iconsConfig.FontDataOwnedByAtlas = false;
    iconsConfig.MergeMode = true;
    iconsConfig.PixelSnapH = true;
    iconsConfig.GlyphOffset.y = 1;
    static const ImWchar iconsRanges[] = { ICON_MIN_LC, ICON_MAX_LC, 0 };
    io.Fonts->AddFontFromMemoryTTF(
        (void*)LUCIDE_TTF,
        LUCIDE_TTF_SIZE,
        iconFontSize,
        &iconsConfig,
        iconsRanges
    );

    // finalize loading fonts
    io.Fonts->Build();
    ImGui::SFML::UpdateFontTexture();

    // load settings from ini file
    mINI::INIFile file(SETTINGS_INI_FILENAME);
    file.read(settings);

    namespace fs = std::filesystem;
    fs::path currentPath = fs::current_path();
    fs::path assetsPath = currentPath / "Assets";

    // open libraries
    if (!fs::exists(assetsPath)) {
        // open libraries from paths saved in PIDStudio.ini
        for (auto const& library : settings[ASSET_LIBRARIES_INI_KEY]) {
            for (auto const& game : supportedGames) {
                if (library.first == game->getIniKey()) {
                    assetLibraries.emplace_back(
                        std::make_shared<AssetLibrary>(this, library.second.c_str(), game)
                    );
                    break;
                }
            }
        }
    }
    else {
        // open libraries from PIDStudio's Assets folder
        for (auto const& path : fs::directory_iterator(assetsPath)) {
            std::string folderName = path.path().filename().string();
            for (auto const& game : supportedGames) {
                if (folderName == game->getName()) {
                    assetLibraries.emplace_back(
                        std::make_shared<AssetLibrary>(this, path.path(), game)
                    );
                    break;
                }
            }
        }
        // open library from Assets folder, if PIDStudio.exe is in the game's folder
        if (assetLibraries.empty()) {
            for (auto const& path : fs::directory_iterator(currentPath)) {
                std::string name = path.path().filename().string();
                std::transform(name.begin(), name.end(), name.begin(), charToLower);
                for (auto const& game : supportedGames) {
                    if (name == game->getExeName()) {
                        assetLibraries.emplace_back(
                            std::make_shared<AssetLibrary>(this, assetsPath, game)
                        );
                        break;
                    }
                }
            }
        }
    }

    // open last project
    fs::path projectPath = settings[LAST_PROJECT_INI_KEY].get("0");

    if (!projectPath.empty()) {
        fs::path iniFile = projectPath / PROJECT_INI_FOLDER / PROJECT_INI_FILENAME;
        if (fs::is_directory(iniFile.parent_path()) && fs::is_regular_file(iniFile))
            openProject(projectPath);
    }
}

PIDStudio::~PIDStudio(){
    // save settings to ini files
    mINI::INIFile file(SETTINGS_INI_FILENAME);
    file.write(settings);

    if (currentProject)
        currentProject->updateINI();

    // properly close all UI stuff
	ImGui::SFML::Shutdown();
	mainWindow.close();
}

int PIDStudio::run() {
	sf::Clock deltaClock{};
    sf::Event event{};

    while (mainWindow.isOpen()) {
        // process window events
        while (mainWindow.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(mainWindow, event);

            switch (event.type) {
            case sf::Event::Closed:
                mainWindow.close();
                break;
            case sf::Event::KeyPressed:
                UI::hotkeyPress(this, event);
                break;
            default:
                break;
            }
        }

        // main UI loop
        ImGui::SFML::Update(mainWindow, deltaClock.restart());

        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x, main_viewport->WorkPos.y));
        ImGui::SetNextWindowSize(ImVec2(main_viewport->WorkSize.x, main_viewport->WorkSize.y));
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::Begin(
            APPLICATION_NAME, (
                ImGuiWindowFlags_MenuBar |
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoDocking
            )
        );

        {
            ImGui::PopStyleVar();

            UI::menuBar(this);
            UI::toolBar(this);
            UI::preDockedWindows(this);
            openedFilesWindows();

            UI::newFolderPopup(this);
            UI::renameNodePopup(this);
            UI::projectCreator(this);
            UI::addImagesToProjectDialog(this);

            Batch::batch(this);
            Batch::creator(this);
            Batch::importPNGSSettings(this);
            
            PaletteMgr::manager(this);

            if (!lastOpenedTreeNode.path.empty())
                contextMenuOpened = ImGui::IsPopupOpen(lastOpenedTreeNode.name.data(), ImGuiPopupFlags_AnyPopup);
        }

        ImGui::End();

        mainWindow.clear();
        ImGui::SFML::Render(mainWindow);
        mainWindow.display();

        // update child windows undocked from main window
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();

        if (libraryToClose) {
            settings[ASSET_LIBRARIES_INI_KEY].remove(libraryToClose->getIniKey());
            assetLibraries.erase(std::find(assetLibraries.begin(), assetLibraries.end(), libraryToClose));
            libraryToClose.reset();
        }

        if (projectToClose) {
            settings[LAST_PROJECT_INI_KEY].remove("0");
            //projects.erase(std::find(projects.begin(), projects.end(), projectToClose));
            currentProject.reset();
            projectToClose.reset();
        }

    }

    return 0;
}

void PIDStudio::openedFilesWindows() {

    if (!filesToClose.empty()) {
        for (const auto& file : filesToClose)
            closeFile(file);
        filesToClose.clear();
    }

    for (const auto& file : openedFiles)
        openedFileWindow(file);

    if (openedFile)
        openedFileWindow(openedFile);
}

void PIDStudio::openedFileWindow(const std::shared_ptr<PIDFile>& file) {

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
    if (file->isModified())
        flags |= ImGuiWindowFlags_UnsavedDocument;

    bool didNotClose = true;
    bool clickedPin = false;

    ImGui::Begin(file->getWindowName().c_str(), flags, &didNotClose);

    ImGuiWindow* window = ImGui::GetCurrentWindow();

    if (file->justOpened) {
        ImGui::SetWindowDock(window, dockspaceId, ImGuiDir_Right);
        file->justOpened = false;
    }

    if (ImGui::IsWindowFocused()) {
        currentFile = file;
        currentPalette = file->getPalette();
    }

    if (ImGui::IsWindowDocked())
        // This fixes windows receiving input after clicking off external modal, which resulted in wrong window being focused.
        ImGui::GetCurrentWindow()->Flags |= ImGuiWindowFlags_NoMouseInputs;

    ImGuiEx::CenteredImage(file->getTexture());
    
    ImGui::End();

    if (file.get() == bringFocusTo) {
        if (ImGuiEx::BringFocusTo(window)) {
            bringFocusTo = nullptr;
            currentPalette = file->getPalette();
            currentFile = file;
        }
    }

    if (!didNotClose)
        filesToClose.insert(file); 
}

void PIDStudio::addLibrary() {
    const char* selectedFolder = UI::selectFolderDialog();
    if (!selectedFolder)
        return;

    namespace fs = std::filesystem;

    fs::path path = selectedFolder;

    const std::string& clawExeName = claw->getExeName();

    bool clawExeExists = fileExists(path, clawExeName);

    if (!clawExeExists) {
        path /= "..";
        clawExeExists = fileExists(path, clawExeName);
    }

    if (clawExeExists) {
        path /= "Assets"; 
    }
    else {
        UI::errorMessageBox(ERROR_UNSUPPORTED_GAME);
        return;
    }

    if (!fs::exists(path)) {
        UI::errorMessageBox(ERROR_OLD_CLAW_VERSION);
        return;
    }

    settings[ASSET_LIBRARIES_INI_KEY][claw->getIniKey()] = path.string();

    auto assetLibrary = std::make_shared<AssetLibrary>(this, path, claw);
    assetLibraries.emplace_back(assetLibrary);

    // check if we can infer palette for one of already opened files from the newly added library
    for (const auto& file : openedFiles) {
        if (file->getPalette()) continue;

        std::shared_ptr<TreeNodeBase> outFoundNode;
        if (assetLibrary->hasFilepath(file->getPath(), outFoundNode)) {
            const auto& palette = assetLibrary->inferPalette(outFoundNode);
            if (!palette) continue;
            file->setPalette(palette);
            if (file == currentFile)
                currentPalette = palette;
        }
    }
}

void PIDStudio::pasteFileFromClipboard(const std::filesystem::path& dstPath) {

    auto& srcPath = fileClipboard.first;

    if (srcPath.empty())
        return;

    namespace fs = std::filesystem;
    
    if (!fs::exists(srcPath)) {
        fileClipboard.first.clear();
        UI::errorMessageBox(ERROR_PASTE_NO_SOURCE);
        return;
    }

    if (!fs::exists(dstPath))
        fs::create_directories(dstPath);

    if (!fs::exists(dstPath)) {
        UI::errorMessageBox(ERROR_PASTE_NO_DESTINATION);
        return;
    }

    fs::path _dstPath = dstPath / srcPath.filename();

    if (fs::exists(_dstPath)) {
        if (!UI::warningMessageBox(WARNING_ASK_TO_OVERWRITE, _dstPath.string()))
            return;
    }
    
    static const auto copyOptions = fs::copy_options::recursive | fs::copy_options::overwrite_existing;

    if (fs::is_directory(srcPath)) {
        fs::create_directories(_dstPath);
        fs::copy(srcPath, _dstPath, copyOptions);
    }
    else if (fs::is_regular_file(srcPath)) {
        fs::copy_file(srcPath, dstPath, copyOptions);
    }

    if (!fs::exists(_dstPath)) {
        UI::errorMessageBox(ERROR_PASTE_UNKNOWN);
        return;
    }

    bool isCut = fileClipboard.second;
    if (isCut) {
        if (fs::is_directory(srcPath))
            fs::remove_all(fileClipboard.first);
        else
            fs::remove(fileClipboard.first);
    }

    fileClipboard.first.clear();
}

void addTilesToProject(
    const std::filesystem::path& pathIn,
    const std::filesystem::path& projectPath,
    const std::filesystem::path& projectFolder,
    std::vector<std::filesystem::path>& files
) {
    namespace fs = std::filesystem;

    bool pathOutExisted = true;
    fs::path pathOut = projectPath / (fs::path)"TILES" / projectFolder;
    if (!fs::exists(pathOut)) {
        fs::create_directories(pathOut);
        pathOutExisted = false;
    }

    // check the last tile in the out directory (the one named with the largest number):
    int lastTile = 1000; // this number ensures the new tiles won't overlap with the original tiles in Claw assets
    if (pathOutExisted) {
        for (const auto& entry : fs::directory_iterator(pathOut)) {
            const int tileNumber = std::stoi(entry.path().filename().string(), nullptr);
            if (tileNumber && tileNumber > lastTile)
                lastTile = tileNumber;
        }
    }

    // let's copy the new tiles, while giving them a new number, then save the path to each tile to a vector for the batch palette transformation
    for (const auto& entry : fs::directory_iterator(pathIn)) {
        std::string ext = getFileExtension((fs::path)entry);
        if (ext != ".pid" && ext != ".bmp" && ext != ".pcx") {
            continue;
        }
        lastTile++;
        fs::path filePath = pathOut/(fs::path)(std::to_string(lastTile) + ".pid");
        fs::copy(entry.path(), filePath);
        files.push_back(filePath);
    }
}

void addImagesetToProject(
    const std::filesystem::path& pathIn,
    const std::filesystem::path& projectPath,
    const std::filesystem::path& projectFolder,
    std::filesystem::path& pathOut
) {
    /* add number suffix to the out directory if necessary */
    namespace fs = std::filesystem;

    pathOut = projectPath / projectFolder / pathIn.filename();

    bool dirExists = true;
    int suffix = 2; /* the potential suffix starts from 2 */
    fs::path pathOutBase = pathOut;
    while (dirExists) {
        if (fs::is_directory(pathOut)) {
            std::string pathStr = pathOutBase.string();
            pathOut = (fs::path)(pathStr + std::to_string(suffix));
            suffix++;
        } else dirExists = false;
    }

    fs::create_directories(pathOut);

    static const auto copyOptions = fs::copy_options::skip_existing | fs::copy_options::recursive;
    fs::copy(pathIn, pathOut, copyOptions);
}

void PIDStudio::addImagesToProject(
    const std::filesystem::path& pathIn,
    const std::shared_ptr<PIDPalette>& paletteIn,
    ADD_IMAGES_OPTION addOption
) {
    namespace fs = std::filesystem;

    Batch::setInPalette(paletteIn);
    Batch::setOutPalette(currentProject->palette);

    if (addOption == ADD_TO_IMAGES || addOption == ADD_TO_LEVEL) {
        fs::path pathOut;
        fs::path folderOut = addOption == ADD_TO_IMAGES ? "IMAGES" : "LEVEL";

        addImagesetToProject(pathIn, currentProject->getPath(), folderOut, pathOut);

        Batch::start(pathOut, BATCH_TRANSFORM_PALETTE);
    }

    if (addOption == ADD_TILES_ACTION || addOption == ADD_TILES_BACK || addOption == ADD_TILES_FRONT) {
        std::vector<fs::path> filesOut;
        filesOut.reserve(1000);

        fs::path folderOut = addOption == ADD_TILES_FRONT ? "FRONT" : addOption == ADD_TILES_BACK ? "BACK" : "ACTION";

        addTilesToProject(pathIn, currentProject->getPath(), folderOut, filesOut);

        Batch::start(filesOut, BATCH_TRANSFORM_PALETTE);
    }

    currentProject->rebuildTree();
}

void PIDStudio::openProject(const std::filesystem::path& path) {
    settings[LAST_PROJECT_INI_KEY]["0"] = path.string();
    auto project = std::make_shared<Project>(this, path);
    currentProject = project;
}

void PIDStudio::createProject(
    const char* name,
    std::filesystem::path& path,
    std::shared_ptr<PIDPalette>& palette
) {
    namespace fs = std::filesystem;

    fs::path projectIniPath = path / PROJECT_INI_FOLDER;
    fs::path projectIniFile = projectIniPath / PROJECT_INI_FILENAME;
    
    auto project = std::make_shared<Project>(this, path);

    project->name = name;
    
    settings[LAST_PROJECT_INI_KEY]["0"] = path.string();

    fs::create_directory(projectIniPath);

    project->settings["Name"]["0"] = name;
    project->settings["Palette"]["Root"] = palette->getName();
    
    palette->saveToFile(projectIniPath / PROJECT_PAL_FILENAME);
    project->palette = palette;

    project->updateINI();

    currentProject = project;
}

void PIDStudio::openBatchCreator(const std::filesystem::path& path) {
    Batch::openCreator(this, path);
}

void PIDStudio::openPaletteManager() {
    PaletteMgr::openManager(this);
}

void PIDStudio::mapPalette(
    const std::filesystem::path& path,
    const std::string& libraryName,
    const std::shared_ptr<PIDPalette>& palette,
    bool isNotFromLibrary
) {
    std::string name = libraryName;

    if (!name.empty())
        name += " - ";

    if (path.filename().string() != "MAIN.PAL") {
        name += path.filename().string();
    }
    else {
        std::filesystem::path pathStr = path.parent_path();
        if (!pathStr.empty()) {
            name += pathStr.parent_path().filename().string();
        }
        else {
            name += "Unknown";
        }
    }
    
    if (isNotFromLibrary) {
        int counter = 1;
        std::string finalName = name;
        while (getPalette(finalName))
            finalName = name + " #" + std::to_string(++counter);
        palette->setName(finalName);
        customPalettes.emplace_back(palette);
    }
    else {
        palette->setName(name);
        libPalettes.emplace_back(palette);
    }
}

std::shared_ptr<PIDPalette> PIDStudio::getPalette(const std::string& name) {
    for (const auto& palette : libPalettes) {
        if (palette->getName() == name) {
            return palette;
        }
    }
    for (const auto& palette : customPalettes) {
        if (palette->getName() == name) {
            return palette;
        }
    }
    return nullptr;
}

void PIDStudio::openImageFileDialog() {
    static const char* filters[3] = {"*.pid", "*.bmp", "*.pcx"};
    static size_t filtersSize = 3;
    static bool multiSelect = true;
    const char* selectedFiles = UI::openFileDialog(filters, filtersSize, _("Palette-based images (*.pid, *.bmp, *.pcx)"));

    if (!selectedFiles) return;

    std::istringstream stream(selectedFiles);
    std::string filePath;

    while (std::getline(stream, filePath, '|')) {
        openImageFile((std::filesystem::path)filePath);
    }
}

void PIDStudio::saveOpenedFile(std::shared_ptr<PIDFile>& file) {
    std::string ext = getFileExtension(file->getPath());

    if (ext == ".pcx" || ext == ".bmp") {
        static const char* pidFilter[1] = {"*.pid"};
        char* savePath = UI::saveFileDialog(pidFilter, 1, _("PID file"));
        if (!savePath) return;
        file->saveToFile(savePath);
        file->rewriteOriginalData();
        return;
    } 

    file->saveToFile(file->getPath());
    file->rewriteOriginalData();
}

void PIDStudio::pinOpenedFile() {
    if (!openedFile)
        return;
    openedFiles.emplace_back(openedFile);
    openedFile.reset();
}

void PIDStudio::closeFile(const std::shared_ptr<PIDFile>& file) {
    if (file -> isModified()) {
        if (UI::askToSave(file->getPath()))
            file -> saveToFile(file->getPath());
    }
    if (file == currentFile)
        currentFile.reset();

    if (file == openedFile)
        openedFile.reset();
    else
        openedFiles.erase(std::find(openedFiles.begin(), openedFiles.end(), file));
}

void PIDStudio::closeAllFiles() {
    for (const auto& file : openedFiles) {
        filesToClose.insert(file);
    }

    if (openedFile) {
        filesToClose.insert(openedFile);
    }
}

void PIDStudio::saveAllOpenedFiles() {
    for (auto& file : openedFiles) {
        if (file -> isModified()) 
            saveOpenedFile(file);
    }

    if (openedFile)
        if (openedFile -> isModified())
            saveOpenedFile(openedFile);
}

void PIDStudio::openImageFile(
    const std::filesystem::path& path,
    std::shared_ptr<PIDPalette> palette,
    bool inBackground
) {
    PIDFile* imageFile;
    bool alreadyOpened = isFileAlreadyOpen(path, &imageFile);

    if (!inBackground && alreadyOpened) {
        bringFocusTo = imageFile;
        return;
    }

    const std::string ext = getFileExtension(path);

    auto file = std::make_shared<PIDFile>(this);

    if (ext == ".pcx") {
        auto pcxFile = std::make_unique<PCXFile>();
        if (!pcxFile->loadFromFile(path))
            return;
        file->getFromPCX(pcxFile);
    }
    else if (ext == ".bmp") {
        auto bmpFile = std::make_unique<BMPFile>();
        if (!bmpFile->loadFromFile(path))
            return;
        file->getFromBMP(bmpFile);
    }
    else if (ext == ".pid") {
        if (!file->loadFromFile(path))
            return;
            // if palette not provided
        if (!palette) {
            // case 1: has own palette
            if (file->getPalette()) {
                palette = file->getPalette();
            }
            else {
                // case 2: is one of library files 
                std::shared_ptr<TreeNodeBase> libraryNode;
                for (auto& library : assetLibraries) {
                    if (library->hasFilepath(path, libraryNode)) {
                        if (libraryNode) {
                            palette = library->inferPalette(libraryNode);
                            break;
                        }
                    }
                }
                
                // case 3: is one of project files
                std::shared_ptr<TreeNodeBase> projectNode;
                if (currentProject && currentProject->hasFilepath(path, projectNode)) {
                    if (projectNode)
                        palette = currentProject->palette;
                }

                // default:
                if (!palette)
                    palette = defaultPalette;
            }
        }
        file->setPalette(palette);
    } else {
        return;
    }

    if (inBackground) {
        currentBackgroundFile = file;
        return;
    }

    if (openedFile && (openedFile->isModified() || keepAllFilesOpen))
        pinOpenedFile();

    if (keepAllFilesOpen)
        openedFiles.emplace_back(file);
    else
        openedFile = file;
        
    bringFocusTo = file.get();
    saveToRecentlyOpened(path);
}

void PIDStudio::openAllImageFiles(const std::filesystem::path& path, const std::shared_ptr<PIDPalette>& palette) {
    keepAllFilesOpen = true;
    namespace fs = std::filesystem;
    for (const auto& entry : fs::recursive_directory_iterator(path)) {
        std::string ext = getFileExtension((fs::path)entry);
        if (ext == ".pid" || ext == ".pcx" || ext == ".bmp")
            openImageFile((fs::path)entry, palette);
    }
}

bool PIDStudio::isFileAlreadyOpen(const std::filesystem::path& path, PIDFile** outFilePtr) {
    for (auto& file : openedFiles) {
        if (file->getPath() == path) {
            if (outFilePtr) {
                *outFilePtr = file.get();
            }
            return true;
        }
    }

    if (openedFile && openedFile->getPath() == path) {
        if (outFilePtr) {
            *outFilePtr = openedFile.get();
        }
        return true;
    }

    return false;
}

std::string PIDStudio::shortenFilePath(const std::filesystem::path& path) {
    if (path.empty()) 
        return path.string();

    const size_t pathLen = path.string().length();

    for (auto const& library : assetLibraries) {
        const std::string libPath = (library->getPath()).string();
        const size_t libPathLen = libPath.length();

        if (libPath == path.string().substr(0, libPathLen)) {
            std::string libName = library->getIniKey();
            return (
                "<" + libName + ">"
                + path.string().substr(libPathLen, pathLen));
        }
    }

    if (!currentProject)
        return path.string();
    
    const std::string projPath = currentProject->getPath().string();
    const size_t projPathLen = projPath.length();

    if (projPath == path.string().substr(0, projPathLen))
        return (
            "<" + currentProject->name + ">"
            + path.string().substr(projPathLen, pathLen)
        );

    return path.string();
}

void PIDStudio::importPNGs(const std::filesystem::path& outPath) {
    Batch::openImportPNGSSettings(this, outPath);
}

bool PIDStudio::loadPaletteFromFile() {
    static const char* filter[1] = {"*.pal"};
    static size_t filterSize = 1;
    const char* selectedFile = UI::openFileDialog(filter, filterSize, _("Palette files"));

    if (!selectedFile)
        return false;

    auto palette = std::make_shared<PIDPalette>();
    if (!palette->loadFromFile(selectedFile))
        return false;
    
    mapPalette(selectedFile, "", palette, true);
    return true;
}

void PIDStudio::savePaletteToFile() {
    static const char* palFilter[1] = {"*.pal"};
    static size_t filterSize = 1;
    const char* selectedFile = UI::saveFileDialog(palFilter, filterSize, _("Palette files"));

    if (!selectedFile)
        return;

    (currentPalette ? currentPalette : defaultPalette)->saveToFile(selectedFile);
}

void PIDStudio::saveCurrentFileAs() {
    if (!currentFile)
        return;

    static const char* filter[2] = {"*.pid", "*.png"};
    static size_t filterSize = 2;
    const char* selectedFile = UI::saveFileDialog(filter, filterSize, _("Image files (*.pid, *.png)"));

    if (!selectedFile)
        return;

    currentFile->saveToFile(selectedFile);
}

void PIDStudio::saveLibraryFileAs(
    const std::shared_ptr<AssetLibrary>& library,
    const std::shared_ptr<TreeNodeBase>& node,
    const char* format,
    bool compression
) {
    const char* selectedFile = 0;
    if (format == ".pid") {
        static const char* filter[1] = {"*.pid"};
        static size_t filterSize = 1;
        selectedFile = UI::saveFileDialog(filter, filterSize, _("PID Files"));
    }
    else if (format == ".png") {
        static const char* filter[1] = {"*.png"};
        static size_t filterSize = 1;
        selectedFile = UI::saveFileDialog(filter, filterSize, _("PNG Files"));
    }

    if (!selectedFile)
        return;

    openImageFile(node->path, library->inferPalette(node), true);
    if (!currentBackgroundFile)
        return;

    if (format == ".png") {
        currentBackgroundFile -> exportToPNG(selectedFile);
    }
    else if (format == ".pid") {
        currentBackgroundFile -> setFlag(PID_Flag_Compression, compression);
        currentBackgroundFile -> saveToFile(selectedFile);
    }
    currentBackgroundFile.reset();
}

void PIDStudio::saveAllFilesAs(
    const std::filesystem::path& path,
    const std::shared_ptr<PIDPalette>& palette,
    const char* format
) {
    const char* selectedFolder = UI::selectFolderDialog();

    if (!selectedFolder)
        return;

    Batch::setExportDirectory(selectedFolder);
    Batch::setInPalette(palette);

    if (format == ".png")
        Batch::start(path, BATCH_SAVE_AS_PNG);
    else if (format == ".pid")
        Batch::start(path, BATCH_SAVE_AS_PID);
}

void PIDStudio::saveToRecentlyOpened(const std::filesystem::path& path) {
    for (int i = 8; i >= 0; i--)
        settings[RECENT_FILES_INI_KEY][std::to_string(i+1)] = settings[RECENT_FILES_INI_KEY].get(std::to_string(i));

    settings[RECENT_FILES_INI_KEY]["0"] = path.string();
}