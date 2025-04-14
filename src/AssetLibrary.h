#pragma once

#include <unordered_map>

#include "FilesystemWatcher.h"

class PIDPalette;
class SupportedGame;

struct AssetLibraryTreeNode : public TreeNodeBase<AssetLibraryTreeNode> {
    std::shared_ptr<PIDPalette> palette;
};

class AssetLibrary : public FilesystemWatcher<AssetLibraryTreeNode>, public std::enable_shared_from_this<AssetLibrary>
{
public:
    typedef void (*FileHandler)(const std::shared_ptr<AssetLibraryTreeNode>&);

    AssetLibrary(class PIDStudio* app, const std::filesystem::path& path, const std::shared_ptr<SupportedGame>& game);

    [[nodiscard]] const char* getIniKey() const;

    bool hasFilepath(const std::filesystem::path& filepath, std::shared_ptr<AssetLibraryTreeNode>& outFoundNode);

    static std::shared_ptr<PIDPalette> inferPalette(const std::shared_ptr<AssetLibraryTreeNode>& node);

    bool isFileTypeSupported(std::string extension);
private:
    static std::unordered_map<std::string, FileHandler> supportedFileTypes;

    class PIDStudio* app;
    std::shared_ptr<SupportedGame> game;

    void populateTree(const std::filesystem::path& path, const std::shared_ptr<AssetLibraryTreeNode>& node) override;
    void processFileNode(const std::shared_ptr<AssetLibraryTreeNode>& childNode) override;
    void displayContextMenu(const std::shared_ptr<AssetLibraryTreeNode> &node) override;
    void openLeafNode(const std::shared_ptr<AssetLibraryTreeNode> &node) override;
};
