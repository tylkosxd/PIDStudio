#include "PIDStudio.h"

#include "formats/PIDFile.h"
#include "formats/PIDPalette.h"
#include "String.h"
#include "Filters.h"

#include "games/Claw.h"
#include "formats/PIDFile.h"

#include <stack>
#include <fmt/core.h>
#include <libintl.h>
#include <tinyfiledialogs/tinyfiledialogs.h>

#include "ImGuiExtensions.h"
#include <imgui-SFML.h>
#include <SFML/Graphics.hpp>
#include "imgui_internal.h"

#include "assets/IconsLucide.h"
#include "assets/lucide.ttf.h"
#include "assets/icon.png.h"
#include "assets/font.ttf.h"
#include "assets/grayscale.pal.h"


#define _(String) gettext(String)
#define _STRINGS_TO_TRANSLATE_ _("Claw") _("Gruntz") _("Get Medieval")

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768

const char APPLICATION_NAME[] = "PIDStudio";
const char SETTINGS_INI_FILENAME[] = "settings.ini";
const char ASSET_LIBRARIES_INI_KEY[] = "AssetLibraries";
const char RECENT_FILES_INI_KEY[] = "RecentFiles";
const char ASSET_LIBRARY_WINDOW_ID[] = "###AssetLibrary";

template <bool isMultiSelect, Filter... filters>
inline const char* openFileDialog(const char* filterName) {
    static constexpr auto filterPatterns = constexpr_get_filter_patterns<filters...>();
    static constexpr auto filterPatternsString = constexpr_get_filter_patterns_string<filters...>();

    return tinyfd_openFileDialog(
        isMultiSelect ? _("Open file(s)") : _("Open file"),
        nullptr,
        sizeof...(filters),
        filterPatterns.data(),
        std::format("{} ({})", filterName, filterPatternsString.data()).c_str(),
        isMultiSelect
    );
}
template <Filter... filters>
inline const char* saveFileDialog(const char* filterName) {
    static constexpr auto filterPatterns = constexpr_get_filter_patterns<filters...>();
    static constexpr auto filterPatternsString = constexpr_get_filter_patterns_string<filters...>();

    return tinyfd_saveFileDialog(
        _("Save file"),
        nullptr,
        sizeof...(filters), 
        filterPatterns.data(), 
        std::format("{} ({})", filterName, filterPatternsString.data()).c_str()
    );
}

PIDStudio::PIDStudio() : mainWindow(sf::VideoMode(SCREEN_WIDTH, SCREEN_HEIGHT), APPLICATION_NAME) {
    // initialize localization
    setlocale(LC_ALL, "");
    bindtextdomain(APPLICATION_NAME, "locale");
    textdomain(APPLICATION_NAME);
    bind_textdomain_codeset(APPLICATION_NAME, "UTF-8");

    // initialize fallback palette and list of supported games
    defaultPalette = std::make_shared<PIDPalette>(GRAYSCALE_PAL);
    defaultPaletteName = "Lights";
    mapPalette(defaultPaletteName, "", defaultPalette);
    claw = std::make_shared<Claw>(this, "Claw", "CLAW", "CLAW.EXE");
    supportedGames.emplace_back(claw);

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

    // load default font
    float baseFontSize = 16.0f;
    float iconFontSize = 13.0f;

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

PIDStudio::~PIDStudio(){
    // save settings to ini file on application close
    mINI::INIFile file(SETTINGS_INI_FILENAME);
    file.write(settings);

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

            switch (event.type)
            {
            case sf::Event::Closed:
                mainWindow.close();
                break;
            case sf::Event::KeyPressed:
                if (event.key.control) {
                    switch (event.key.code)
                    {
                    case sf::Keyboard::O:
                        openPidFileDialog();
                        break;
                    case sf::Keyboard::W:
                        if (currentFile && !event.key.shift) {
                            filesToClose.insert(currentFile);
                            break;
                        }
                        if (event.key.shift) {
                            closeAllFiles();
                            break;
                        }
                        break;
                    case sf::Keyboard::S:
                        if (currentFile && currentFile -> isModified() && !event.key.shift) {
                            saveOpenedFile(currentFile);
                            break;
                        }
                        if (event.key.shift && canClickSaveAll()) {
                            saveAllOpenedFiles();
                            break;
                        }
                    case sf::Keyboard::E: /* 'E' for 'Export' */
                        if (currentFile)
                            saveCurrentFileAs();
                        break;
                    default:
                        break;
                    }
                }
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
        ImGui::PopStyleVar();

        menuBar();
        toolBar();
        preDockedWindows();
        openedFilesWindows();

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
    }

    return 0;
}

void PIDStudio::menuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu(_("File"))) {

            if (ImGui::MenuItemEx(_("Open File..."), ICON_LC_IMAGE_UP, "Ctrl+O")) openPidFileDialog();
            if (ImGui::MenuItemEx(_("Open Folder..."), ICON_LC_FOLDER_UP, "Ctrl+Shift+O", false, false)) { /* TODO */ }

            ImGui::Separator();

            closeContextMenu();

            ImGui::Separator();

            bool currentFileModified = currentFile && currentFile -> isModified();
            if (ImGui::MenuItemEx(_("Save"), ICON_LC_SAVE, "Ctrl+S", false, currentFileModified))
                saveOpenedFile(currentFile);
            
            if (ImGui::MenuItemEx(_("Save as..."), ICON_LC_IMAGE_DOWN, "Ctrl+E"))
                saveCurrentFileAs();

            if (ImGui::MenuItemEx(_("Save all"), ICON_LC_SAVE_ALL, "Ctrl+Shift+S", false, canClickSaveAll()))
                saveAllOpenedFiles();

            ImGui::Separator();

            if (ImGui::BeginMenuEx(_("Recently opened files"), nullptr, settings.has(RECENT_FILES_INI_KEY))) {
                recentlyOpenedContextMenu();
                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::MenuItem(_("Exit"), "Alt+F4")) { mainWindow.close(); }

            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void PIDStudio::toolBar()
{
    ImGui::PushStyleColor(ImGuiCol_Button, 0);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::Button(ICON_LC_IMAGE_UP)) openPidFileDialog();

        if (!currentFile) ImGui::BeginDisabled();
        if (ImGui::Button(ICON_LC_IMAGE_DOWN)) saveCurrentFileAs();
        if (!currentFile) ImGui::EndDisabled();

        bool currentFileModified = currentFile && currentFile -> isModified();

        if (!currentFileModified) ImGui::BeginDisabled();
        if (ImGui::Button(ICON_LC_SAVE)) saveOpenedFile(currentFile);
        if (!currentFileModified) ImGui::EndDisabled();

        bool canSaveAll = canClickSaveAll(); 
        if (!canSaveAll) ImGui::BeginDisabled();
        if (ImGui::Button(ICON_LC_SAVE_ALL)) saveAllOpenedFiles();
        if (!canSaveAll) ImGui::EndDisabled();

        ImGui::EndMenuBar();
    }

    ImGui::PopStyleColor();
}

void PIDStudio::preDockedWindows()
{
    static bool shouldPrepareDockspace = true; // pre-dock windows on launch, but allow user to move them freely later
    dockspaceId = ImGui::DockSpace(ImGui::GetID(APPLICATION_NAME));

    if (shouldPrepareDockspace) {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(
        dockspaceId,
        ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_NoResizeX | ImGuiDockNodeFlags_NoResizeY
        );
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetWindowSize());

        // split initial UI layout to specific dockable areas
        dockspaceIdRight = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Right, 0.25f, nullptr, &dockspaceIdLeft);
        dockspaceIdRightTop = ImGui::DockBuilderSplitNode(dockspaceIdRight, ImGuiDir_Up, 0.3f, nullptr, &dockspaceIdRightBottom);

        ImGui::SetNextWindowDockID(dockspaceIdRightTop, ImGuiDir_Up);
    }
    paletteWindow();

    if (shouldPrepareDockspace) {
        ImGui::SetNextWindowDockID(dockspaceIdRightTop, ImGuiDir_Up);
    }
    offsetsWindow();

    if (shouldPrepareDockspace) {
        ImGui::SetNextWindowDockID(dockspaceIdRightTop, ImGuiDir_Up);
    }
    flagsWindow();

    if (shouldPrepareDockspace) {
        ImGui::SetNextWindowDockID(dockspaceIdRightTop, ImGuiDir_Up);
    }
    metadataWindow();

    if (shouldPrepareDockspace) {
        ImGui::SetNextWindowDockID(dockspaceIdRightBottom, ImGuiDir_Down);
    }
    projectsWindow();
    
    if (shouldPrepareDockspace) {
        ImGui::SetNextWindowDockID(dockspaceIdRightBottom, ImGuiDir_Down);
    }
    libraryWindow();

    if (shouldPrepareDockspace) {
        shouldPrepareDockspace = false;
        ImGui::DockBuilderFinish(dockspaceId);
    }
}

void PIDStudio::openedFilesWindows() {
    if (!filesToClose.empty()) {
        for (const auto& file : filesToClose) {
            closeFile(file);
        }
        filesToClose.clear();
    }

    if (openedLibraryFile) {
        auto result = openedFileWindow(openedLibraryFile);

        switch (result) {
        case KEEP_OPEN:
            keepLibraryFileOpened();
            break;
        case CLOSE:
            filesToClose.insert(openedLibraryFile);
            break;
        default:
            break;
        }
    }

    for (const auto& file : openedFiles) {
        auto result = openedFileWindow(file);

        if (result == CLOSE) {
            filesToClose.insert(file);
        }
    }
}

PIDStudio::OPENED_FILE_WINDOW_RESULT PIDStudio::openedFileWindow(const std::shared_ptr<PIDFile>& file)
{
    bool isLibraryFile = file == openedLibraryFile;

    // Library files use same ID to make sure the window remains its position etc.
    std::string windowName = isLibraryFile ? "[L] " + file->getName() + ASSET_LIBRARY_WINDOW_ID : file->getWindowName();
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;

    bool didNotCloseWindow = true, didClickKeepLibraryFileOpen = false; // outputs from UI library
    ImGui::Begin(
        windowName.c_str(),
        flags,
        &didNotCloseWindow,
        isLibraryFile ? &didClickKeepLibraryFileOpen : nullptr
    );

    /* Fixing ImGui::Begin setting 3rd argument to false on middle mouse button press. */
    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Middle)) {
        didNotCloseWindow = true;
    }

    if (ImGui::BeginPopupContextItem()) {
        closeContextMenu();
        ImGui::EndPopup();
    }

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (ImGui::IsWindowAppearing()) {
        ImGui::SetWindowDock(window, dockspaceId, ImGuiDir_Right);
        currentFile = file;
    }
    if (ImGui::IsWindowFocused()) {
        currentPalette = file->getPalette();
    }
    if (ImGui::IsWindowDocked()) {
        // This fixes windows receiving input after clicking off external modal, which resulted in wrong window being focused.
        ImGui::GetCurrentWindow()->Flags |= ImGuiWindowFlags_NoMouseInputs;
    }
    ImGui::CenteredImage(file->getTexture());
    ImGui::End();

    if (file.get() == bringFocusTo) {
        if (ImGui::BringFocusTo(window)) {
            bringFocusTo = nullptr;
            currentPalette = file->getPalette();
            currentFile = file;
        }
    }

    return !didNotCloseWindow ? CLOSE : didClickKeepLibraryFileOpen ? KEEP_OPEN : NONE;
}


std::string PIDStudio::resetPaletteComboSelect() {
    for (const auto& [key, palette] : (libPalettes)) {
        libPalettesComboS[key] = false;
    }
    for (const auto& [key, palette] : (customPalettes)) {
        customPalettesComboS[key] = false;
    }
    ownPaletteComboS = false;

    if (currentFile) {
        if ((currentFile -> getFlags()) & PID_Flag_Lights) {
            libPalettesComboS[defaultPaletteName] = true;
            return defaultPaletteName;
        }
        if (currentPalette == (currentFile -> getOwnPalette())) {
            ownPaletteComboS = true;
            return _("Own palette");
        }
    }
        for (const auto& [key, palette] : (libPalettes)) {
            if (palette == currentPalette) {
                libPalettesComboS[key] = true;
                return key;
            }
        }
        for (const auto& [key, palette] : (customPalettes)) {
            if (palette == currentPalette) {
                libPalettesComboS[key] = true;
                return key;
            }
        }
    libPalettesComboS[defaultPaletteName] = true;
    return defaultPaletteName;
}

void PIDStudio::paletteComboSelect() {
    
    static std::string preview;

    if (currentFile && currentFile -> isTransformedToPalette()) {
        ImGui::BeginDisabled();
    } else {
        preview = resetPaletteComboSelect();
    } 

    if (ImGui::BeginCombo(_("From"), preview.c_str(), ImGuiComboFlags_HeightLarge)) {

        for (const auto& [key, palette] : (libPalettes)) {
            if (ImGui::Selectable(key.c_str(), &libPalettesComboS[key])) {
                if (currentFile && key == defaultPaletteName.c_str()) {
                    currentFile -> setFlag(PID_Flag_Lights, true);
                    currentFile -> resetTexture();
                } else {
                    if (currentFile) {
                        currentFile -> setFlag(PID_Flag_Lights, false);
                        currentFile -> setPalette(palette);
                    }
                    currentPalette = palette;
                }
            }
        }

        ImGui::Separator();

        if (currentFile && currentFile -> getOwnPalette()) {
            if (ImGui::Selectable(_("Own palette"), &ownPaletteComboS)) {
                currentFile -> setPalette(currentFile -> getOwnPalette());
                currentFile -> setFlag(PID_Flag_Lights, false);
                currentPalette = currentFile -> getOwnPalette();
            }
        }

        for (const auto& [key, palette] : (customPalettes)) {
            if (ImGui::Selectable(key.c_str(), &customPalettesComboS[key])) {
                if (currentFile) {
                    currentFile -> setFlag(PID_Flag_Lights, false);
                    currentFile -> setPalette(palette);
                }
                currentPalette = palette;
            }
        }

        ImGui::Separator();

        bool oneWaySelectable = false;
        if (ImGui::Selectable(_("Load from file"), &oneWaySelectable))
            loadPaletteFromFile();

        ImGui::EndCombo();
    }

    if (currentFile && currentFile -> isTransformedToPalette()) ImGui::EndDisabled();

}


void PIDStudio::resetPaletteComboTransform() {
    for (const auto& [key, palette] : (libPalettes)) {
        libPalettesComboT[key] = false;
    }
    for (const auto& [key, palette] : (customPalettes)) {
        customPalettesComboT[key] = false;
    }
    ownPaletteComboT = false;
}

void PIDStudio::paletteComboTransform() {

    static std::string selectHelper;
    static std::string preview = " ";

    if (ImGui::BeginCombo(_("To"), preview.c_str(), ImGuiComboFlags_HeightLarge)) {

        for (const auto& [key, palette] : (libPalettes)) {
            if (ImGui::Selectable(key.c_str(), &libPalettesComboT[key])) {
                selectHelper = key;
                resetPaletteComboTransform();
                libPalettesComboT[selectHelper] = true;
                toTransformPalette = palette;
                preview = key;
            }
        }

        ImGui::Separator();

        if (currentFile && currentFile -> getOwnPalette()) {
            if (ImGui::Selectable(_("Own palette"), &ownPaletteComboT)) {
                resetPaletteComboTransform();
                ownPaletteComboT = true;
                toTransformPalette = currentFile -> getOwnPalette();
                preview = _("Own palette");
            }
        }

        for (const auto& [key, palette] : (customPalettes)) {
            if (ImGui::Selectable(key.c_str(), &customPalettesComboT[key])) {
                selectHelper = key;
                resetPaletteComboTransform();
                customPalettesComboT[selectHelper] = true;
                toTransformPalette = palette;
                preview = key;
            }
        }

        ImGui::Separator();

        bool oneWaySelectable = false;
        if (ImGui::Selectable(_("Load from file"), &oneWaySelectable))
            loadPaletteFromFile();

        ImGui::EndCombo();
    }
}

void PIDStudio::paletteWindow() {
    if (ImGui::Begin(_("Palette"))) {
        auto palette = defaultPalette;
        bool noLightsFlag = !(currentFile && (currentFile -> getFlags() & PID_Flag_Lights));
        if (currentPalette && noLightsFlag) palette = currentPalette;

        ImGui::CenteredImageHorizontal(palette->getTexture());

        paletteComboSelect();

        if (!currentFile) ImGui::BeginDisabled();

        paletteComboTransform();

        if (ImGui::Button(ICON_LC_SHUFFLE)) {
            if (currentFile && toTransformPalette) {
                currentFile -> transformImageToPalette(toTransformPalette);
                currentPalette = toTransformPalette;
            }
        }
        ImGui::SameLine();
        ImGui::Text("%s", _("Transform "));
        ImGui::SameLine();
        if (ImGui::Button(ICON_LC_ROTATE_CCW)) {
            if (currentFile) {
                currentFile -> resetTransformationToPalette();
                currentPalette = currentFile -> getPalette();
            }
        }
        ImGui::SameLine();
        ImGui::Text("%s", _("Reset"));

        if (!currentFile) ImGui::EndDisabled();

        if (ImGui::BeginPopupForLastItem("Palette")) {
            if (ImGui::MenuItem(_("Save to file"))) savePaletteToFile();
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}


void PIDStudio::offsetsWindow() {
    if (currentFile) {
        inputIntOffsetX = currentFile->getOffsetX();
        inputIntOffsetY = currentFile->getOffsetY();
    }
    if (ImGui::Begin(_("Offsets"))) {
        if (currentFile) {
            int width = currentFile->getWidth();
            int height = currentFile->getHeight();
            if (ImGui::InputInt(_("Off. X"), &inputIntOffsetX, 1, 5)) {
                if (inputIntOffsetX > width) inputIntOffsetX = width;
                if (inputIntOffsetX < -width) inputIntOffsetX = -width;
                currentFile->setOffsetX(inputIntOffsetX);
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_LC_REFRESH_CW)) {
                currentFile->setOffsetX(currentFile->getOriginalOffsetX());
            }
            if (ImGui::InputInt(_("Off. Y"), &inputIntOffsetY, 1, 5)) {
                if (inputIntOffsetY > height) inputIntOffsetY = height;
                if (inputIntOffsetY < -height) inputIntOffsetY = -height;
                currentFile->setOffsetY(inputIntOffsetY);
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_LC_REFRESH_CCW)) {
                currentFile->setOffsetY(currentFile->getOriginalOffsetY());
            }
        } else {
            ImGui::Text("%s", _("No opened files."));
        }
    }
    ImGui::End();
}

void PIDStudio::flagsWindow(){
    if (currentFile) {
        checkboxTransparencyFlag = currentFile->getFlags() & PID_Flag_Transparency;
        checkboxVideoMemoryFlag = currentFile->getFlags() & PID_Flag_VideoMemory;
        checkboxSystemMemoryFlag = currentFile->getFlags() & PID_Flag_SystemMemory;
        checkboxCompressionFlag = currentFile->getFlags() & PID_Flag_Compression;
    }
    if (ImGui::Begin(_("Flags"))) {
        if (currentFile) {
            if (ImGui::Checkbox(_("Transparency"), &checkboxTransparencyFlag))
                currentFile -> setFlag(PID_Flag_Transparency, checkboxTransparencyFlag);

                /* the 2 below are mutualy exclusive */
            if (ImGui::Checkbox(_("Use video memory"), &checkboxVideoMemoryFlag)) {
                currentFile -> setFlag(PID_Flag_VideoMemory, checkboxVideoMemoryFlag);
                if (checkboxVideoMemoryFlag) 
                    currentFile -> setFlag(PID_Flag_SystemMemory, false);
            }
            if (ImGui::Checkbox(_("Use system memory"), &checkboxSystemMemoryFlag)) {
                currentFile -> setFlag(PID_Flag_SystemMemory, checkboxSystemMemoryFlag);
                if (checkboxSystemMemoryFlag)
                    currentFile -> setFlag(PID_Flag_VideoMemory, false);
            }

            /* Mirror and Invert flags are not supported in Claw, so let's omit them by now*/

            if (ImGui::Checkbox(_("Compression"), &checkboxCompressionFlag))
                currentFile -> setFlag(PID_Flag_Compression, checkboxCompressionFlag);

            /* Lights flag is set in the palette dock, and the "OwnPalette" will be set by default when saving the file*/

        } else {
            ImGui::Text("%s", _("No opened files."));
        }
    }
    ImGui::End();
}

void PIDStudio::metadataWindow() {
    if (ImGui::Begin(_("Data"))) {
        if (currentFile) {
            int* userData = currentFile->getUserData();
            ImGui::Text("%s: %dx%d\n%s: %d\n%s: %d, %d",
                _("Size"),
                currentFile -> getWidth(),
                currentFile -> getHeight(),
                _("Magic number"), currentFile->getMagicNumber(),
                _("User values"), userData[0], userData[1]
            );
        } else {
            ImGui::Text("%s", _("No opened files."));
        }
    }
    ImGui::End();
}

void PIDStudio::libraryWindow() {
    ImGui::Begin(_("Library"));

    if (assetLibraries.empty()) {
        const char* text = _("No games in the library.");
        const char* label = _("Add a game");

        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 textSize = ImGui::CalcTextSize(text);
        ImVec2 labelSize = ImGui::CalcTextSize(label);

        ImGui::SetCursorPos(
            ImVec2(
                (windowSize.x - textSize.x) * 0.5f,
                (windowSize.y - textSize.y - labelSize.y) * 0.5f - 10.0f
            )
        );
        ImGui::Text("%s", text);

        ImGui::SetCursorPos(
            ImVec2(
                (windowSize.x - labelSize.x) * 0.5f,
                (windowSize.y - labelSize.y) * 0.5f + 15.0f
            )
        );

        if (ImGui::Button(label)) {
            addLibraryDialog();
        }
    } else {
        for (const std::shared_ptr<AssetLibrary>& library : assetLibraries)
            library->displayTree();
    }

    ImGui::End();
}

void PIDStudio::projectsWindow() {
    ImGui::Begin(_("Projects"));

    if (projects.empty()) {
        ImGui::Text("Test");
    } else {
        for (const std::shared_ptr<Project>& project : projects)
            project->displayTree();
    }

    ImGui::End();
}

void PIDStudio::openPidFileDialog() {
    const char* selectedFiles = openFileDialog<true, pidFilter>(_("Image Files"));

    if (!selectedFiles) return;

    std::istringstream stream(selectedFiles);
    std::string filePath;

    while (std::getline(stream, filePath, '|')) {
        openPidFile(filePath);
    }
}

void PIDStudio::addLibraryDialog() {
    const char* selectedFolder = tinyfd_selectFolderDialog(_("Select game directory"), nullptr);

    if (!selectedFolder) return;

    std::filesystem::path path = selectedFolder;

    if (!std::filesystem::exists(path / claw->getExeName())) {
        path /= "..";
    }

    if (std::filesystem::exists(path / claw->getExeName())) {
        path /= "Assets"; 
        if (!std::filesystem::exists(path)) {
            tinyfd_messageBox(
                _("No Assets directory"), 
                _("Looks like you are trying to use older version of Claw where assets were packed inside CLAW.REZ file. Please update to CrazyHook version."), 
                "ok", 
                "error", 
                1
            );
            return;
        }

        addLibrary(path, claw);
        return;
    }

    std::string message = fmt::format(
        "{} {}.", 
        _("Looks like you are trying to add a game that is not supported. Currently supported games:"), 
        SupportedGame::getNames(supportedGames)
    );
    tinyfd_messageBox(_("Game not recognized"), message.c_str(), "ok", "error", 1);
}

void PIDStudio::libraryEntryContextMenu(
    const std::shared_ptr<AssetLibrary>& library,
    const std::shared_ptr<AssetLibraryTreeNode>& node,
    bool isLeaf,
    bool isRoot
) {
    if (isLeaf) {
        if (ImGui::BeginMenu(_("Save as..."))) {
            if (ImGui::MenuItem(_("PID")))
                saveNodeFileAs(library, node, ".pid");
            if (ImGui::MenuItem(_("Compressed PID")))
                saveNodeFileAs(library, node, ".pid", true);
            if (ImGui::MenuItem(_("PNG")))
                saveNodeFileAs(library, node, ".png");
            ImGui::EndMenu();
        }
    } else {
        if (isRoot) {
            if (ImGui::MenuItem(_("Remove from library"))) libraryToClose = library;
        } else {
            if (ImGui::MenuItem(_("Open all")))
                forEachInLibraryNode(library, node, "open");
        }
        ImGui::Separator();
        if (ImGui::BeginMenu(_("Save all as..."))) {
            if (ImGui::MenuItem(_("PID")))
                saveAllFilesAs(library, node, ".pid");
            if (ImGui::MenuItem(_("Compressed PID")))
                saveAllFilesAs(library, node, ".pid", true);
            if (ImGui::MenuItem(_("PNG")))
                saveAllFilesAs(library, node, ".png");
            ImGui::EndMenu();
        }
    }
}

void PIDStudio::addLibrary(std::filesystem::path& path, const std::shared_ptr<SupportedGame>& game) {
    std::string pathString = path.string();
    settings[ASSET_LIBRARIES_INI_KEY][game->getIniKey()] = pathString;

    auto assetLibrary = std::make_shared<AssetLibrary>(this, path, game);
    assetLibraries.emplace_back(assetLibrary);

    // check if we can infer palette for one of already opened files from the newly added library
    for (const auto& file : openedFiles) {
        if (file->getPalette()) continue;

        std::shared_ptr<AssetLibraryTreeNode> outFoundNode;
        if (assetLibrary->hasFilepath(file->getPath(), outFoundNode)) {
            const auto& palette = assetLibrary->inferPalette(outFoundNode);

            if (!palette) continue;

            file->setPalette(palette);

            if (file == currentFile) {
                currentPalette = palette;
            }
        }
    }
}

void PIDStudio::mapPalette(
    std::filesystem::path path,
    std::string libraryName,
    std::shared_ptr<PIDPalette>& palette,
    bool isNotFromLibrary
) {
    std::string name = libraryName;
    if (!name.empty()) {
        name += " - ";
    }
    if (path.filename().string() != "MAIN.PAL") {
        name += path.filename().string();
    } else {
        std::filesystem::path pathStr = path.parent_path();
        if (!pathStr.string().empty()) {
            name += pathStr.parent_path().filename().string();
        } else {
            name += "Unknown";
        }
    }
    if (!isNotFromLibrary && !libPalettes.contains(name)) {
        libPalettes.insert({name, palette});
        libPalettesComboS.insert({name, false});
        libPalettesComboT.insert({name, false});
    } else if (isNotFromLibrary) {
        int counter = 0;
        std::string finalName = name;
        while (customPalettes.contains(finalName)) {
            counter++;
            finalName = name + " #" + std::to_string(counter);
        }
        customPalettes.insert({finalName, palette});
        customPalettesComboS.insert({finalName, false});
        customPalettesComboT.insert({name, false});
    }
}

void PIDStudio::closeContextMenu() {
    bool isAnyTabOpen = false;
    std::string closeFile = _("Close");
    if (currentFile) {
        isAnyTabOpen = true;
        closeFile += ' ' + currentFile->getName();
    }

    if (ImGui::MenuItem(closeFile.c_str(), "Ctrl+W", false, isAnyTabOpen)) { filesToClose.insert(currentFile); }
    if (ImGui::MenuItem(_("Close all"), "Ctrl+Shift+W", false, isAnyTabOpen)) { closeAllFiles(); }
}

void PIDStudio::saveOpenedFile(std::shared_ptr<PIDFile>& file) {
    file -> saveToFile(file -> getPath());
    file -> rewriteOriginalData();
}

void PIDStudio::keepLibraryFileOpened() {
    bringFocusTo = openedLibraryFile.get();
    openedFiles.push_back(openedLibraryFile);
    openedLibraryFile.reset();
}

void PIDStudio::closeFile(const std::shared_ptr<PIDFile>& file) {
    if (file -> isModified()) {
        std::string question = _("Save the file before closing?\n");
        question += (file -> getPath()).string();
        if (tinyfd_messageBox(_("Save"), question.c_str(), "yesno", "question", 1) == 1) { 
            file -> saveToFile(file -> getPath());
        }
    }
    if (file == currentFile) {
        currentFile.reset();
    }
    if (file == openedLibraryFile) {
        openedLibraryFile.reset();
    } else {
        openedFiles.erase(std::find(openedFiles.begin(), openedFiles.end(), file));
    }
}

void PIDStudio::closeAllFiles() {
    for (const auto& file : openedFiles) {
        filesToClose.insert(file);
    }

    if (openedLibraryFile) {
        filesToClose.insert(openedLibraryFile);
    }
}

void PIDStudio::saveAllOpenedFiles() {
    for (auto& file : openedFiles) {
        if (file -> isModified()) 
            saveOpenedFile(file);
    }

    if (openedLibraryFile) {
        if (openedLibraryFile -> isModified())
            saveOpenedFile(openedLibraryFile);
    }
}

bool PIDStudio::canClickSaveAll() {
    if (!currentFile) return false;

    if (openedLibraryFile && openedLibraryFile -> isModified()) return true;

    for (const auto& file : openedFiles) {
        if (file -> isModified()) 
            return true;
    }

    return false;
}

void PIDStudio::openPidFile(std::string filePath) {
    std::filesystem::path path(filePath);
    if (isFileAlreadyOpen(path)) return;

    std::shared_ptr<AssetLibraryTreeNode> libraryFileNode;
    for (auto& library : assetLibraries) {
        if (library->hasFilepath(path, libraryFileNode)) {
            openLibraryFile(library, libraryFileNode);
            break;
        }
    }

    if (libraryFileNode) return;

    std::shared_ptr<PIDFile> file = std::make_shared<PIDFile>(this);
    if (file->loadFromFile(path)) {
        bringFocusTo = file.get();
        openedFiles.emplace_back(file);
        saveToRecentlyOpened(file->getPath().string());
    }
}

void PIDStudio::openLibraryFile(
    const std::shared_ptr<AssetLibrary>& library,
    const std::shared_ptr<AssetLibraryTreeNode>& node,
    bool inSeparateWindow
) {
    PIDFile* openedFile;
    if (isFileAlreadyOpen(node->path, &openedFile)) {
        bringFocusTo = openedFile;
        return;
    }

    auto file = std::make_shared<PIDFile>(this);
    if (file->loadFromFile(node->path)) {
        if (!inSeparateWindow && openedLibraryFile && openedLibraryFile->isModified()) {
            std::string question = _("Save the file before closing?\n");
            question += (openedLibraryFile -> getPath()).string();
            if (tinyfd_messageBox(_("Save"), question.c_str(), "yesno", "question", 1) == 1)
                openedLibraryFile -> saveToFile(openedLibraryFile -> getPath());
        }
        if (inSeparateWindow && openedLibraryFile) {
            keepLibraryFileOpened();
        }
        openedLibraryFile = file;
        bringFocusTo = file.get();

        std::shared_ptr<PIDPalette> palette = library->inferPalette(node);

        if (palette) {
            openedLibraryFile->setPalette(palette);
            currentPalette = palette;
        }

        if (inSeparateWindow) {
            openedFiles.emplace_back(openedLibraryFile);
            openedLibraryFile.reset();
        }

        saveToRecentlyOpened(file->getPath().string());
    }
}

std::shared_ptr<PIDFile> PIDStudio::openLibraryFileinBackground(
    const std::shared_ptr<AssetLibrary>& library,
    const std::shared_ptr<AssetLibraryTreeNode>& node
) {
    auto file = std::make_shared<PIDFile>(this);
    file -> loadFromFile(node -> path);

    auto palette = library -> inferPalette(node);
    if (palette) file -> setPalette(palette);

    return file;
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

    return false;
}

void PIDStudio::forEachInLibraryNode(
    const std::shared_ptr<AssetLibrary>& library,
    const std::shared_ptr<AssetLibraryTreeNode>& selectedNode,
    const char* action, /* "open", "saveAs"*/
    const char* param1, /* file format for "saveAs"*/
    const char* param2, /* folder path for "saveAs"*/
    bool param3, /* true for compressed PIDs*/
    size_t basePathLength /* 0 by default, pass on subsequent calls when working on the filesystem */
) {
    namespace fs = std::filesystem;
    
    bool workingWithFilesystem = action == "saveAs";

    if (workingWithFilesystem && basePathLength == 0) {
        basePathLength = (selectedNode -> path.parent_path()).string().length();
        if (basePathLength == 0) {
            basePathLength = (library -> getPath()).parent_path().string().length();
        }
    }
    
    for (const auto &entry: selectedNode->children) {

        if (entry->path.empty()) { return; }

        if (fs::is_directory(entry -> path) && !(((fs::path)(entry -> name)).has_extension())) {
            /* recursion */
            forEachInLibraryNode(library, entry, action, param1, param2, param3, basePathLength);

        } else if (action == "open") {
            openLibraryFile(library, entry, true);

        } else if (action == "saveAs") {
            std::string ext = ((fs::path)(entry -> name)).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), charToLower);
            if (!(library -> isFileTypeSupported(ext))) continue;
            if (ext == ".pal" || ext == ".pcx") continue;

            auto file = openLibraryFileinBackground(library, entry);
            fs::path relativeFilePath = (file -> getPath()).string().substr(basePathLength+1);

            fs::path path = (fs::path)param2 / relativeFilePath;
            path = path.replace_extension((fs::path)param1);

            fs::path parentPath = path.parent_path();
            if (!parentPath.empty() && !fs::exists(parentPath)) {
                if (!fs::create_directories(parentPath)) {
                    tinyfd_messageBox(_("Error"), _("Failed to save!"), "ok", "error", 1);
                    return;
                }
            }

            file -> setFlag(PID_Flag_Compression, param3);
            file -> saveToFile(path);
            file.reset();
        }
    }
}

void PIDStudio::loadPaletteFromFile() {
    const char* selectedFile = openFileDialog<false, palFilter>(_("Palette Files"));

    if (!selectedFile) return;

    currentPalette = std::make_shared<PIDPalette>();
    currentPalette->loadFromFile(selectedFile);
    mapPalette(selectedFile, "", currentPalette, true);

    if (!currentFile) return;

    currentFile->setPalette(currentPalette);

    if (currentFile == openedLibraryFile) {
        keepLibraryFileOpened();
    }
}

void PIDStudio::savePaletteToFile() {
    const char* selectedFile = saveFileDialog<palFilter>(_("Palette Files"));

    if (!selectedFile) return;

    (currentPalette ? currentPalette : defaultPalette)->saveToFile(selectedFile);
}

void PIDStudio::saveCurrentFileAs() {
    if (!currentFile) return;

    const char* selectedFile = saveFileDialog<pidFilter, pngFilter>(_("Image Files"));

    if (!selectedFile) return;

    currentFile->saveToFile(selectedFile);
}

void PIDStudio::saveNodeFileAs(
    const std::shared_ptr<AssetLibrary>& library,
    const std::shared_ptr<AssetLibraryTreeNode>& node,
    const char* format,
    bool compression
) {
    const char* selectedFile = 0;
    if (format == ".pid")
        selectedFile = saveFileDialog<pidFilter>(_("Image Files"));
    else if (format == ".png")
        selectedFile = saveFileDialog<pngFilter>(_("Image Files"));

    if (!selectedFile) return;

    auto file = openLibraryFileinBackground(library, node);
    file -> setFlag(PID_Flag_Compression, compression);
    file -> saveToFile(selectedFile);
    file.reset();
}

void PIDStudio::saveAllFilesAs(
    const std::shared_ptr<AssetLibrary>& library,
    const std::shared_ptr<AssetLibraryTreeNode>& selectedNode,
    const char* format,
    bool compression
) {
    const char* selectedFolder = tinyfd_selectFolderDialog( _("Select folder"), nullptr);

    if (!selectedFolder) {return;}

    forEachInLibraryNode(library, selectedNode, "saveAs", format, selectedFolder, compression);
}

void PIDStudio::saveToRecentlyOpened(std::string path) {
    for (int i = 8; i >= 0; i--)
        settings[RECENT_FILES_INI_KEY][std::to_string(i+1)] = settings[RECENT_FILES_INI_KEY].get(std::to_string(i));

    settings[RECENT_FILES_INI_KEY]["0"] = path;
}

void PIDStudio::recentlyOpenedContextMenu() {
    for (int i = 0; i < 10; i++) {
        std::filesystem::path filePath = settings[RECENT_FILES_INI_KEY][std::to_string(i)];
        if (filePath.empty()) continue;
        std::filesystem::path parentPath = filePath.parent_path();
        std::string menuLabel;
        for (auto const& library : settings[ASSET_LIBRARIES_INI_KEY]) {
            while (parentPath.string().length() >= library.second.length()) {
                if (parentPath.string() == library.second) {
                    menuLabel = std::to_string(i+1) + ". " + "[" + library.first + "]"
                        + filePath.string().substr(library.second.length(), filePath.string().length());
                    break;
                }
                parentPath = parentPath.parent_path();
            }
        }
        if (menuLabel.empty()) menuLabel = std::to_string(i+1) + ". " + filePath.string();
        if (ImGui::MenuItem(menuLabel.c_str(), nullptr, false))
            openPidFile(filePath.string());
    }
}