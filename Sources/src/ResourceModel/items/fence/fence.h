#pragma once
// Fence sub-editor - project extension .fnc, project XML root tag
// "Fence_Composer_Project". MFC source: Sources/src/editor/FenceTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"
#include "../ai_tiles.h"

namespace NResourceModel
{

class CFenceTreeRootItem : public CStatsItem
{
public:
	CFenceTreeRootItem() : CStatsItem( ETIT_FENCE_ROOT_ITEM, "Fence_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CFenceCommonPropsItem : public CStatsItem
{
public:
	CFenceCommonPropsItem() : CStatsItem( ETIT_FENCE_COMMON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CFenceDirectionItem : public CStatsItem
{
public:
	CFenceDirectionItem() : CStatsItem( ETIT_FENCE_DIRECTION_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CFenceInsertItem : public CStatsItem
{
public:
	CFenceInsertItem() : CStatsItem( ETIT_FENCE_INSERT_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CFencePropsItem : public CStatsItem
{
public:
	CFencePropsItem() : CStatsItem( ETIT_FENCE_PROPS_ITEM ) { InitDefaultValues(); }

	Vec3 vSpritePos{ 16 * fWorldCellSize, 16 * fWorldCellSize, 0 };	// the segment sprite's place in the view
	CListOfTiles lockedTiles;
	CListOfTiles transeparences;
	int nSegmentIndex = -1;	// the export assigns it

protected:
	void ReadData( const NResourceXml::Node &node ) override;
	void WriteData( NResourceXml::Node &node ) const override;
	bool OwnsField( const std::string &name ) const override;
	void InitDefaultValues() override;
};

}
