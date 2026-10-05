#include "mesh.h"

#include "../../combos.h"
#include "../../editor_env.h"

namespace NResourceModel
{

void CMeshTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_MESH_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_UNIT_ACKS_ITEM;
	child.szDefaultName = "Acknowledgments";
	child.szDisplayName = "Acknowledgments";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MESH_EFFECTS_ITEM;
	child.szDefaultName = "Effects";
	child.szDisplayName = "Effects";
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

	child.nChildItemType = ETIT_MESH_DEFENCES_ITEM;
	child.szDefaultName = "Defences";
	child.szDisplayName = "Defences";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MESH_JOGGINGS_ITEM;
	child.szDefaultName = "Joggings";
	child.szDisplayName = "Joggings";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MESH_PLATFORMS_ITEM;
	child.szDefaultName = "Platforms";
	child.szDisplayName = "Platforms";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MESH_GRAPHICS_ITEM;
	child.szDefaultName = "Graphics Info";
	child.szDisplayName = "Graphics Info";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MESH_LOCATORS_ITEM;
	child.szDefaultName = "Locators";
	child.szDisplayName = "Locators";
	defaultChilds.push_back( child );
}

void CMeshCommonPropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Default mesh object";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Type";
	prop.szDisplayName = "Type";
	prop.value = "armor light";

	prop.szStrings.push_back( "transport carrier" );
	prop.szStrings.push_back( "transport support" );
	prop.szStrings.push_back( "transport medicine" );
	prop.szStrings.push_back( "transport tractor" );
	prop.szStrings.push_back( "transport military auto" );
	prop.szStrings.push_back( "transport civilian auto" );

	prop.szStrings.push_back( "artillery gun" );
	prop.szStrings.push_back( "artillery howitzer" );
	prop.szStrings.push_back( "artillery heavy gun" );
	prop.szStrings.push_back( "artillery heavy machine gun" );
	prop.szStrings.push_back( "artillery antiair gun" );
	prop.szStrings.push_back( "artillery rocket" );
	prop.szStrings.push_back( "artillery super" );
	prop.szStrings.push_back( "artillery mortar" );

	prop.szStrings.push_back( "SPG assault" );
	prop.szStrings.push_back( "SPG antitank" );
	prop.szStrings.push_back( "SPG super" );
	prop.szStrings.push_back( "SPG antiair" );

	prop.szStrings.push_back( "armor light" );
	prop.szStrings.push_back( "armor medium" );
	prop.szStrings.push_back( "armor super" );
	prop.szStrings.push_back( "armor heavy" );

	prop.szStrings.push_back( "avia scout" );
	prop.szStrings.push_back( "avia bomber" );
	prop.szStrings.push_back( "avia attack" );
	prop.szStrings.push_back( "avia fighter" );
	prop.szStrings.push_back( "avia super" );
	prop.szStrings.push_back( "avia lander" );

	prop.szStrings.push_back( "train locomotive" );
	prop.szStrings.push_back( "train cargo" );
	prop.szStrings.push_back( "train carrier" );
	prop.szStrings.push_back( "train super" );
	prop.szStrings.push_back( "train armor" );

	defaultValues.push_back( prop );
	prop.szStrings.clear();


	prop.nId = 3;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "AI class";
	prop.szDisplayName = "AI class";
	prop.value = "track";
	LoadAIClassCombo( &prop );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 4;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Picture";
	prop.szDisplayName = "Picture";
	prop.value = "";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 5;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Health";
	prop.szDisplayName = "Health";
	prop.value = 100.0f;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Repair cost";
	prop.szDisplayName = "Repair cost";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Camouflage";
	prop.szDisplayName = "Camouflage";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 8;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Speed";
	prop.szDisplayName = "Speed";
	prop.value = 5.0f;
	defaultValues.push_back( prop );

	prop.nId = 9;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Passability";
	prop.szDisplayName = "Passability";
	prop.value = 100.0f;
	defaultValues.push_back( prop );

	prop.nId = 10;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Pulling power";
	prop.szDisplayName = "Pulling power";
	prop.value = 3.0f;
	defaultValues.push_back( prop );

	prop.nId = 11;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Uninstall rotate time";
	prop.szDisplayName = "Uninstall rotate time";
	prop.value = 5.0f;
	defaultValues.push_back( prop );

	prop.nId = 12;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Uninstall transport time";
	prop.szDisplayName = "Uninstall transport time";
	prop.value = 5.0f;
	defaultValues.push_back( prop );

	prop.nId = 13;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Weight";
	prop.szDisplayName = "Weight";
	prop.value = 20.0f;
	defaultValues.push_back( prop );

	prop.nId = 14;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Crew";
	prop.szDisplayName = "Crew";
	prop.value = 4;
	defaultValues.push_back( prop );

	prop.nId = 15;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Passangers";
	prop.szDisplayName = "Passangers";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 16;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Priority";
	prop.szDisplayName = "Priority";
	prop.value = 1;
	defaultValues.push_back( prop );

	prop.nId = 17;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Rotate speed";
	prop.szDisplayName = "Rotate speed";
	prop.value = 20.0f;
	defaultValues.push_back( prop );

	prop.nId = 18;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Bound tile radius";
	prop.szDisplayName = "Bound tile radius";
	prop.value = 1;
	defaultValues.push_back( prop );

	prop.nId = 19;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Turn radius";
	prop.szDisplayName = "Turn radius";
	prop.value = 5.0f;
	defaultValues.push_back( prop );

	prop.nId = 20;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Silhouette";
	prop.szDisplayName = "Silhouette";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 21;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "AI price";
	prop.szDisplayName = "AI price";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 22;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Sight";
	prop.szDisplayName = "Sight";
	prop.value = 20.0f;
	defaultValues.push_back( prop );

	prop.nId = 23;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Sight power";
	prop.szDisplayName = "Sight power";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	values = defaultValues;


	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_MESH_AVIA_ITEM;
	child.szDefaultName = "Aviation property";
	child.szDisplayName = "Aviation property";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MESH_TRACK_ITEM;
	child.szDefaultName = "Tracks";
	child.szDisplayName = "Tracks";
	defaultChilds.push_back( child );
}

void CMeshSoundPropsItem::InitDefaultValues()
{
	values.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sound";
	prop.szDisplayName = "Sound";
	prop.value = "";
	prop.szStrings.push_back( GetEditorDataDir() + "sounds\\" );
	prop.szStrings.push_back( szSoundFilter );
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Sound min distance";
	prop.szDisplayName = "Sound min distance";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Sound max distance";
	prop.szDisplayName = "Sound max distance";
	prop.value = 2.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CMeshAviaItem::InitDefaultValues()
{
	values.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Maximal flight height";
	prop.szDisplayName = "Maximal flight height";
	prop.value = 500.0f;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Diving angle";
	prop.szDisplayName = "Diving angle";
	prop.value = 45.0f;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Climb angle";
	prop.szDisplayName = "Climb angle";
	prop.value = 45.0f;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Tilt angle";
	prop.szDisplayName = "Tilt angle";
	prop.value = 30.0f;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Tilt ratio";
	prop.szDisplayName = "Tilt ratio";
	prop.value = 2.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CMeshTrackItem::InitDefaultValues()
{
	values.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Does leave tracks?";
	prop.szDisplayName = "Does leave tracks?";
	prop.value = false;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Relative width";
	prop.szDisplayName = "Relative width";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Relative offset";
	prop.szDisplayName = "Relative offset";
	prop.value = 0.1f;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Relative start";
	prop.szDisplayName = "Relative start";
	prop.value = 0.1f;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Relative end";
	prop.szDisplayName = "Relative end";
	prop.value = 0.1f;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Intensity";
	prop.szDisplayName = "Intensity";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Life time";
	prop.szDisplayName = "Life time";
	prop.value = 100;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CMeshEffectsItem::InitDefaultValues()
{
	values.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Effect diesel";
	prop.szDisplayName = "Effect diesel";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Effect smoke";
	prop.szDisplayName = "Effect smoke";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Effect wheel dust";
	prop.szDisplayName = "Effect wheel dust";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Effect shoot dust";
	prop.szDisplayName = "Effect shoot dust";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Fatality effect";
	prop.szDisplayName = "Fatality effect";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Disappear effect";
	prop.szDisplayName = "Disappear effect";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_SOUND_REF;
	prop.szDefaultName = "Start move sound";
	prop.szDisplayName = "Start move sound";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 8;
	prop.nDomenType = DT_SOUND_REF;
	prop.szDefaultName = "Cycle move sound";
	prop.szDisplayName = "Cycle move sound";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 9;
	prop.nDomenType = DT_SOUND_REF;
	prop.szDefaultName = "Stop move sound";
	prop.szDisplayName = "Stop move sound";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CMeshDefencesItem::InitDefaultValues()
{
	values.clear();
	defaultValues = values;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_MESH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Front";
	child.szDisplayName = "Front";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MESH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Left";
	child.szDisplayName = "Left";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MESH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Right";
	child.szDisplayName = "Right";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MESH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Back";
	child.szDisplayName = "Back";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MESH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Top";
	child.szDisplayName = "Top";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MESH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Bottom";
	child.szDisplayName = "Bottom";
	defaultChilds.push_back( child );
}

void CMeshDefencePropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Min armor";
	prop.szDisplayName = "Min armor";
	prop.value = 4;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Max armor";
	prop.szDisplayName = "Max armor";
	prop.value = 4;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CMeshJoggingsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_MESH_JOGGING_PROPS_ITEM;
	child.szDefaultName = "X Jogging";
	child.szDisplayName = "X Jogging";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MESH_JOGGING_PROPS_ITEM;
	child.szDefaultName = "Y Jogging";
	child.szDisplayName = "Y Jogging";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MESH_JOGGING_PROPS_ITEM;
	child.szDefaultName = "Z Jogging";
	child.szDisplayName = "Z Jogging";
	defaultChilds.push_back( child );
}

void CMeshJoggingPropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Period 1";
	prop.szDisplayName = "Period 1";
	prop.value = 1.0;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Period 2";
	prop.szDisplayName = "Period 2";
	prop.value = 1.0;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Amplitude 1";
	prop.szDisplayName = "Amplitude 1";
	prop.value = 1.0;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Amplitude 2";
	prop.szDisplayName = "Amplitude 2";
	prop.value = 1.0;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Phase 1";
	prop.szDisplayName = "Phase 1";
	prop.value = 1.0;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Phase 2";
	prop.szDisplayName = "Phase 2";
	prop.value = 1.0;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CMeshPlatformsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_MESH_PLATFORM_PROPS_ITEM;
	child.szDefaultName = "Base";
	child.szDisplayName = "Base";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MESH_PLATFORM_PROPS_ITEM;
	child.szDefaultName = "Turret";
	child.szDisplayName = "Turret";
	defaultChilds.push_back( child );
}

void CMeshPlatformPropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Part";
	prop.szDisplayName = "Part";
	prop.value = "NA";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Gun carriage 1";
	prop.szDisplayName = "Gun carriage 1";
	prop.value = "NA";
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Gun carriage 2";
	prop.szDisplayName = "Gun carriage 2";
	prop.value = "NA";
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Vertical rotation speed";
	prop.szDisplayName = "Vertical rotation speed";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Horizontal rotation speed";
	prop.szDisplayName = "Horizontal rotation speed";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Rotation sound";
	prop.szDisplayName = "Rotation sound";
	prop.value = "";
	prop.szStrings.push_back( GetEditorDataDir() );
	prop.szStrings.push_back( szSoundFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_MESH_GUNS_ITEM;
	child.szDefaultName = "Guns";
	child.szDisplayName = "Guns";
	defaultChilds.push_back( child );
}

void CMeshGunsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
}

void CMeshGunPropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Shoot point";
	prop.szDisplayName = "Shoot point";
	prop.value = "NA";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Shoot part";
	prop.szDisplayName = "Shoot part";
	prop.value = "NA";
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_WEAPON_REF;
	prop.szDefaultName = "Weapon";
	prop.szDisplayName = "Weapon";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Gun priority";
	prop.szDisplayName = "Gun priority";
	prop.value = 1;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Recoil flag";
	prop.szDisplayName = "Recoil flag";
	prop.value = false;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Recoil time";
	prop.szDisplayName = "Recoil time";
	prop.value = 50;
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Recoil shake time";
	prop.szDisplayName = "Recoil shake time";
	prop.value = 500;
	defaultValues.push_back( prop );

	prop.nId = 8;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Recoil shake angle";
	prop.szDisplayName = "Recoil shake angle";
	prop.value = 0.0f;
	defaultValues.push_back( prop );

	prop.nId = 9;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Ammo count";
	prop.szDisplayName = "Ammo count";
	prop.value = 100;
	defaultValues.push_back( prop );

	prop.nId = 10;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Reload cost";
	prop.szDisplayName = "Reload cost";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CMeshGraphicsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "3D model ready";
	prop.szDisplayName = "3D model combat";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szMODFilter );
	prop.value = "1.mod";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "3D model install";
	prop.szDisplayName = "3D model install";
	prop.value = "2.mod";
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "3D model transportable";
	prop.szDisplayName = "3D model transportable";
	prop.value = "3.mod";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 4;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Alive summer texture";
	prop.szDisplayName = "Alive summer texture";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	prop.value = "1.tga";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 5;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Alive winter texture";
	prop.szDisplayName = "Alive winter texture";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	prop.value = "1w.tga";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 6;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Alive afrika texture";
	prop.szDisplayName = "Alive afrika texture";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	prop.value = "1a.tga";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 7;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Dead summer texture";
	prop.szDisplayName = "Dead summer texture";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	prop.value = "2.tga";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 8;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Dead winter texture";
	prop.szDisplayName = "Dead winter texture";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	prop.value = "2w.tga";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 9;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Dead afrika texture";
	prop.szDisplayName = "Dead afrika texture";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	prop.value = "2a.tga";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_MESH_DEATH_CRATERS_ITEM;
	child.szDefaultName = "Death craters";
	child.szDisplayName = "Death craters";
	defaultChilds.push_back( child );
}

void CMeshLocatorsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CMeshLocatorPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CMeshDeathCratersItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSEDIR;
	prop.szDefaultName = "Craters directory";
	prop.szDisplayName = "Craters directory";
	prop.value = "";
	std::string szDir = GetEditorDataDir();
	szDir += "TerraObjects\\Death_Hole\\";
	prop.szStrings.push_back( szDir.c_str() );
	defaultValues.push_back( prop );

	values = defaultValues;

	defaultChilds.clear();
}

void CMeshDeathCraterPropsItem::InitDefaultValues()
{
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_DEATH_REF;
	prop.szDefaultName = "Crater file";
	prop.szDisplayName = "Crater reference";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

}
