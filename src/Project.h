#pragma once

#include "FilesystemWatcher.h"

struct ProjectTreeNode : TreeNodeBase<ProjectTreeNode> {

};

class Project : public FilesystemWatcher<ProjectTreeNode> {
};
