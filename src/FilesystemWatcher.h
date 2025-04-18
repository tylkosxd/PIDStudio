#pragma once

#include <filesystem>
#include <vector>

#include "efsw/efsw.hpp"

template<typename Node>
struct TreeNodeBase {
    std::shared_ptr<Node> parent;
    std::string name;
    std::vector<std::shared_ptr<Node>> children;
    std::filesystem::path path;
    bool isHidden = true;

    std::shared_ptr<Node> resolve(const char* path);
    std::shared_ptr<Node> resolve(const char** path);
};

template<typename Node>
class FilesystemWatcher : public efsw::FileWatchListener {
public:
    explicit FilesystemWatcher(const std::filesystem::path& path);

    void rebuildTree();
    virtual void displayTree();

    void handleFileAction(
        efsw::WatchID watchId,
        const std::string &dir,
        const std::string &filename,
        efsw::Action action,
        std::string oldFilename
    ) override;

    inline bool isLeaf(const std::shared_ptr<Node>& node) { return node->children.empty(); }
    inline bool isRoot(const std::shared_ptr<Node>& node) { return node == root; }
    std::filesystem::path getRootPath() const { return path; }
protected:
    virtual void populateTree(
        const std::filesystem::path& rootPath,
        const std::shared_ptr<Node>& rootNode
    );
    virtual void processFileNode(const std::shared_ptr<Node>& childNode);
    virtual void displayContextMenu(const std::shared_ptr<Node>& node) {};
    virtual void openLeafNode(
        const std::shared_ptr<Node>& node,
        bool inSeparateWindow = false
    ) {};

    inline const std::shared_ptr<Node>& getRoot() { return root; }
    inline const std::filesystem::path& getPath() { return path; }
private:
    efsw::FileWatcher fileWatcher;
    bool requiresRebuilding;
    std::shared_ptr<Node> root;
    std::filesystem::path path;
};

#include "FilesystemWatcher.tpp"
