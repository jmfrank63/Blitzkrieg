#include "infantry.h"

#include <cstdint>

#include "../../editor_env.h"

namespace NResourceModel
{

void CAnimationTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_UNIT_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ACKS_ITEM;
	child.szDefaultName = "Acknowledgments";
	child.szDisplayName = "Acknowledgments";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_LOCALIZATION_ITEM;
	child.szDefaultName = "Localization";
	child.szDisplayName = "Localization";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ACTIONS_ITEM;
	child.szDefaultName = "Actions";
	child.szDisplayName = "Actions";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_EXPOSURES_ITEM;
	child.szDefaultName = "Exposures";
	child.szDisplayName = "Exposures";
	defaultChilds.push_back( child );

	/*child.nChildItemType = ETIT_UNIT_AI_PROPS_ITEM;
	child.szDefaultName = "AI";
	child.szDisplayName = "AI";
	defaultChilds.push_back( child );*/

	child.nChildItemType = ETIT_UNIT_WEAPON_PROPS_ITEM;
	child.szDefaultName = "Weapon";
	child.szDisplayName = "Weapon";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_GRENADE_PROPS_ITEM;
	child.szDefaultName = "Grenade";
	child.szDisplayName = "Grenade";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_DIRECTORIES_ITEM;
	child.szDefaultName = "Directories";
	child.szDisplayName = "Directories";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATIONS_ITEM;
	child.szDefaultName = "Animations";
	child.szDisplayName = "Animations";
	defaultChilds.push_back( child );
}

void CUnitCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown unit";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Type";
	prop.szDisplayName = "Type";
	prop.value = "soldier";
	prop.szStrings.push_back( "soldier" );
	prop.szStrings.push_back( "engineer" );
	prop.szStrings.push_back( "sniper" );
	prop.szStrings.push_back( "officer" );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 3;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Image";
	prop.szDisplayName = "Image";
	prop.value = "";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Health";
	prop.szDisplayName = "Health";
	prop.value = 100.0f;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Armor";
	prop.szDisplayName = "Armor";
	prop.value = 4;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Camouflage";
	prop.szDisplayName = "Camouflage";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Speed";
	prop.szDisplayName = "Speed";
	prop.value = 2.0f;
	defaultValues.push_back( prop );

	prop.nId = 8;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Passability";
	prop.szDisplayName = "Passability";
	prop.value = 100.0f;
	defaultValues.push_back( prop );

	prop.nId = 9;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Can attack up";
	prop.szDisplayName = "Can attack up";
	prop.value = true;
	defaultValues.push_back( prop );

	prop.nId = 10;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Can attack down";
	prop.szDisplayName = "Can attack down";
	prop.value = true;
	defaultValues.push_back( prop );

	prop.nId = 11;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "AI price";
	prop.szDisplayName = "AI price";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 12;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Sight";
	prop.szDisplayName = "Sight";
	prop.value = 20.0f;
	defaultValues.push_back( prop );

	prop.nId = 13;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Sight power";
	prop.szDisplayName = "Sight power";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
	defaultChilds.clear();
}

void CUnitAIPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CUnitWeaponPropsItem::InitDefaultValues()
{
	defaultValues.clear();

	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_WEAPON_REF;
	prop.szDefaultName = "Weapon name";
	prop.szDisplayName = "Weapon name";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Ammo count";
	prop.szDisplayName = "Ammo count";
	prop.value = 100;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Reload cost";
	prop.szDisplayName = "Reload cost";
	prop.value = 100.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CUnitGrenadePropsItem::InitDefaultValues()
{
	defaultValues.clear();

	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_WEAPON_REF;
	prop.szDefaultName = "Grenade name";
	prop.szDisplayName = "Grenade name";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Ammo count";
	prop.szDisplayName = "Ammo count";
	prop.value = 100;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Reload cost";
	prop.szDisplayName = "Reload cost";
	prop.value = 100.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CDirectoryPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_BROWSEDIR;
	prop.szDefaultName = "Directory";
	prop.szDisplayName = "Directory";
	prop.value = "_.";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CUnitSeasonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_UNIT_DIRECTORY_PROPS_ITEM;
	child.szDefaultName = "Right Up";
	child.szDisplayName = "Right Up";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_DIRECTORY_PROPS_ITEM;
	child.szDefaultName = "Up";
	child.szDisplayName = "Up";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_DIRECTORY_PROPS_ITEM;
	child.szDefaultName = "Left Up";
	child.szDisplayName = "Left Up";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_DIRECTORY_PROPS_ITEM;
	child.szDefaultName = "Left";
	child.szDisplayName = "Left";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_DIRECTORY_PROPS_ITEM;
	child.szDefaultName = "Left Down";
	child.szDisplayName = "Left Down";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_DIRECTORY_PROPS_ITEM;
	child.szDefaultName = "Down";
	child.szDisplayName = "Down";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_DIRECTORY_PROPS_ITEM;
	child.szDefaultName = "Right Down";
	child.szDisplayName = "Right Down";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_DIRECTORY_PROPS_ITEM;
	child.szDefaultName = "Right";
	child.szDisplayName = "Right";
	defaultChilds.push_back( child );
}

void CDirectoriesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_UNIT_SEASON_PROPS_ITEM;
	child.szDefaultName = "Summer";
	child.szDisplayName = "Summer";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_SEASON_PROPS_ITEM;
	child.szDefaultName = "Winter";
	child.szDisplayName = "Winter";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_SEASON_PROPS_ITEM;
	child.szDefaultName = "Africa";
	child.szDisplayName = "Africa";
	defaultChilds.push_back( child );
}

void CUnitAnimationsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Run";
	child.szDisplayName = "Run";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Crawl";
	child.szDisplayName = "Crawl";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Shoot";
	child.szDisplayName = "Shoot";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Shoot down";
	child.szDisplayName = "Shoot down";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Shoot trench";
	child.szDisplayName = "Shoot trench";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Aiming";
	child.szDisplayName = "Aiming";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Aiming down";
	child.szDisplayName = "Aiming down";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Aiming trench";
	child.szDisplayName = "Aiming trench";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Throw";
	child.szDisplayName = "Throw";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Throw down";
	child.szDisplayName = "Throw down";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Throw trench";
	child.szDisplayName = "Throw trench";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Death1";
	child.szDisplayName = "Death";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Death down";
	child.szDisplayName = "Death down";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Prisoning";
	child.szDisplayName = "Prisoning";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Idle";
	child.szDisplayName = "Idle";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Idle down";
	child.szDisplayName = "Idle down";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Idle2";
	child.szDisplayName = "Idle2";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Lie to stand cross";
	child.szDisplayName = "Lie to stand cross";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Stand to lie cross";
	child.szDisplayName = "Stand to lie cross";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Use down";
	child.szDisplayName = "Use down";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Use up";
	child.szDisplayName = "Use up";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Pointing";
	child.szDisplayName = "Pointing";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Binoculars";
	child.szDisplayName = "Binoculars";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ANIMATION_PROPS_ITEM;
	child.szDefaultName = "Radio";
	child.szDisplayName = "Radio";
	defaultChilds.push_back( child );
}

void CUnitAnimationPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Frame time";
	prop.szDisplayName = "Frame time";
	prop.value = 125;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Action frame";
	prop.szDisplayName = "Action frame";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Animation speed";
	prop.szDisplayName = "Animation speed";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Is cycled?";
	prop.szDisplayName = "Is cycled?";
	prop.value = false;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Number of directions";
	prop.szDisplayName = "Number of directions";
	prop.value = "8";
	prop.szStrings.push_back( "8" );
	prop.szStrings.push_back( "4" );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 5;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sound file name";
	prop.szDisplayName = "Sound file name";
	prop.value = "";
	defaultValues.push_back( prop );


	values = defaultValues;
}

void CUnitFramePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CUnitActionsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_ACTION_REF;
	prop.szDefaultName = "Available actions";
	prop.szDisplayName = "Available actions";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CUnitActionPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_ACTION_REF;
	prop.szDefaultName = "Unknown action";
	prop.szDisplayName = "Unknown action";
	prop.value = (std::int64_t) 0;
	defaultValues.push_back( prop );
}

void CUnitExposuresItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_ACTION_REF;
	prop.szDefaultName = "Available exposures";
	prop.szDisplayName = "Available exposures";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CUnitAcksItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_ASK_REF;
	prop.szDefaultName = "Acks file";
	prop.szDisplayName = "Acks set";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_ASK_REF;
	prop.szDefaultName = "Acks file 2";
	prop.szDisplayName = "Acks set 2";
	prop.value = "";
	defaultValues.push_back( prop );
}

}
