#pragma once

#include <unordered_map>

#include "FilesystemWatcher.h"

class PIDPalette;
class SupportedGame;

class AssetLibrary : public FilesystemWatcher, public std::enable_shared_from_this<AssetLibrary> {
public:
    typedef void (*FileHandler)(const std::shared_ptr<TreeNodeBase>&);

    AssetLibrary(class PIDStudio* app, const std::filesystem::path& path, const std::shared_ptr<SupportedGame>& game);

    [[nodiscard]] const char* getIniKey() const;

    bool hasFilepath(const std::filesystem::path& filepath, std::shared_ptr<TreeNodeBase>& outFoundNode);

    static std::shared_ptr<PIDPalette> inferPalette(const std::shared_ptr<TreeNodeBase>& node);

    bool isFileTypeSupported(std::string extension);

private:
    static std::unordered_map<std::string, FileHandler> supportedFileTypes;
    std::shared_ptr<SupportedGame> game;

private:
    void populateTree() override;
    void processFileNode(const std::shared_ptr<TreeNodeBase>& childNode) override;
    void displayContextMenu(const std::shared_ptr<TreeNodeBase> &node) override;
    void openLeafNode(const std::shared_ptr<TreeNodeBase> &node) override;
};
