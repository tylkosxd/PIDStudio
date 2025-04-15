#include "AssetLibrary.h"

#include "PIDStudio.h"
#include "formats/PIDPalette.h"
#include "formats/PCXFile.h"
#include "SupportedGame.h"
#include "String.h"

void palFileHandler(const std::shared_ptr<AssetLibraryTreeNode> &node) {
    node->palette = std::make_shared<PIDPalette>();
    if (node->palette->loadFromFile(node->path)) {
        node->parent->palette = node->palette;
    }
}

void pcxFileHandler(const std::shared_ptr<AssetLibraryTreeNode> &node) {
    std::shared_ptr<PCXFile> pcxFile = std::make_shared<PCXFile>();
    if (pcxFile->loadFromFile(node->path)) {
        node->palette = pcxFile->getPalette();
        node->parent->palette = node->palette;
    }
}

void pidFileHandler(const std::shared_ptr<AssetLibraryTreeNode> &node) {
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
) : FilesystemWatcher<AssetLibraryTreeNode>(path), app(app), game(game) {}

void AssetLibrary::populateTree(
    const std::filesystem::path &rootPath,
    const std::shared_ptr<AssetLibraryTreeNode> &rootNode
) {
    FilesystemWatcher::populateTree(rootPath, rootNode);
    game->initializeLibrary(getRoot());
}

bool AssetLibrary::isFileTypeSupported(std::string extension) {
    return supportedFileTypes.contains(extension);
}

void AssetLibrary::processFileNode(const std::shared_ptr<AssetLibraryTreeNode> &childNode) {
    std::string ext = childNode->path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), charToLower);

    if (!supportedFileTypes.contains(ext)) return;

    supportedFileTypes[ext](childNode);
    FilesystemWatcher::processFileNode(childNode);
}

void AssetLibrary::displayContextMenu(const std::shared_ptr<AssetLibraryTreeNode> &node) {
    app->libraryEntryContextMenu(shared_from_this(), node, isLeaf(node), isRoot(node));
}

void AssetLibrary::openLeafNode(const std::shared_ptr<AssetLibraryTreeNode> &node, bool inSeparateWindow) {
    app->openLibraryFile(shared_from_this(), node, inSeparateWindow);
}

bool AssetLibrary::hasFilepath(
    const std::filesystem::path &filepath,
    std::shared_ptr<AssetLibraryTreeNode> &outFoundNode
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

std::shared_ptr<PIDPalette> AssetLibrary::inferPalette(const std::shared_ptr<AssetLibraryTreeNode> &node) {
    std::shared_ptr<PIDPalette> palette = node->palette;
    std::shared_ptr<AssetLibraryTreeNode> parent = node->parent;

    while (parent && !palette) {
        palette = parent->palette;
        parent = parent->parent;
    }

    return palette;
}

const char *AssetLibrary::getIniKey() const {
    return game->getIniKey();
}
