#pragma once

#include "PIDStudio.h"
#include "FilesystemWatcher.h"
#include "String.h"
#include "gui/Dialogs.h"

#include <stack>

#include "imgui.h"
#include "ImGuiExtensions.h"
#include "imgui_internal.h"

std::shared_ptr<TreeNodeBase> TreeNodeBase::resolve(const char *resolvePath) {
    for (auto node: children) {
        if (stringEquals(node->name, resolvePath, false)) {
            return node;
        }
    }

    return nullptr;
}

std::shared_ptr<TreeNodeBase> TreeNodeBase::resolve(const char **resolvePaths) {
    std::shared_ptr<TreeNodeBase> node = resolve(*resolvePaths++);

    while (*resolvePaths && node) {
        node = node->resolve(*resolvePaths++);
    }

    return node;
}

FilesystemWatcher::FilesystemWatcher(PIDStudio* app, const std::filesystem::path &path) : app(app), path(path) {
    fileWatcher.addWatch(path.string(), this, true);
    fileWatcher.watch();
}

void FilesystemWatcher::rebuildTree() {
    root = std::make_shared<TreeNodeBase>();
    root->isHidden = false;

    populateTree();
    requiresRebuilding = false;
}

void FilesystemWatcher::displayTree() {
    if (requiresRebuilding) {
        rebuildTree();
    }

    std::stack<std::shared_ptr<TreeNodeBase>> stack;
    stack.emplace(root);

    while (!stack.empty()) {
        const auto node = stack.top();
        stack.pop();

        if (!node) {
            ImGui::TreePop();
            continue;
        }
        if (node->isHidden) {
            continue;
        }

        ImGuiTreeNodeFlags flags = (node->isLeaf) ? ImGuiTreeNodeFlags_Leaf
            : ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

        bool isOpen = ImGui::TreeNodeEx(node->name.c_str(), flags);

        ImVec2 nodeRectMin = ImGui::GetItemRectMin();
        ImVec2 nodeSize = ImGui::GetItemRectSize();

        if (ImGuiEx::BeginPopupForLastItem(node->name.c_str())) {
            app->lastOpenedTreeNode = {node->path, node->name, nodeRectMin, nodeSize};
            displayContextMenu(node);
            ImGui::EndPopup();
        }

        if (isOpen) {
            if (node->isLeaf) {
                ImGuiItemStatusFlags leafFlags = ImGui::GetCurrentContext()->LastItemData.StatusFlags;
                if (leafFlags & ImGuiItemStatusFlags_ToggledSelection)
                    openLeafNode(node);
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

void FilesystemWatcher::populateTree() {
    std::stack<std::pair<std::filesystem::path, std::shared_ptr<TreeNodeBase>>> stack;
    stack.emplace(path, root);
    root->isRoot = true;
    root->path = path;

    while (!stack.empty()) {
        const auto [parentPath, parentNode] = stack.top();
        stack.pop();

        for (const auto &entry: std::filesystem::directory_iterator(parentPath)) {
            auto childNode = std::make_shared<TreeNodeBase>();
            childNode->parent = parentNode;
            childNode->path = entry.path();
            childNode->name = childNode->path.filename().string();
            parentNode->children.emplace_back(childNode);

            if (entry.is_directory()) {
                stack.emplace(childNode->path, childNode);
                if (childNode->path.filename().string().substr(0,1) != ".")
                    childNode->isHidden = false;
            }
            else if (entry.is_regular_file()) {
                processFileNode(childNode);
            }
        }
    }
}

void FilesystemWatcher::processFileNode(const std::shared_ptr<TreeNodeBase> &childNode) {
    childNode->isLeaf = true;
    if (!childNode->isHidden) {
        auto parent = childNode->parent;
        do {
            parent->isHidden = false;
            parent = parent->parent;
        } while (parent && parent->isHidden);
    }
}

void FilesystemWatcher::deleteNode(const std::shared_ptr<TreeNodeBase>& node) {
    WARNING_MESSAGE message = node->isLeaf ? WARNING_ASK_TO_DELETE_SINGLE : WARNING_ASK_TO_DELETE_MULTIPLE;
    if (UI::warningMessageBox(message, (node->path).string())) {
        if (node->isLeaf)
            std::filesystem::remove(node->path);
        else
            std::filesystem::remove_all(node->path);
        requiresRebuilding = true;
    }
}
