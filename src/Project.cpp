#include "Project.h"
#include "String.h"
#include "PIDStudio.h"
#include "gui/GUI.h"

#include "formats/PIDPalette.h"

Project::Project(
    PIDStudio *app,
    const std::filesystem::path &path
) : FilesystemWatcher(app, path) {
    namespace fs = std::filesystem;
    fs::path projectIniFile = path / PROJECT_INI_FOLDER / PROJECT_INI_FILENAME;

    if (!fs::exists(projectIniFile))
        return;

    mINI::INIFile file(projectIniFile.string());
    file.read(settings);

    name = settings["Name"].get("0");
    palette = app->getPalette(settings["Palette"].get("Root"));

    if (!palette) {
        fs::path projectPaletteFile = path / PROJECT_INI_FOLDER / PROJECT_PAL_FILENAME;

        auto projectPalette = std::make_shared<PIDPalette>();

        if (projectPalette->loadFromFile(projectPaletteFile)) {
            app->customPalettes.emplace_back(projectPalette);
            projectPalette->setName(settings["Name"].get("0"));
            palette = projectPalette;
        }
        else {
            palette = app->defaultPalette;
        }
    }
}

void Project::populateTree() {
    FilesystemWatcher::populateTree();
    getRoot()->name = name;
}

void Project::processFileNode(const std::shared_ptr<TreeNodeBase> &node) {
    std::string ext = node->path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), charToLower);

    if (ext == ".pid" || ext == ".pcx" || ext == ".bmp") {
        node->isHidden = false;
        FilesystemWatcher::processFileNode(node);
    }
}

void Project::displayContextMenu(const std::shared_ptr<TreeNodeBase> &node) {
    UI::projectEntryContextMenu(app, shared_from_this(), node);
}

std::shared_ptr<PIDPalette> Project::inferPalette(const std::shared_ptr<TreeNodeBase> &node) {
    return palette;
}

void Project::openLeafNode(const std::shared_ptr<TreeNodeBase> &node) {
    app->openImageFile(node->path, inferPalette(node));
}

bool Project::hasFilepath(const std::filesystem::path &filepath, std::shared_ptr<TreeNodeBase> &outFoundNode) {
    const std::filesystem::path &path = getPath();

    auto mismatch = std::mismatch(path.begin(), path.end(), filepath.begin());
    bool isFilepathInProject = mismatch.first == path.end();

    if (isFilepathInProject) {
        outFoundNode = getRoot();
        for (auto it = mismatch.second; it != filepath.end(); it++) {
            outFoundNode = outFoundNode->resolve(it->string().c_str());
            if (!outFoundNode) return false;
        }
    }

    return isFilepathInProject && outFoundNode;
}

void Project::setRootPalette(const std::shared_ptr<PIDPalette>& newPal) {
    this->palette = newPal;
    std::filesystem::path path = this->getPath() / PROJECT_INI_FOLDER / PROJECT_PAL_FILENAME;
    newPal->saveToFile(path);
    updateINI();
}

void Project::updateINI() {
    settings["Name"]["0"] = name;
    settings["Palette"]["Root"] = palette->getName();

    namespace fs = std::filesystem;
    fs::path iniFile = (this->getPath()) / PROJECT_INI_FOLDER / PROJECT_INI_FILENAME;

    if (fs::is_directory(iniFile.parent_path())) {
        mINI::INIFile file(iniFile.string().c_str());
        file.write(settings);
    }
}