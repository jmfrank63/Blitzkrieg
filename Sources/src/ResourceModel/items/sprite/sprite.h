#pragma once
// Sprite sub-editor - project extension .spt, project XML root tag
// "Sprite_Composer_Project". MFC source: Sources/src/editor/SpriteTreeItem.{h,cpp}.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:55-57.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CSpriteTreeRootItem : public CStatsItem
{
public:
	CSpriteTreeRootItem() : CStatsItem( ETIT_SPRITE_ROOT_ITEM, "Sprite_Composer_Project" ) {}
};
class CSpritePropsItem : public CStatsItem { public: CSpritePropsItem() : CStatsItem( ETIT_SPRITE_PROPS_ITEM ) {} };
class CSpritesItem     : public CStatsItem { public: CSpritesItem()     : CStatsItem( ETIT_SPRITES_ITEM ) {} };

}
