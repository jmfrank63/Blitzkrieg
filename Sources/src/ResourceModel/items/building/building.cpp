#include "building.h"

#include "../../combos.h"
#include "../../editor_env.h"

namespace NResourceModel
{

void CBuildingTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_BUILDING_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_PASSES_ITEM;
	child.szDefaultName = "AI classes to pass";
	child.szDisplayName = "AI classes to pass";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_DEFENCES_ITEM;
	child.szDefaultName = "Defence";
	child.szDisplayName = "Defence";
	defaultChilds.push_back( child );

	/*child.nChildItemType = ETIT_BUILDING_ENTRANCES_ITEM;
	child.szDefaultName = "Entrances";
	child.szDisplayName = "Entrances";
	defaultChilds.push_back( child );*/

	child.nChildItemType = ETIT_BUILDING_SLOTS_ITEM;
	child.szDefaultName = "Slots";
	child.szDisplayName = "Shoot slots";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_FIRE_POINTS_ITEM;
	child.szDefaultName = "Fire points";
	child.szDisplayName = "Fire points";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_DIR_EXPLOSIONS_ITEM;
	child.szDefaultName = "Direction explosions";
	child.szDisplayName = "Direction explosions";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_SMOKES_ITEM;
	child.szDefaultName = "Smoke points";
	child.szDisplayName = "Smoke points";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_GRAPHICS_ITEM;
	child.szDefaultName = "Graphics Info";
	child.szDisplayName = "Graphics Info";
	defaultChilds.push_back( child );
}

void CBuildingCommonPropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown Building";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Building type";
	prop.szDisplayName = "Building type";
	prop.value = "building";
	prop.szStrings.push_back( "building" );
	prop.szStrings.push_back( "main storage" );
	prop.szStrings.push_back( "temporary storage" );
	prop.szStrings.push_back( "dot" );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Health";
	prop.szDisplayName = "Health";
	prop.value = 1500;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Repair cost";
	prop.szDisplayName = "Repair cost";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Number of rest slots";
	prop.szDisplayName = "Number of rest slots";
	prop.value = 40;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Number of medical slots";
	prop.szDisplayName = "Number of medical slots";
	prop.value = 40;
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_SOUND_REF;
	prop.szDefaultName = "Ambient sound";
	prop.szDisplayName = "Sound";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 8;
	prop.nDomenType = DT_SOUND_REF;
	prop.szDefaultName = "Cycled sound";
	prop.szDisplayName = "Cycled sound";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CBuildingPassesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CBuildingPassPropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "AI class to pass";
	prop.szDisplayName = "AI class to pass";
	prop.value = "";
	LoadAIClassCombo( &prop );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;
}

void CBuildingEntrancesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CBuildingEntrancePropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "AI position";
	prop.szDisplayName = "AI position";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 1;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Stormable";
	prop.szDisplayName = "Stormable";
	prop.value = true;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CBuildingSlotsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CBuildingSlotPropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Direction";
	prop.szDisplayName = "Direction";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Angle";
	prop.szDisplayName = "Angle";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Sight multiplier";
	prop.szDisplayName = "Sight multiplier";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Cover";
	prop.szDisplayName = "Cover";
	prop.value = 0.3f;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_WEAPON_REF;
	prop.szDefaultName = "Build in weapon";
	prop.szDisplayName = "Build in weapon";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Ammo";
	prop.szDisplayName = "Ammo";
	prop.value = 100;
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Rotation speed";
	prop.szDisplayName = "Rotation speed";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 8;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Priority";
	prop.szDisplayName = "Priority";
	prop.value = 1;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CBuildingGraphicsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_BUILDING_SUMMER_PROPS_ITEM;
	child.szDefaultName = "Summer";
	child.szDisplayName = "Summer";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_WINTER_PROPS_ITEM;
	child.szDefaultName = "Winter";
	child.szDisplayName = "Winter";
	defaultChilds.push_back( child );
}

void CBuildingSummerPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_BUILDING_GRAPHIC1_PROPS_ITEM;
	child.szDefaultName = "Whole";
	child.szDisplayName = "Whole";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_GRAPHIC2_PROPS_ITEM;
	child.szDefaultName = "Damaged";
	child.szDisplayName = "Damaged";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_GRAPHIC3_PROPS_ITEM;
	child.szDefaultName = "Destroyed";
	child.szDisplayName = "Destroyed";
	defaultChilds.push_back( child );
}

void CBuildingWinterPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_BUILDING_GRAPHICW1_PROPS_ITEM;
	child.szDefaultName = "Whole";
	child.szDisplayName = "Whole";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_GRAPHICW2_PROPS_ITEM;
	child.szDefaultName = "Damaged";
	child.szDisplayName = "Damaged";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_GRAPHICW3_PROPS_ITEM;
	child.szDefaultName = "Destroyed";
	child.szDisplayName = "Destroyed";
	defaultChilds.push_back( child );
}

void CBuildingGraphic1PropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sprite";
	prop.szDisplayName = "Sprite";
	prop.value = "1.tga";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Shadow";
	prop.szDisplayName = "Shadow";
	prop.value = "1s.tga";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;
}

void CBuildingGraphic2PropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sprite";
	prop.szDisplayName = "Sprite";
	prop.value = "2.tga";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Shadow";
	prop.szDisplayName = "Shadow";
	prop.value = "2s.tga";
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Noise file";
	prop.szDisplayName = "Noise file";
	prop.value = "2g.tga";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;
}

void CBuildingGraphic3PropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sprite";
	prop.szDisplayName = "Sprite";
	prop.value = "3.tga";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Shadow";
	prop.szDisplayName = "Shadow";
	prop.value = "3s.tga";
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Noise file";
	prop.szDisplayName = "Noise file";
	prop.value = "3g.tga";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;
}

void CBuildingGraphicW1PropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sprite";
	prop.szDisplayName = "Sprite";
	prop.value = "1w.tga";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Shadow";
	prop.szDisplayName = "Shadow";
	prop.value = "1ws.tga";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;
}

void CBuildingGraphicW2PropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sprite";
	prop.szDisplayName = "Sprite";
	prop.value = "2w.tga";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Shadow";
	prop.szDisplayName = "Shadow";
	prop.value = "2ws.tga";
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Noise file";
	prop.szDisplayName = "Noise file";
	prop.value = "2wg.tga";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;
}

void CBuildingGraphicW3PropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sprite";
	prop.szDisplayName = "Sprite";
	prop.value = "3w.tga";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Shadow";
	prop.szDisplayName = "Shadow";
	prop.value = "3ws.tga";
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Noise file";
	prop.szDisplayName = "Noise file";
	prop.value = "3wg.tga";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;
}

void CBuildingDefencesItem::InitDefaultValues()
{
	values.clear();
	defaultValues = values;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_BUILDING_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Front";
	child.szDisplayName = "Front";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Left";
	child.szDisplayName = "Left";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Right";
	child.szDisplayName = "Right";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Back";
	child.szDisplayName = "Back";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Top";
	child.szDisplayName = "Top";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Bottom";
	child.szDisplayName = "Bottom";
	defaultChilds.push_back( child );
}

void CBuildingDefencePropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Min armor";
	prop.szDisplayName = "Min armor";
	prop.value = 40;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Max armor";
	prop.szDisplayName = "Max armor";
	prop.value = 90;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Silhouette";
	prop.szDisplayName = "Silhouette";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CBuildingFirePointsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CBuildingFirePointPropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Direction";
	prop.szDisplayName = "Direction";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Fire effect";
	prop.szDisplayName = "Fire effect";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Vertical angle";
	prop.szDisplayName = "Vertical angle";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CBuildingDirExplosionsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Effect explosion";
	prop.szDisplayName = "Effect explosion";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_BUILDING_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Front left";
	child.szDisplayName = "Front left";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Front right";
	child.szDisplayName = "Front right";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Back right";
	child.szDisplayName = "Back right";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Back left";
	child.szDisplayName = "Back left";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_BUILDING_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Top center";
	child.szDisplayName = "Top center";
	defaultChilds.push_back( child );
}

void CBuildingDirExplosionPropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Direction";
	prop.szDisplayName = "Direction";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Vertical angle";
	prop.szDisplayName = "Vertical angle";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CBuildingSmokesItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Effect explosion";
	prop.szDisplayName = "Effect explosion";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CBuildingSmokePropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Direction";
	prop.szDisplayName = "Direction";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Vertical angle";
	prop.szDisplayName = "Vertical angle";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

}
