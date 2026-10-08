#pragma once
// Bridge sub-editor - project extension .bdg, project XML root tag
// "Bridge_Composer_Project". MFC source: Sources/src/editor/BridgeTreeItem.{h,cpp}.
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

class CBridgeTreeRootItem : public CStatsItem
{
public:
	CBridgeTreeRootItem() : CStatsItem( ETIT_BRIDGE_ROOT_ITEM, "Bridge_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBridgeCommonPropsItem : public CStatsItem
{
public:
	CBridgeCommonPropsItem() : CStatsItem( ETIT_BRIDGE_COMMON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBridgeDefencesItem : public CStatsItem
{
public:
	CBridgeDefencesItem() : CStatsItem( ETIT_BRIDGE_DEFENCES_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBridgeDefencePropsItem : public CStatsItem
{
public:
	CBridgeDefencePropsItem() : CStatsItem( ETIT_BRIDGE_DEFENCE_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBridgeStagePropsItem : public CStatsItem
{
public:
	CBridgeStagePropsItem() : CStatsItem( ETIT_BRIDGE_STAGE_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

// MFC's shared base of the begin, center and end span lists; it has no type of
// its own and adds only a key handler.
class CBridgeCommonSpansItem : public CStatsItem
{
protected:
	explicit CBridgeCommonSpansItem( int nType ) : CStatsItem( nType ) {}
};

class CBridgeBeginSpansItem : public CBridgeCommonSpansItem
{
public:
	CBridgeBeginSpansItem() : CBridgeCommonSpansItem( ETIT_BRIDGE_BEGIN_SPANS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBridgeCenterSpansItem : public CBridgeCommonSpansItem
{
public:
	CBridgeCenterSpansItem() : CBridgeCommonSpansItem( ETIT_BRIDGE_CENTER_SPANS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBridgeEndSpansItem : public CBridgeCommonSpansItem
{
public:
	CBridgeEndSpansItem() : CBridgeCommonSpansItem( ETIT_BRIDGE_END_SPANS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBridgePartsItem : public CStatsItem
{
public:
	CBridgePartsItem() : CStatsItem( ETIT_BRIDGE_PARTS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

	int nSpanIndex = -1;	// the export assigns it
	CListOfTiles lockedTiles;
	CListOfTiles transeparences;
	CListOfTiles unLockedTiles;

protected:
	void ReadData( const NResourceXml::Node &node ) override;
	void WriteData( NResourceXml::Node &node ) const override;
	bool OwnsField( const std::string &name ) const override;
	void InitDefaultValues() override;
};

class CBridgePartPropsItem : public CStatsItem
{
public:
	CBridgePartPropsItem() : CStatsItem( ETIT_BRIDGE_PART_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBridgeFirePointsItem : public CStatsItem
{
public:
	CBridgeFirePointsItem() : CStatsItem( ETIT_BRIDGE_FIRE_POINTS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBridgeFirePointPropsItem : public CStatsItem
{
public:
	CBridgeFirePointPropsItem() : CStatsItem( ETIT_BRIDGE_FIRE_POINT_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBridgeDirExplosionsItem : public CStatsItem
{
public:
	CBridgeDirExplosionsItem() : CStatsItem( ETIT_BRIDGE_DIR_EXPLOSIONS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBridgeDirExplosionPropsItem : public CStatsItem
{
public:
	CBridgeDirExplosionPropsItem() : CStatsItem( ETIT_BRIDGE_DIR_EXPLOSION_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBridgeSmokesItem : public CStatsItem
{
public:
	CBridgeSmokesItem() : CStatsItem( ETIT_BRIDGE_SMOKES_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBridgeSmokePropsItem : public CStatsItem
{
public:
	CBridgeSmokePropsItem() : CStatsItem( ETIT_BRIDGE_SMOKE_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

}
