#pragma once

#include "AssetLibrary.h"
#include "Project.h"

#include <imgui.h>
#include <SFML/Graphics/RenderWindow.hpp>

#define MINI_CASE_SENSITIVE
#include <mini/ini.h>

#include <set>

class PIDFile;
class PIDPalette;
class SupportedGame;

class PIDStudio {
	enum OPENED_FILE_WINDOW_RESULT {
		NONE,
		CLOSE,
		KEEP_OPEN,
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

private:
	std::vector<std::shared_ptr<SupportedGame>> supportedGames;
	std::shared_ptr<SupportedGame> claw;

	mINI::INIStructure settings;
	sf::RenderWindow mainWindow;
	std::vector<std::shared_ptr<AssetLibrary>> assetLibraries;
	std::vector<std::shared_ptr<Project>> projects;

	std::vector<std::shared_ptr<PIDFile>> openedFiles;
	std::shared_ptr<PIDFile> openedLibraryFile;
	std::shared_ptr<PIDFile> currentlyFocusedFile;
	std::set<std::shared_ptr<PIDFile>> filesToClose;
    std::shared_ptr<AssetLibrary> libraryToClose;

	std::shared_ptr<PIDPalette> currentPalette;
	std::shared_ptr<PIDPalette> defaultPalette;

	PIDFile* bringFocusTo = nullptr;

	ImGuiID dockspaceId{},
			dockspaceIdLeft{},
			dockspaceIdRight{},
			dockspaceIdRightTop{},
			dockspaceIdRightBottom{};

	void menuBar();
	void toolBar();
	void preDockedWindows();
	void paletteWindow();
	void offsetsWindow();
	void metadataWindow();
	void flagsWindow();
	void libraryWindow();
    void projectsWindow();

	void closeContextMenu();
	void saveAsContextMenu();

	void openedFilesWindows();
	OPENED_FILE_WINDOW_RESULT openedFileWindow(const std::shared_ptr<PIDFile>& file);

	void keepLibraryFileOpened();
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
	void loadPaletteFromFile();
	void savePaletteToFile();
	void saveCurrentFileAs();
	void saveNodeFileAs(
		const std::shared_ptr<AssetLibrary>& library,
		const std::shared_ptr<AssetLibraryTreeNode>& node
	);
	void saveAllFilesAs(
		const std::shared_ptr<AssetLibrary>& library,
		const std::shared_ptr<AssetLibraryTreeNode>& selectedNode,
		const char* format,
		bool compression = false
	);
	void setFlagsCheckboxes();
	void setOffsetsInputs();
	void saveCurrentFile();
	bool checkboxTransparencyFlag[1];
	bool checkboxVideoMemoryFlag[1];
	bool checkboxSystemMemoryFlag[1];
	bool checkboxMirrorFlag[1];
	bool checkboxInvertFlag[1];
	bool checkboxCompressionFlag[1];
	bool checkboxLightsFlag[1];
	bool checkboxOwnPaletteFlag[1];
	int inputIntOffsetX[1];
	int inputIntOffsetY[1];
};
