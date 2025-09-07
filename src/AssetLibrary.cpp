#include "AssetLibrary.h"

#include "PIDStudio.h"
#include "formats/PIDPalette.h"
#include "formats/PCXFile.h"
#include "SupportedGame.h"
#include "String.h"
#include "gui/GUI.h"

#define PCX_PAL_OFFSET -768

void palFileHandler(const std::shared_ptr<TreeNodeBase> &node) {
    node->palette = std::make_shared<PIDPalette>();
    if (node->palette->loadFromFile(node->path)) {
        node->parent->palette = node->palette;
    }
}

void pcxFileHandler(const std::shared_ptr<TreeNodeBase> &node) {
    node->palette = std::make_shared<PIDPalette>();
    if (node->palette->loadFromFilePartially(node->path, PCX_PAL_OFFSET, std::ios_base::end)) {
        node->parent->palette = node->palette;
    }
    node->isHidden = false;
}

void pidFileHandler(const std::shared_ptr<TreeNodeBase> &node) {
    node->isHidden = false;
}

std::unordered_map<std::string, AssetLibrary::FileHandler> AssetLibrary::supportedFileTypes = {
    {".pal", palFileHandler},
    {".pcx", pcxFileHandler},
    {".pid", pidFileHandler}
};

AssetLibrary::AssetLibrary(
    PIDStudio *app,
    const std::filesystem::path &path,
    const std::shared_ptr<SupportedGame> &game
) : FilesystemWatcher(app, path), game(game) {}

void AssetLibrary::populateTree() {
    FilesystemWatcher::populateTree();
    game->initializeLibrary(getRoot());
}

bool AssetLibrary::isFileTypeSupported(std::string extension) {
    return supportedFileTypes.contains(extension);
}

void AssetLibrary::processFileNode(const std::shared_ptr<TreeNodeBase> &childNode) {
    std::string ext = childNode->path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), charToLower);

    if (!supportedFileTypes.contains(ext)) return;

    supportedFileTypes[ext](childNode);
    FilesystemWatcher::processFileNode(childNode);
}

void AssetLibrary::displayContextMenu(const std::shared_ptr<TreeNodeBase> &node) {
    UI::libraryEntryContextMenu(app, shared_from_this(), node);
}

void AssetLibrary::openLeafNode(const std::shared_ptr<TreeNodeBase> &node) {
    app->openImageFile(node->path, inferPalette(node));
}

bool AssetLibrary::hasFilepath(
    const std::filesystem::path &filepath,
    std::shared_ptr<TreeNodeBase> &outFoundNode
) {
    const std::filesystem::path &path = getPath();

    auto mismatch = std::mismatch(path.begin(), path.end(), filepath.begin());
    bool isFilepathWithinLibrary = mismatch.first == path.end();

    if (isFilepathWithinLibrary) {
        outFoundNode = getRoot();
        for (auto it = mismatch.second; it != filepath.end(); it++) {
            outFoundNode = outFoundNode->resolve(it->string().c_str());
            if (!outFoundNode) return false;
        }
    }

    return isFilepathWithinLibrary && outFoundNode;
}

std::shared_ptr<PIDPalette> AssetLibrary::inferPalette(const std::shared_ptr<TreeNodeBase> &node) {
    auto palette = node->palette;
    auto parent = node->parent;

    while (parent && !palette) {
        palette = parent->palette;
        parent = parent->parent;
    }

    return palette;
}

const char *AssetLibrary::getIniKey() const {
    return game->getIniKey();
}
