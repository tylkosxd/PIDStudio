#pragma once

#include "FilesystemWatcher.h"
#include "String.h"

#include <stack>

#include "imgui.h"
#include "ImGuiExtensions.h"
#include "imgui_internal.h"

#include <SFML/Window.hpp>

template<typename Node>
std::shared_ptr<Node> TreeNodeBase<Node>::resolve(const char *resolvePath) {
    for (auto node: children) {
        if (stringEquals(node->name, resolvePath, false)) {
            return node;
        }
    }

    return nullptr;
}

template<typename Node>
std::shared_ptr<Node> TreeNodeBase<Node>::resolve(const char **resolvePaths) {
    std::shared_ptr<Node> node = resolve(*resolvePaths++);

    while (*resolvePaths && node) {
        node = node->resolve(*resolvePaths++);
    }

    return node;
}

template<typename Node>
FilesystemWatcher<Node>::FilesystemWatcher(const std::filesystem::path &path) : path(path), requiresRebuilding(true) {
    fileWatcher.addWatch(path.string(), this, true);
    fileWatcher.watch();
}

template<typename Node>
void FilesystemWatcher<Node>::rebuildTree() {
    root = std::make_shared<Node>();
    root->isHidden = false;

    populateTree(path, root);
    requiresRebuilding = false;
}

template<typename Node>
void FilesystemWatcher<Node>::displayTree() {
    if (requiresRebuilding) {
        rebuildTree();
    }

    std::stack<std::shared_ptr<Node>> stack;
    stack.emplace(root);

    while (!stack.empty()) {
        const auto node = stack.top();
        stack.pop();

        if (!node) {
            ImGui::TreePop();
            continue;
        }
        if (node->isHidden) { continue; }

        ImGuiTreeNodeFlags flags = isLeaf(node)
            ? ImGuiTreeNodeFlags_Leaf
            : ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

        bool isOpen = ImGui::TreeNodeEx(node->name.c_str(), flags);

        if (ImGui::BeginPopupForLastItem(node->name.c_str())) {
            displayContextMenu(node);
            ImGui::EndPopup();
        }

        if (isOpen) {
            if (isLeaf(node)) {
                ImGuiItemStatusFlags leafFlags = ImGui::GetCurrentContext()->LastItemData.StatusFlags;
                if (leafFlags & ImGuiItemStatusFlags_ToggledSelection) {
                    openLeafNode(node);
                }
                if (leafFlags & ImGuiItemStatusFlags_HoveredRect) {
                    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Middle)) {
                        openLeafNode(node, true);
                    }
                }
                ImGui::TreePop();
            } else {
                stack.emplace(nullptr); // delay ImGui::TreePop() to remain correct structure
                for (auto it = node->children.rbegin(); it != node->children.rend(); ++it) {
                    stack.emplace(*it);
                }
            }
        }
    }
}

template<typename Node>
void FilesystemWatcher<Node>::populateTree(
    const std::filesystem::path &rootPath,
    const std::shared_ptr<Node> &rootNode
) {
    std::stack<std::pair<std::filesystem::path, std::shared_ptr<Node>>> stack;
    stack.emplace(rootPath, rootNode);

    while (!stack.empty()) {
        const auto [parentPath, parentNode] = stack.top();
        stack.pop();

        for (const auto &entry: std::filesystem::directory_iterator(parentPath)) {
            std::shared_ptr<Node> childNode = std::make_shared<Node>();
            childNode->parent = parentNode;
            childNode->path = entry.path();
            childNode->name = childNode->path.filename().string();
            parentNode->children.emplace_back(childNode);

            if (entry.is_directory()) {
                stack.emplace(childNode->path, childNode);
            } else if (entry.is_regular_file()) {
                processFileNode(childNode);
            }
        }
    }
}

template<typename Node>
void FilesystemWatcher<Node>::processFileNode(const std::shared_ptr<Node> &childNode) {
    if (!childNode->isHidden) {
        std::shared_ptr<Node> parent = childNode->parent;
        do {
            parent->isHidden = false;
            parent = parent->parent;
        } while (parent && parent->isHidden);
    }
}

template<typename Node>
void FilesystemWatcher<Node>::handleFileAction(
    efsw::WatchID watchId,
    const std::string &dir,
    const std::string &filename,
    efsw::Action action,
    std::string oldFilename
) {
// TODO: could simply add/remove nodes instead of rebuilding entire tree, but I'll start with that as simpler approach
    requiresRebuilding = true;

    /*
    std::filesystem::path relative = std::filesystem::proximate(dir, path);
    std::shared_ptr<AssetLibrary::TreeNode> node = root;

    for (auto& segment : relative) {
        std::string segmentString = segment.string();
        std::shared_ptr<AssetLibrary::TreeNode> childNode = node->resolve(segmentString.c_str());

        if (!childNode) {
            node->children.
        }
    }
    switch (action) {
        case efsw::Actions::Add:
        std::cout << "DIR (" << relative << ") FILE (" << filename << ") has event Added"
        << std::endl;
        break;
        case efsw::Actions::Delete:
        std::cout << "DIR (" << relative << ") FILE (" << filename << ") has event Delete"
        << std::endl;
        break;
        case efsw::Actions::Modified:
        std::cout << "DIR (" << relative << ") FILE (" << filename << ") has event Modified"
        << std::endl;
        break;
        case efsw::Actions::Moved:
        std::cout << "DIR (" << relative << ") FILE (" << filename << ") has event Moved from ("
        << oldFilename << ")" << std::endl;
        break;
        default:
        std::cout << "Should never happen!" << std::endl;
    }
    */
}
