#pragma once
// Sprite sub-editor - project extension .spt, project XML root tag
// "Sprite_Composer_Project". MFC source: Sources/src/editor/SpriteTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CSpriteTreeRootItem : public CStatsItem
{
public:
	CSpriteTreeRootItem() : CStatsItem( ETIT_SPRITE_ROOT_ITEM, "Sprite_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CSpritePropsItem : public CStatsItem
{
public:
	CSpritePropsItem() : CStatsItem( ETIT_SPRITE_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CSpritesItem : public CStatsItem
{
public:
	CSpritesItem() : CStatsItem( ETIT_SPRITES_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

}
