#pragma once

#include <filesystem>
#include <vector>

#include "efsw/efsw.hpp"

class PIDStudio;
class PIDPalette;

struct TreeNodeBase {
    std::shared_ptr<TreeNodeBase> parent;
    std::string name;
    std::vector<std::shared_ptr<TreeNodeBase>> children;
    std::filesystem::path path;
    std::shared_ptr<PIDPalette> palette;

    bool isHidden = true;
    bool isLeaf = false;
    bool isRoot = false;

    std::shared_ptr<TreeNodeBase> resolve(const char* path);
    std::shared_ptr<TreeNodeBase> resolve(const char** path);
};

class FilesystemWatcher : public efsw::FileWatchListener {
public:
    explicit FilesystemWatcher(PIDStudio* app, const std::filesystem::path& path);

    void rebuildTree();
    virtual void displayTree();
    void deleteNode(const std::shared_ptr<TreeNodeBase>&);

    void handleFileAction(
        efsw::WatchID watchId,
        const std::string &dir,
        const std::string &filename,
        efsw::Action action,
        std::string oldFilename
    ) override { requiresRebuilding = true; };

    inline const std::filesystem::path& getPath() const { return path; }

protected:
    virtual void populateTree();
    virtual void processFileNode(const std::shared_ptr<TreeNodeBase>& childNode);
    virtual void displayContextMenu(const std::shared_ptr<TreeNodeBase>& node) {};
    virtual void openLeafNode(const std::shared_ptr<TreeNodeBase>& node) {};

    inline const std::shared_ptr<TreeNodeBase>& getRoot() { return root; }

protected:
    PIDStudio* app;

private:
    efsw::FileWatcher fileWatcher;
    bool requiresRebuilding = true;
    std::shared_ptr<TreeNodeBase> root;
    std::filesystem::path path;
};
