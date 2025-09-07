#pragma once

#include "FilesystemWatcher.h"
#include <mini/ini.h>

class PIDPalette;

class Project : public FilesystemWatcher, public std::enable_shared_from_this<Project> {
    public:
        typedef void (*FileHandler)(const std::shared_ptr<TreeNodeBase>&);

        Project(class PIDStudio* app, const std::filesystem::path& path);

        std::shared_ptr<PIDPalette> inferPalette(const std::shared_ptr<TreeNodeBase> &node);

        bool hasFilepath(
            const std::filesystem::path &filepath,
            std::shared_ptr<TreeNodeBase> &outFoundNode
        );

        void setRootPalette(const std::shared_ptr<PIDPalette>& newPal);
        void updateINI();

    public:
        std::string name;
        std::shared_ptr<PIDPalette> palette;
        mINI::INIStructure settings;

    private:
        void populateTree() override;
        void processFileNode(const std::shared_ptr<TreeNodeBase> &childNode) override;
        void displayContextMenu(const std::shared_ptr<TreeNodeBase> &node) override;
        void openLeafNode(const std::shared_ptr<TreeNodeBase> &node) override;
};
