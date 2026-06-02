#include "StdAfx.h"

#include "..\scene\scene.h"
#include "..\common\world.h"
#include "editor.h"
#include "frames.h"
#include "ChapterFrm.h"
#include "ChapterTreeItem.h"
#include "UnitSide.h"

void CChapterTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = E_CHAPTER_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic info";
	child.szDisplayName = "Basic info";
	defaultChilds.push_back( child );

	child.nChildItemType = E_CHAPTER_MISSIONS_ITEM;
	child.szDefaultName = "Missions";
	child.szDisplayName = "Missions";
	defaultChilds.push_back( child );

	child.nChildItemType = E_CHAPTER_PLACES_ITEM;
	child.szDefaultName = "Place holders";
	child.szDisplayName = "Place holders";
	defaultChilds.push_back( child );
}

void CChapterCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	if ( !g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME ) )
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
	prop.szStrings.push_back( g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->GetProjectFileName() );
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
	prop.szStrings.push_back( g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->GetProjectFileName() );
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
	prop.szStrings.push_back( g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->GetProjectFileName() );
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

int CChapterCommonPropsItem::GetSeason()
{
	std::string szVal = values[6].value;
	if ( szVal == "summer" )
		return SEASON_SUMMER;
	if ( szVal == "winter" )
		return SEASON_WINTER;
	if ( szVal == "africa" )
		return SEASON_AFRIKA;
	NI_ASSERT( 0 );
	return 0;
}

void CChapterCommonPropsItem::SetSeason( int nVal )
{
	switch ( nVal )
	{
		case SEASON_SUMMER:
			values[6].value = "summer";
			break;
		case SEASON_WINTER:
			values[6].value = "winter";
			break;
		case SEASON_AFRIKA:
			values[6].value = "africa";
			break;
		default:
			NI_ASSERT( 0 );
	}
}

void CChapterCommonPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 || nItemId == 2 || nItemId == 3 || nItemId == 4 || nItemId == 5 || nItemId == 9 )
	{
		if ( !IsRelatedPath( value ) )
		{
			string szValue = value;
			string szRelatedPath;
			bool bRes =	MakeSubRelativePath( g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->GetProjectFileName().c_str(), szValue.c_str(), szRelatedPath );
			if ( bRes )
			{
				szRelatedPath = szRelatedPath.substr( 0, szRelatedPath.rfind( '.' ) );
				CVariant newVal = szRelatedPath;
				CTreeItem::UpdateItemValue( nItemId, newVal );
				g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->UpdatePropView( this );
			}
			else
			{
				AfxMessageBox( "Error: file should be inside chapter editor project directory" );
				CTreeItem::UpdateItemValue( nItemId, "" );
				g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->UpdatePropView( this );
			}
		}
		
		if ( nItemId == 4 )
		{
			CChapterFrame *pFrame = static_cast<CChapterFrame *> ( g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME ) );
			pFrame->LoadImageTexture( GetMapImage() );
		}

		return;
	}

	if ( nItemId == 8 )
	{
		std::string szValue = "scenarios\\settings\\";
		szValue += (const char *) value;
		CVariant newVal = szValue;
		CTreeItem::UpdateItemValue( nItemId, newVal );
		g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->UpdatePropView( this );
		return;
	}
}

void CChapterCommonPropsItem::MyLButtonClick()
{
	CChapterFrame *pFrame = static_cast<CChapterFrame *> ( g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME ) );
	pFrame->SetActiveMission( 0 );
	pFrame->SetActivePlaceHolder( 0 );
}

void CChapterMissionsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CChapterMissionsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_INSERT:
			CTreeItem *pItem = new CChapterMissionPropsItem;
			pItem->SetItemName( "Mission" );
			AddChild( pItem );
			g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CChapterMissionsItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->DisplayInsertMenu();
	if ( nRes == ID_INSERT_TREE_ITEM )
	{
		CTreeItem *pItem = new CChapterMissionPropsItem;
		pItem->SetItemName( "Mission" );
		AddChild( pItem );
		g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->SetChangedFlag( true );
	}
}

void CChapterMissionsItem::MyLButtonClick()
{
	CChapterFrame *pFrame = static_cast<CChapterFrame *> ( g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME ) );
	pFrame->SetActiveMission( 0 );
	pFrame->SetActivePlaceHolder( 0 );
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

void CChapterMissionPropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->ClearPropView();
			DeleteMeInParentTreeItem();
			g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CChapterMissionPropsItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->DisplayDeleteMenu();
	if ( nRes == ID_MENU_DELETE_TREE_ITEM )
	{
		g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->ClearPropView();
		DeleteMeInParentTreeItem();
		g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->SetChangedFlag( true );
	}
}

void CChapterMissionPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 2 || nItemId == 3 )
	{
		CChapterFrame *pFrame = static_cast<CChapterFrame *> ( g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME ) );
		pFrame->SetActiveMission( this );
	}
}

void CChapterMissionPropsItem::MyLButtonClick()
{
	CChapterFrame *pFrame = static_cast<CChapterFrame *> ( g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME ) );
	pFrame->SetActiveMission( this );
}



void CChapterPlacesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CChapterPlacesItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_INSERT:
			CTreeItem *pItem = new CChapterPlacePropsItem;
			pItem->SetItemName( "Place" );
			AddChild( pItem );
			g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CChapterPlacesItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->DisplayInsertMenu();
	if ( nRes == ID_INSERT_TREE_ITEM )
	{
		CTreeItem *pItem = new CChapterPlacePropsItem;
		pItem->SetItemName( "Place" );
		AddChild( pItem );
		g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->SetChangedFlag( true );
	}
}

void CChapterPlacesItem::MyLButtonClick()
{
	CChapterFrame *pFrame = static_cast<CChapterFrame *> ( g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME ) );
	pFrame->SetActiveMission( 0 );
	pFrame->SetActivePlaceHolder( 0 );
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

void CChapterPlacePropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->ClearPropView();
			DeleteMeInParentTreeItem();
			g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CChapterPlacePropsItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->DisplayDeleteMenu();
	if ( nRes == ID_MENU_DELETE_TREE_ITEM )
	{
		g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->ClearPropView();
		DeleteMeInParentTreeItem();
		g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME )->SetChangedFlag( true );
	}
}

void CChapterPlacePropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 || nItemId == 2 )
	{
		CChapterFrame *pFrame = static_cast<CChapterFrame *> ( g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME ) );
		pFrame->SetActivePlaceHolder( this );
	}
}

void CChapterPlacePropsItem::MyLButtonClick()
{
	CChapterFrame *pFrame = static_cast<CChapterFrame *> ( g_frameManager.GetFrame( CFrameManager::E_CHAPTER_FRAME ) );
	pFrame->SetActivePlaceHolder( this );
}
