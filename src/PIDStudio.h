#pragma once

#include "AssetLibrary.h"
#include "Project.h"

#include <imgui.h>
#include <SFML/Graphics/RenderWindow.hpp>

#define MINI_CASE_SENSITIVE
#include <mini/ini.h>

#include <set>
#include <map>
#include <unordered_map>

class PIDFile;
class PIDPalette;
class SupportedGame;

class PIDStudio {
	enum OPENED_FILE_WINDOW_RESULT {
		NONE,
		CLOSE,
		KEEP_OPEN
	};
public:
	PIDStudio();
	~PIDStudio();

	int run();
	void openLibraryFile(
        const std::shared_ptr<AssetLibrary>& library,
        const std::shared_ptr<AssetLibraryTreeNode>& node,
        bool inSeparateWindow = false
    );
	void keepLibraryFileOpened();
	std::shared_ptr<PIDFile> openLibraryFileinBackground(
		const std::shared_ptr<AssetLibrary>& library,
		const std::shared_ptr<AssetLibraryTreeNode>& node
	);
	void libraryEntryContextMenu(
        const std::shared_ptr<AssetLibrary>& library,
        const std::shared_ptr<AssetLibraryTreeNode>& node,
        bool isLeaf,
        bool isRoot
    );
	std::shared_ptr<PIDPalette> getDefaultPalette() { return defaultPalette; }

	void mapPalette(
		std::filesystem::path,
		std::string libraryName,
		std::shared_ptr<PIDPalette>&,
		bool isLoadedFromFile = false
	);

private:
	std::vector<std::shared_ptr<SupportedGame>> supportedGames;
	std::shared_ptr<SupportedGame> claw;

	mINI::INIStructure settings;
	sf::RenderWindow mainWindow;
	std::vector<std::shared_ptr<AssetLibrary>> assetLibraries;
	std::vector<std::shared_ptr<Project>> projects;

	std::vector<std::shared_ptr<PIDFile>> openedFiles;
	std::shared_ptr<PIDFile> openedLibraryFile;
	std::shared_ptr<PIDFile> currentFile;
	std::set<std::shared_ptr<PIDFile>> filesToClose;
    std::shared_ptr<AssetLibrary> libraryToClose;

	std::shared_ptr<PIDPalette> currentPalette;
	std::shared_ptr<PIDPalette> defaultPalette;
	std::string defaultPaletteName;
	std::shared_ptr<PIDPalette> toTransformPalette;

	std::map<std::string, std::shared_ptr<PIDPalette>> libPalettes; /* Palettes from the library */
	std::unordered_map<std::string, bool> libPalettesComboS; /* States of Select palette combo box */
	std::unordered_map<std::string, bool> libPalettesComboT; /* States of Transform palette combo box */
	std::map<std::string, std::shared_ptr<PIDPalette>> customPalettes; /* Palettes loaded from file*/
	std::unordered_map<std::string, bool> customPalettesComboS; /* States of Select palette combo box */
	std::unordered_map<std::string, bool> customPalettesComboT; /* States of Transform palette combo box */
	bool ownPaletteComboS;
	bool ownPaletteComboT;

	PIDFile* bringFocusTo = nullptr;

	ImGuiID dockspaceId{},
			dockspaceIdLeft{},
			dockspaceIdRight{},
			dockspaceIdRightTop{},
			dockspaceIdRightBottom{};

	void menuBar();
	void toolBar();
	void preDockedWindows();

	std::string resetPaletteComboSelect();
	void paletteComboSelect();
	void resetPaletteComboTransform();
	void paletteComboTransform();

	void paletteWindow();
	void offsetsWindow();
	void metadataWindow();
	void flagsWindow();
	void libraryWindow();
    void projectsWindow();

	void closeContextMenu();

	void openedFilesWindows();
	OPENED_FILE_WINDOW_RESULT openedFileWindow(const std::shared_ptr<PIDFile>& file);

	void saveToRecentlyOpened(std::string path);
	void recentlyOpenedContextMenu();

	void closeFile(const std::shared_ptr<PIDFile>& file);
	void closeAllFiles();

	void openPidFileDialog();
	void addLibraryDialog();
	void addLibrary(std::filesystem::path& path, const std::shared_ptr<SupportedGame>& game);
	bool isFileAlreadyOpen(const std::filesystem::path& path, PIDFile** outFilePtr = nullptr);
	void forEachInLibraryNode(
		const std::shared_ptr<AssetLibrary>& library,
		const std::shared_ptr<AssetLibraryTreeNode>& node,
		const char* action, /* "open", "saveAs"*/
		const char* param1 = nullptr, /* file format for "saveAs" action - ".pid" or ".png" */
		const char* param2 = nullptr, /* folder path for "saveAs" action */
		bool param3 = false, /* true for compressed PIDs*/
		size_t basePathLength = 0
	);
	void openPidFile(std::string filePath);
	void loadPaletteFromFile();
	void savePaletteToFile();
	void saveCurrentFileAs();
	void saveNodeFileAs(
		const std::shared_ptr<AssetLibrary>& library,
		const std::shared_ptr<AssetLibraryTreeNode>& node,
		const char* format,
		bool compression = false
	);
	void saveAllFilesAs(
		const std::shared_ptr<AssetLibrary>& library,
		const std::shared_ptr<AssetLibraryTreeNode>& selectedNode,
		const char* format,
		bool compression = false
	);
	void saveOpenedFile(std::shared_ptr<PIDFile>& file);
	void saveAllOpenedFiles();
	bool canClickSaveAll();
	bool checkboxTransparencyFlag;
	bool checkboxVideoMemoryFlag;
	bool checkboxSystemMemoryFlag;
	bool checkboxCompressionFlag;
	int inputIntOffsetX;
	int inputIntOffsetY;
};
