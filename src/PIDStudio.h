#pragma once

#include <imgui.h>
#include <SFML/Graphics/RenderWindow.hpp>

#define MINI_CASE_SENSITIVE
#include <mini/ini.h>

#include <set>
#include <filesystem>

class AssetLibrary;
class Project;
class PIDFile;
class PCXFile;
class BMPFile;
class PIDPalette;
class SupportedGame;
struct TreeNodeBase;

const char APPLICATION_NAME[] = "PIDStudio";
const char SETTINGS_INI_FILENAME[] = "PIDStudio.ini";
const char ASSET_LIBRARIES_INI_KEY[] = "AssetLibraries";
const char RECENT_FILES_INI_KEY[] = "RecentFiles";
const char LAST_PROJECT_INI_KEY[] = "LastProject";
const std::filesystem::path PROJECT_INI_FOLDER = ".pidstudio";
const std::filesystem::path PROJECT_INI_FILENAME = "project.ini";
const std::filesystem::path PROJECT_PAL_FILENAME = "project.pal";

enum ADD_IMAGES_OPTION {
	ADD_TO_IMAGES,
	ADD_TO_LEVEL,
	ADD_TILES_ACTION,
	ADD_TILES_BACK,
	ADD_TILES_FRONT,
	NONE
};

class PIDStudio {
public:
	PIDStudio();
	~PIDStudio();

	int run();

	void pinOpenedFile();

	void openBatchCreator(const std::filesystem::path& path);

	void openPaletteManager();

	void mapPalette(
		const std::filesystem::path&,
		const std::string& libraryName,
		const std::shared_ptr<PIDPalette>&,
		bool isLoadedFromFile = false
	);

	std::shared_ptr<PIDPalette> getPalette(const std::string& name);

	void addImagesToProject(
		const std::filesystem::path& pathIn,
		const std::shared_ptr<PIDPalette>& paletteIn,
		ADD_IMAGES_OPTION addOption
	);

	void closeFile(const std::shared_ptr<PIDFile>& file);

	void closeAllFiles();

	void openedFilesWindows();

	void openedFileWindow(const std::shared_ptr<PIDFile>& file);

	bool isFileAlreadyOpen(const std::filesystem::path& path, PIDFile** outFilePtr = nullptr);

	void openImageFileDialog();

	void openImageFile(
		const std::filesystem::path&,
		std::shared_ptr<PIDPalette> palette = nullptr,
		bool inBackground = false
	);

	void openAllImageFiles(const std::filesystem::path& path, const std::shared_ptr<PIDPalette>& palette);

	void saveToRecentlyOpened(const std::filesystem::path& path);

	void addLibrary();

	void createProject(
		const char* name,
		std::filesystem::path& path,
		std::shared_ptr<PIDPalette>& palette
	);

	void openProject(const std::filesystem::path& path);

	void pasteFileFromClipboard(const std::filesystem::path& dstPath);

	void importPNGs(const std::filesystem::path& outPath);

	bool loadPaletteFromFile();

	void savePaletteToFile();

	void saveCurrentFileAs();

	void saveLibraryFileAs(
		const std::shared_ptr<AssetLibrary>& library,
		const std::shared_ptr<TreeNodeBase>& node,
		const char* format,
		bool compression = false
	);

	void saveAllFilesAs(
		const std::filesystem::path& path,
		const std::shared_ptr<PIDPalette>& palette,
		const char* format
	);

	void saveOpenedFile(std::shared_ptr<PIDFile>& file);

	void saveAllOpenedFiles();

	std::string shortenFilePath(const std::filesystem::path& path);

public:

	// a pair of a path to a file and a bool that's true when the file has been cut, not copied
	std::pair<std::filesystem::path, bool> fileClipboard;

	// used for palette transformation of the PID files:
	std::shared_ptr<PIDPalette> lastInPalette;
	std::shared_ptr<PIDPalette> lastOutPalette;
	uint8_t lastColorTable[256];

	std::vector<std::shared_ptr<SupportedGame>> supportedGames;
	std::shared_ptr<SupportedGame> claw;
	std::shared_ptr<SupportedGame> getMedieval;
	std::shared_ptr<SupportedGame> gruntz;

	mINI::INIStructure settings;
	sf::RenderWindow mainWindow;
	std::vector<std::shared_ptr<AssetLibrary>> assetLibraries;
	std::shared_ptr<AssetLibrary> libraryToClose;

	std::vector<std::shared_ptr<PIDFile>> openedFiles;
	std::shared_ptr<PIDFile> openedFile;
	std::shared_ptr<PIDFile> currentFile;
	std::shared_ptr<PIDFile> currentBackgroundFile;
	std::set<std::shared_ptr<PIDFile>> filesToClose;

	std::shared_ptr<PIDPalette> currentPalette;
	std::shared_ptr<PIDPalette> defaultPalette;

	std::vector<std::shared_ptr<PIDPalette>> libPalettes; /* Palettes from the library */
	std::vector<std::shared_ptr<PIDPalette>> customPalettes; /* Other palettes */

	std::shared_ptr<Project> currentProject;
	std::vector<std::shared_ptr<Project>> projects;
	std::shared_ptr<Project> projectToClose;

	ImGuiID dockspaceId{};

	struct {
		std::filesystem::path path;
		std::string name;
		ImVec2 pos = {0, 0};
		ImVec2 size = {0, 0};
	} lastOpenedTreeNode;

	bool contextMenuOpened = false;

	bool keepAllFilesOpen = false;

	ADD_IMAGES_OPTION addImagesOption = NONE;

private:
	PIDFile* bringFocusTo = nullptr;

};
