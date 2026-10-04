#pragma once
// Mine sub-editor - project extension .mcp, project XML root tag
// "Mine_Composer_Project". MFC source: Sources/src/editor/MineTreeItem.{h,cpp}.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:223-224.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CMineTreeRootItem : public CStatsItem
{
public:
	CMineTreeRootItem() : CStatsItem( ETIT_MINE_ROOT_ITEM, "Mine_Composer_Project" ) {}
};
class CMineCommonPropsItem : public CStatsItem { public: CMineCommonPropsItem() : CStatsItem( ETIT_MINE_COMMON_PROPS_ITEM ) {} };

}
