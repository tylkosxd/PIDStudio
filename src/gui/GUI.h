#pragma once

class PIDStudio;
class PIDFile;
class PIDPalette;
class SupportedGame;
class AssetLibrary;
class Project;
struct TreeNodeBase;

namespace UI {
    void menuBar(PIDStudio*);

    void recentlyOpenedContextMenu(PIDStudio*);

    void toolBar(PIDStudio*);

    bool anyFileModified(PIDStudio*);
    bool currentFileModified(PIDStudio*);

    void preDockedWindows(PIDStudio*);

    void libraryEntryContextMenu(PIDStudio*, const std::shared_ptr<AssetLibrary>&, const std::shared_ptr<TreeNodeBase>&);
    void projectEntryContextMenu(PIDStudio*, const std::shared_ptr<Project>&, const std::shared_ptr<TreeNodeBase>&);

    void newFolderPopup(PIDStudio*);
    
    void renameNodePopup(PIDStudio*);

    void hotkeyPress(PIDStudio*, sf::Event);

    void openProjectCreator(PIDStudio*);
    void projectCreator(PIDStudio*);

    void addImagesToProjectDialog(PIDStudio*);
};