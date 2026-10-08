#include "weapon.h"

#include "../../editor_env.h"

namespace NResourceModel
{

void CWeaponTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_WEAPON_COMMON_PROPS_ITEM;
	child.szDefaultName = "Name";
	child.szDisplayName = "Name";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_WEAPON_SHOOT_TYPES_ITEM;
	child.szDefaultName = "Shoot types";
	child.szDisplayName = "Shoot types";
	defaultChilds.push_back( child );
}

void CWeaponCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown Weapon";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Delta angle";
	prop.szDisplayName = "Delta angle";
	prop.value = 10;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Ammo per burst";
	prop.szDisplayName = "Ammo per burst";
	prop.value = 1;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Aiming time";
	prop.szDisplayName = "Aiming time";
	prop.value = 100.0f;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Dispertion";
	prop.szDisplayName = "Dispersion";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Range min";
	prop.szDisplayName = "Range min";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Range max";
	prop.szDisplayName = "Range max";
	prop.value = 100.0f;
	defaultValues.push_back( prop );

	prop.nId = 8;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Ceiling";
	prop.szDisplayName = "Ceiling";
	prop.value = 100;
	defaultValues.push_back( prop );

	prop.nId = 9;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Reveal radius";
	prop.szDisplayName = "Reveal radius";
	prop.value = 10.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CWeaponShootTypesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CWeaponDamagePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Trajectory type";
	prop.szDisplayName = "Trajectory type";
	prop.value = "line";
	prop.szStrings.push_back( "line" );
	prop.szStrings.push_back( "howitzer" );
	prop.szStrings.push_back( "bomb" );
	prop.szStrings.push_back( "cannon" );
	prop.szStrings.push_back( "rocket" );
	prop.szStrings.push_back( "grenade" );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Piercing power";
	prop.szDisplayName = "Piercing power";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Random piercing";
	prop.szDisplayName = "Random piercing";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Damage power";
	prop.szDisplayName = "Damage power";
	prop.value = 5;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Random damage";
	prop.szDisplayName = "Random damage";
	prop.value = 2;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Damage area";
	prop.szDisplayName = "Damage area";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Damage area 2";
	prop.szDisplayName = "Damage area 2";
	prop.value = 2.0f;
	defaultValues.push_back( prop );

	prop.nId = 8;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Speed";
	prop.szDisplayName = "Speed";
	prop.value = 10.0f;
	defaultValues.push_back( prop );

	prop.nId = 9;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Track damage flag";
	prop.szDisplayName = "Track damage flag";
	prop.value = false;
	defaultValues.push_back( prop );

	prop.nId = 10;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Detonation power";
	prop.szDisplayName = "Detonation power";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	prop.nId = 11;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Rate of fire";
	prop.szDisplayName = "Rate of fire";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 12;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Relax time";
	prop.szDisplayName = "Relax time";
	prop.value = 100.0f;
	defaultValues.push_back( prop );

	prop.nId = 13;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Shell type";
	prop.szDisplayName = "Shell type";
	prop.value = "damage";
	prop.szStrings.push_back( "damage" );
	prop.szStrings.push_back( "morale" );
	prop.szStrings.push_back( "smoke" );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 14;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Trace probability (%)";
	prop.szDisplayName = "Trace probability (%)";
	prop.value = 10.0f;
	defaultValues.push_back( prop );

	prop.nId = 15;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Trace speed coefficient";
	prop.szDisplayName = "Trace speed coefficient";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 16;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Chance to break track";
	prop.szDisplayName = "Chance to break track";
	prop.value = 0.01f;
	defaultValues.push_back( prop );

	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_WEAPON_CRATERS_ITEM;
	child.szDefaultName = "Picture craters";
	child.szDisplayName = "Picture craters";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_WEAPON_EFFECTS_ITEM;
	child.szDefaultName = "Effects";
	child.szDisplayName = "Effects";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_WEAPON_FLASH_PROPS_ITEM;
	child.szDefaultName = "Flash fire";
	child.szDisplayName = "Flash fire";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_WEAPON_FLASH_PROPS_ITEM;
	child.szDefaultName = "Flash explosion";
	child.szDisplayName = "Flash explosion";
	defaultChilds.push_back( child );
}

void CWeaponEffectPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Effect";
	prop.szDisplayName = "Effect";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sound";
	prop.szDisplayName = "Sound";
	prop.value = "";
	prop.szStrings.push_back( GetEditorDataDir() + "sounds\\" );
	prop.szStrings.push_back( szSoundFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Min distance";
	prop.szDisplayName = "Min distance";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Max distance";
	prop.szDisplayName = "Max distance";
	prop.value = 2.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CWeaponFlashPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 0;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Flash power";
	prop.szDisplayName = "Flash power";
	prop.value = 100;
	defaultValues.push_back( prop );

	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Flash duration";
	prop.szDisplayName = "Flash duration";
	prop.value = 1000;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CWeaponCratersItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSEDIR;
	prop.szDefaultName = "Craters directory";
	prop.szDisplayName = "Craters directory";
	prop.value = "";
	std::string szDir = GetEditorDataDir();
	szDir += "TerraObjects\\Shell_Hole\\";
	prop.szStrings.push_back( szDir.c_str() );
	defaultValues.push_back( prop );

	values = defaultValues;

	defaultChilds.clear();
}

void CWeaponCraterPropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_CRATER_REF;
	prop.szDefaultName = "Crater file";
	prop.szDisplayName = "Crater reference";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CWeaponEffectsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_SOUND_REF;
	prop.szDefaultName = "Human fire sound";
	prop.szDisplayName = "Human fire sound";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Gun fire effect";
	prop.szDisplayName = "Gun fire effect";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Trajectory effect";
	prop.szDisplayName = "Trajectory effect";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Direct hit effect";
	prop.szDisplayName = "Direct hit effect";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Miss hit effect";
	prop.szDisplayName = "Miss hit effect";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Reflect hit effect";
	prop.szDisplayName = "Reflect hit effect";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Ground hit effect";
	prop.szDisplayName = "Ground hit effect";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 8;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Water hit effect";
	prop.szDisplayName = "Water hit effect";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 9;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Air hit effect";
	prop.szDisplayName = "Air hit effect";
	prop.value = "";
	defaultValues.push_back( prop );
}

}
