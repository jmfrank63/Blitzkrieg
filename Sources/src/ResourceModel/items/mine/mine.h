#pragma once
// Mine sub-editor - project extension .mcp, project XML root tag
// "Mine_Composer_Project". MFC source: Sources/src/editor/MineTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CMineTreeRootItem : public CStatsItem
{
public:
	CMineTreeRootItem() : CStatsItem( ETIT_MINE_ROOT_ITEM, "Mine_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CMineCommonPropsItem : public CStatsItem
{
public:
	CMineCommonPropsItem() : CStatsItem( ETIT_MINE_COMMON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

}
