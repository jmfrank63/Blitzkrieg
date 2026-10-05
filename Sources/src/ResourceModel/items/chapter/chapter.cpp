#include "chapter.h"

#include "../../combos.h"
#include "../../editor_env.h"

namespace NResourceModel
{

void CChapterTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_CHAPTER_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic info";
	child.szDisplayName = "Basic info";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_CHAPTER_MISSIONS_ITEM;
	child.szDefaultName = "Missions";
	child.szDisplayName = "Missions";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_CHAPTER_PLACES_ITEM;
	child.szDefaultName = "Place holders";
	child.szDisplayName = "Place holders";
	defaultChilds.push_back( child );
}

void CChapterCommonPropsItem::InitDefaultValues()
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
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Map image";
	prop.szDisplayName = "Map image";
	prop.value = "map";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 5;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Chapter script";
	prop.szDisplayName = "Chapter script";
	prop.value = "";
	prop.szStrings.push_back( GetFrameProjectFileName() );
	prop.szStrings.push_back( szLuaFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 6;
	prop.nDomenType = DT_MUSIC_REF;
	prop.szDefaultName = "Interface music";
	prop.szDisplayName = "Interface music";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Season";
	prop.szDisplayName = "Season";
	prop.value = "summer";
	prop.szStrings.push_back( "summer" );
	prop.szStrings.push_back( "winter" );
	prop.szStrings.push_back( "africa" );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 8;
	prop.nDomenType = DT_SETTING_REF;
	prop.szDefaultName = "Setting file";
	prop.szDisplayName = "Setting file";
	prop.value = "";
	prop.szStrings.push_back( GetFrameProjectFileName() );
	prop.szStrings.push_back( szXMLFilter );
	defaultValues.push_back( prop );

	prop.nId = 9;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Context file";
	prop.szDisplayName = "Context file";
	prop.value = "context";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 10;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Player side";
	prop.szDisplayName = "Player side";
	prop.value = "German";
	FillVectorOfSides( prop.szStrings );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;
}

void CChapterMissionsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CChapterMissionPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_SCENARIO_MISSION_REF;
	prop.szDefaultName = "Mission";
	prop.szDisplayName = "Mission";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Mission position X";
	prop.szDisplayName = "Mission position X";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Mission position Y";
	prop.szDisplayName = "Mission position Y";
	prop.value = 0;
	defaultValues.push_back( prop );
}

void CChapterPlacesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CChapterPlacePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Place holder position X";
	prop.szDisplayName = "Place holder position X";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Place holder position Y";
	prop.szDisplayName = "Place holder position Y";
	prop.value = 0;
	defaultValues.push_back( prop );
}

}
