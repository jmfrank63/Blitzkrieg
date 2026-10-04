#pragma once
// Fence sub-editor - project extension .fnc, project XML root tag
// "Fence_Composer_Project". MFC source: Sources/src/editor/FenceTreeItem.{h,cpp}.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:158-162.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CFenceTreeRootItem : public CStatsItem
{
public:
	CFenceTreeRootItem() : CStatsItem( ETIT_FENCE_ROOT_ITEM, "Fence_Composer_Project" ) {}
};
class CFenceCommonPropsItem : public CStatsItem { public: CFenceCommonPropsItem() : CStatsItem( ETIT_FENCE_COMMON_PROPS_ITEM ) {} };
class CFenceDirectionItem   : public CStatsItem { public: CFenceDirectionItem()   : CStatsItem( ETIT_FENCE_DIRECTION_ITEM ) {} };
class CFenceInsertItem      : public CStatsItem { public: CFenceInsertItem()      : CStatsItem( ETIT_FENCE_INSERT_ITEM ) {} };
class CFencePropsItem       : public CStatsItem { public: CFencePropsItem()       : CStatsItem( ETIT_FENCE_PROPS_ITEM ) {} };

}
