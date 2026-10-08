#pragma once
// Weapon sub-editor - project extension .wpn, project XML root tag
// "Weapon_Composer_Project". MFC source: Sources/src/editor/WeaponTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CWeaponTreeRootItem : public CStatsItem
{
public:
	CWeaponTreeRootItem() : CStatsItem( ETIT_WEAPON_ROOT_ITEM, "Weapon_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CWeaponCommonPropsItem : public CStatsItem
{
public:
	CWeaponCommonPropsItem() : CStatsItem( ETIT_WEAPON_COMMON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CWeaponShootTypesItem : public CStatsItem
{
public:
	CWeaponShootTypesItem() : CStatsItem( ETIT_WEAPON_SHOOT_TYPES_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CWeaponDamagePropsItem : public CStatsItem
{
public:
	CWeaponDamagePropsItem() : CStatsItem( ETIT_WEAPON_DAMAGE_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
	// CWeaponFrame::FillRPGStats (WeaponFrm.cpp:134) reads the track damage flag on every save.
	bool BoolsReadAsInt() const override { return true; }
};

class CWeaponSoundPropsItem : public CStatsItem
{
public:
	CWeaponSoundPropsItem() : CStatsItem( ETIT_WEAPON_SOUND_PROPS_ITEM ) { bStaticElements = true; }
};

class CWeaponEffectPropsItem : public CStatsItem
{
public:
	CWeaponEffectPropsItem() : CStatsItem( ETIT_WEAPON_EFFECT_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CWeaponFlashPropsItem : public CStatsItem
{
public:
	CWeaponFlashPropsItem() : CStatsItem( ETIT_WEAPON_FLASH_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CWeaponCratersItem : public CStatsItem
{
public:
	CWeaponCratersItem() : CStatsItem( ETIT_WEAPON_CRATERS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CWeaponCraterPropsItem : public CStatsItem
{
public:
	CWeaponCraterPropsItem() : CStatsItem( ETIT_WEAPON_CRATER_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CWeaponEffectsItem : public CStatsItem
{
public:
	CWeaponEffectsItem() : CStatsItem( ETIT_WEAPON_EFFECTS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

}
