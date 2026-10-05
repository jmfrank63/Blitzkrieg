#pragma once
// Building sub-editor - project extension .bld, project XML root tag
// "Building_Composer_Project". MFC source: Sources/src/editor/BuildTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CBuildingTreeRootItem : public CStatsItem
{
public:
	CBuildingTreeRootItem() : CStatsItem( ETIT_BUILDING_ROOT_ITEM, "Building_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingCommonPropsItem : public CStatsItem
{
public:
	CBuildingCommonPropsItem() : CStatsItem( ETIT_BUILDING_COMMON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingPassesItem : public CStatsItem
{
public:
	CBuildingPassesItem() : CStatsItem( ETIT_BUILDING_PASSES_ITEM ) { bStaticElements = false; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingPassPropsItem : public CStatsItem
{
public:
	CBuildingPassPropsItem() : CStatsItem( ETIT_BUILDING_PASS_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingEntrancesItem : public CStatsItem
{
public:
	CBuildingEntrancesItem() : CStatsItem( ETIT_BUILDING_ENTRANCES_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingEntrancePropsItem : public CStatsItem
{
public:
	CBuildingEntrancePropsItem() : CStatsItem( ETIT_BUILDING_ENTRANCE_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingSlotsItem : public CStatsItem
{
public:
	CBuildingSlotsItem() : CStatsItem( ETIT_BUILDING_SLOTS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingSlotPropsItem : public CStatsItem
{
public:
	CBuildingSlotPropsItem() : CStatsItem( ETIT_BUILDING_SLOT_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingFirePointsItem : public CStatsItem
{
public:
	CBuildingFirePointsItem() : CStatsItem( ETIT_BUILDING_FIRE_POINTS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingFirePointPropsItem : public CStatsItem
{
public:
	CBuildingFirePointPropsItem() : CStatsItem( ETIT_BUILDING_FIRE_POINT_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingGraphicsItem : public CStatsItem
{
public:
	CBuildingGraphicsItem() : CStatsItem( ETIT_BUILDING_GRAPHICS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingSummerPropsItem : public CStatsItem
{
public:
	CBuildingSummerPropsItem() : CStatsItem( ETIT_BUILDING_SUMMER_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingWinterPropsItem : public CStatsItem
{
public:
	CBuildingWinterPropsItem() : CStatsItem( ETIT_BUILDING_WINTER_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

// MFC's shared base of the six graphic items. It sets
// E_BUILDING_GRAPHIC1_PROPS_ITEM, which every derived constructor overwrites.
class CBuildingGraphicPropsItem : public CStatsItem
{
protected:
	explicit CBuildingGraphicPropsItem( int nType ) : CStatsItem( nType ) { bStaticElements = true; }
};

class CBuildingGraphic1PropsItem : public CBuildingGraphicPropsItem
{
public:
	CBuildingGraphic1PropsItem() : CBuildingGraphicPropsItem( ETIT_BUILDING_GRAPHIC1_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingGraphic2PropsItem : public CBuildingGraphicPropsItem
{
public:
	CBuildingGraphic2PropsItem() : CBuildingGraphicPropsItem( ETIT_BUILDING_GRAPHIC2_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingGraphic3PropsItem : public CBuildingGraphicPropsItem
{
public:
	CBuildingGraphic3PropsItem() : CBuildingGraphicPropsItem( ETIT_BUILDING_GRAPHIC3_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingGraphicW1PropsItem : public CBuildingGraphicPropsItem
{
public:
	CBuildingGraphicW1PropsItem() : CBuildingGraphicPropsItem( ETIT_BUILDING_GRAPHICW1_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingGraphicW2PropsItem : public CBuildingGraphicPropsItem
{
public:
	CBuildingGraphicW2PropsItem() : CBuildingGraphicPropsItem( ETIT_BUILDING_GRAPHICW2_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingGraphicW3PropsItem : public CBuildingGraphicPropsItem
{
public:
	CBuildingGraphicW3PropsItem() : CBuildingGraphicPropsItem( ETIT_BUILDING_GRAPHICW3_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingDefencesItem : public CStatsItem
{
public:
	CBuildingDefencesItem() : CStatsItem( ETIT_BUILDING_DEFENCES_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingDefencePropsItem : public CStatsItem
{
public:
	CBuildingDefencePropsItem() : CStatsItem( ETIT_BUILDING_DEFENCE_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingDirExplosionsItem : public CStatsItem
{
public:
	CBuildingDirExplosionsItem() : CStatsItem( ETIT_BUILDING_DIR_EXPLOSIONS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingDirExplosionPropsItem : public CStatsItem
{
public:
	CBuildingDirExplosionPropsItem() : CStatsItem( ETIT_BUILDING_DIR_EXPLOSION_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingSmokesItem : public CStatsItem
{
public:
	CBuildingSmokesItem() : CStatsItem( ETIT_BUILDING_SMOKES_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CBuildingSmokePropsItem : public CStatsItem
{
public:
	CBuildingSmokePropsItem() : CStatsItem( ETIT_BUILDING_SMOKE_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

}
