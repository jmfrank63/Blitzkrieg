#include "mission.h"

#include "../../editor_env.h"

namespace NResourceModel
{

void CMissionTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_MISSION_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic info";
	child.szDisplayName = "Basic info";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MISSION_MUSICS_ITEM;
	child.szDefaultName = "Combat musics";
	child.szDisplayName = "Combat musics";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MISSION_MUSICS_ITEM;
	child.szDefaultName = "Exploration musics";
	child.szDisplayName = "Exploration musics";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_MISSION_OBJECTIVES_ITEM;
	child.szDefaultName = "Objectives";
	child.szDisplayName = "Objectives";
	defaultChilds.push_back( child );
}

void CMissionCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	if ( !HasEditorFrame() )
	{
		values = defaultValues;
		return;
	}

	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Header text";
	prop.szDisplayName = "Header text";
	prop.value = "header";
	prop.szStrings.push_back( GetFrameProjectFileName() );
	prop.szStrings.push_back( szTextFilter );
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "SubHeader text";
	prop.szDisplayName = "SubHeader text";
	prop.value = "subheader";
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Description text";
	prop.szDisplayName = "Description text";
	prop.value = "desc";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 4;
	prop.nDomenType = DT_MAP_REF;
	prop.szDefaultName = "Template map";
	prop.szDisplayName = "Template map";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_MAP_REF;
	prop.szDefaultName = "Final map";
	prop.szDisplayName = "Final map";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_SETTING_REF;
	prop.szDefaultName = "Settings file";
	prop.szDisplayName = "Settings file";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CMissionObjectivesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CMissionObjectivePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	if ( !HasEditorFrame() )
	{
		values = defaultValues;
		return;
	}

	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Objective header";
	prop.szDisplayName = "Objective header";
	prop.value = "";
	prop.szStrings.push_back( GetFrameProjectFileName() );
	prop.szStrings.push_back( szTextFilter );
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Objective text";
	prop.szDisplayName = "Objective text";
	prop.value = "1";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Objective position X";
	prop.szDisplayName = "Objective position X";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Objective position Y";
	prop.szDisplayName = "Objective position Y";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Objective secret flag";
	prop.szDisplayName = "Objective secret flag";
	prop.value = true;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Objective script ID";
	prop.szDisplayName = "Objective script ID";
	prop.value = -1;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CMissionMusicsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CMissionMusicPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_MUSIC_REF;
	prop.szDefaultName = "Music file";
	prop.szDisplayName = "Music reference";
	prop.value = "";
	defaultValues.push_back( prop );
}

}
