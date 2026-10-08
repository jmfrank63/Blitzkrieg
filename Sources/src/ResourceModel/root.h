#pragma once
// A Root is the top CTreeItem of a project: the root item any sub-editor
// creates on open and feeds to the factory. The scaffold task does not define
// any concrete roots (T02 adds E_WEAPON_ROOT_ITEM etc.), so Root is a bare
// CTreeItem alias plus a helper that walks children recursively. Later tasks
// specialise it per sub-editor; for now the project loader produces a plain
// CTreeItem whose children are either known items the factory built or
// FutureBlobs wrapping unknown tags.

#include "tree_item.h"

namespace NResourceModel
{

using Root = CTreeItem;

}
