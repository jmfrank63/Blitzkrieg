#pragma once
// Infantry (Unit/Animation) sub-editor - project extension .unt, project XML root tag
// "Unit_Composer_Project". MFC source: Sources/src/editor/AnimTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CAnimationTreeRootItem : public CStatsItem
{
public:
	CAnimationTreeRootItem() : CStatsItem( ETIT_ANIMATION_ROOT_ITEM, "Unit_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CUnitCommonPropsItem : public CStatsItem
{
public:
	CUnitCommonPropsItem() : CStatsItem( ETIT_UNIT_COMMON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CUnitAIPropsItem : public CStatsItem
{
public:
	CUnitAIPropsItem() : CStatsItem( ETIT_UNIT_AI_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CUnitWeaponPropsItem : public CStatsItem
{
public:
	CUnitWeaponPropsItem() : CStatsItem( ETIT_UNIT_WEAPON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CUnitGrenadePropsItem : public CStatsItem
{
public:
	CUnitGrenadePropsItem() : CStatsItem( ETIT_UNIT_GRENADE_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CDirectoryPropsItem : public CStatsItem
{
public:
	CDirectoryPropsItem() : CStatsItem( ETIT_UNIT_DIRECTORY_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CUnitSeasonPropsItem : public CStatsItem
{
public:
	CUnitSeasonPropsItem() : CStatsItem( ETIT_UNIT_SEASON_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CDirectoriesItem : public CStatsItem
{
public:
	CDirectoriesItem() : CStatsItem( ETIT_UNIT_DIRECTORIES_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CUnitAnimationsItem : public CStatsItem
{
public:
	CUnitAnimationsItem() : CStatsItem( ETIT_UNIT_ANIMATIONS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CUnitAnimationPropsItem : public CStatsItem
{
public:
	CUnitAnimationPropsItem() : CStatsItem( ETIT_UNIT_ANIMATION_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CUnitFramePropsItem : public CStatsItem
{
public:
	CUnitFramePropsItem() : CStatsItem( ETIT_UNIT_FRAME_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CUnitActionsItem : public CStatsItem
{
public:
	CUnitActionsItem() : CStatsItem( ETIT_UNIT_ACTIONS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CUnitActionPropsItem : public CStatsItem
{
public:
	CUnitActionPropsItem() : CStatsItem( ETIT_UNIT_ACTION_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CUnitExposuresItem : public CStatsItem
{
public:
	CUnitExposuresItem() : CStatsItem( ETIT_UNIT_EXPOSURES_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CUnitAcksItem : public CStatsItem
{
public:
	CUnitAcksItem() : CStatsItem( ETIT_UNIT_ACKS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CUnitAckTypesItem : public CStatsItem
{
public:
	CUnitAckTypesItem() : CStatsItem( ETIT_UNIT_ACK_TYPES_ITEM ) {}
};

class CUnitAckTypePropsItem : public CStatsItem
{
public:
	CUnitAckTypePropsItem() : CStatsItem( ETIT_UNIT_ACK_TYPE_PROPS_ITEM ) {}
};

}
