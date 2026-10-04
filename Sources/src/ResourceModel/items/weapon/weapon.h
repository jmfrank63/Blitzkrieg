#pragma once
// Weapon sub-editor - project extension .wpn, project XML root tag
// "Weapon_Composer_Project". MFC source: Sources/src/editor/WeaponTreeItem.{h,cpp}.
// Each class below mirrors a REGISTER_CLASS entry in
// Sources/src/editor/TreeItemFactory.cpp:106-115. Prop vectors stay empty for
// T02 and move in with their defaults in a later task; the shell is enough for
// the minimal fixture (which plants the authored data under a <fixture> blob).

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CWeaponTreeRootItem : public CStatsItem
{
public:
	CWeaponTreeRootItem() : CStatsItem( ETIT_WEAPON_ROOT_ITEM, "Weapon_Composer_Project" ) {}
};
class CWeaponCommonPropsItem : public CStatsItem { public: CWeaponCommonPropsItem() : CStatsItem( ETIT_WEAPON_COMMON_PROPS_ITEM ) {} };
class CWeaponShootTypesItem  : public CStatsItem { public: CWeaponShootTypesItem()  : CStatsItem( ETIT_WEAPON_SHOOT_TYPES_ITEM ) {} };
class CWeaponDamagePropsItem : public CStatsItem { public: CWeaponDamagePropsItem() : CStatsItem( ETIT_WEAPON_DAMAGE_PROPS_ITEM ) {} };
class CWeaponSoundPropsItem  : public CStatsItem { public: CWeaponSoundPropsItem()  : CStatsItem( ETIT_WEAPON_SOUND_PROPS_ITEM ) {} };
class CWeaponEffectPropsItem : public CStatsItem { public: CWeaponEffectPropsItem() : CStatsItem( ETIT_WEAPON_EFFECT_PROPS_ITEM ) {} };
class CWeaponFlashPropsItem  : public CStatsItem { public: CWeaponFlashPropsItem()  : CStatsItem( ETIT_WEAPON_FLASH_PROPS_ITEM ) {} };
class CWeaponCratersItem     : public CStatsItem { public: CWeaponCratersItem()     : CStatsItem( ETIT_WEAPON_CRATERS_ITEM ) {} };
class CWeaponCraterPropsItem : public CStatsItem { public: CWeaponCraterPropsItem() : CStatsItem( ETIT_WEAPON_CRATER_PROPS_ITEM ) {} };
class CWeaponEffectsItem     : public CStatsItem { public: CWeaponEffectsItem()     : CStatsItem( ETIT_WEAPON_EFFECTS_ITEM ) {} };

}
