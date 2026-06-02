#include "StdAfx.h"

#include "editor.h"
#include "frames.h"
#include "MissionFrm.h"
#include "MissionTreeItem.h"

void CMissionTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = E_MISSION_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic info";
	child.szDisplayName = "Basic info";
	defaultChilds.push_back( child );

	child.nChildItemType = E_MISSION_MUSICS_ITEM;
	child.szDefaultName = "Combat musics";
	child.szDisplayName = "Combat musics";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_MISSION_MUSICS_ITEM;
	child.szDefaultName = "Exploration musics";
	child.szDisplayName = "Exploration musics";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_MISSION_OBJECTIVES_ITEM;
	child.szDefaultName = "Objectives";
	child.szDisplayName = "Objectives";
	defaultChilds.push_back( child );
}

void CMissionCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	if ( !g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME ) )
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
	prop.szStrings.push_back( g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->GetProjectFileName() );
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

void CMissionCommonPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 || nItemId == 2 || nItemId == 3 )
	{
		if ( !IsRelatedPath( value ) )
		{
			string szValue = value;
			string szRelatedPath;
			bool bRes =	MakeSubRelativePath( g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->GetProjectFileName().c_str(), szValue.c_str(), szRelatedPath );
			if ( bRes )
			{
				szRelatedPath = szRelatedPath.substr( 0, szRelatedPath.rfind( '.' ) );
				CVariant newVal = szRelatedPath;
				CTreeItem::UpdateItemValue( nItemId, newVal );
				g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->UpdatePropView( this );
			}
			else
			{
				AfxMessageBox( "Error: file should be inside mission editor project directory" );
				CTreeItem::UpdateItemValue( nItemId, "" );
				g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->UpdatePropView( this );
			}
		}		
	}	
}

void CMissionCommonPropsItem::MyLButtonClick()
{
	CMissionFrame *pFrame = static_cast<CMissionFrame *> ( g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME ) );
	pFrame->SetActiveObjective( 0 );
}

void CMissionObjectivesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CMissionObjectivesItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_INSERT:
			CMissionObjectivePropsItem *pItem = new CMissionObjectivePropsItem;
			pItem->SetItemName( "Objective" );
			string szObjectiveName = NStr::Format( "%d", GetChildsCount() );
			pItem->SetObjeciveText( szObjectiveName.c_str() );
			AddChild( pItem );
			g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CMissionObjectivesItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->DisplayInsertMenu();
	if ( nRes == ID_INSERT_TREE_ITEM )
	{
		CMissionObjectivePropsItem *pItem = new CMissionObjectivePropsItem;
		pItem->SetItemName( "Objective" );
		string szObjectiveName = NStr::Format( "%d", GetChildsCount() );
		pItem->SetObjeciveText( szObjectiveName.c_str() );
		AddChild( pItem );
		g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->SetChangedFlag( true );
	}
}

void CMissionObjectivesItem::MyLButtonClick()
{
	CMissionFrame *pFrame = static_cast<CMissionFrame *> ( g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME ) );
	pFrame->SetActiveObjective( 0 );
}

void CMissionObjectivePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	if ( !g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME ) )
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
	prop.szStrings.push_back( g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->GetProjectFileName() );
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

void CMissionObjectivePropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->ClearPropView();
			DeleteMeInParentTreeItem();
			g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CMissionObjectivePropsItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->DisplayDeleteMenu();
	if ( nRes == ID_MENU_DELETE_TREE_ITEM )
	{
		g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->ClearPropView();
		DeleteMeInParentTreeItem();
		g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->SetChangedFlag( true );
	}
}

void CMissionObjectivePropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 || nItemId == 2 )
	{
		if ( !IsRelatedPath( value ) )
		{
			string szValue = value;
			string szRelatedPath;
			bool bRes =	MakeSubRelativePath( g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->GetProjectFileName().c_str(), szValue.c_str(), szRelatedPath );
			if ( bRes )
			{
				szRelatedPath = szRelatedPath.substr( 0, szRelatedPath.rfind( '.' ) );
				CVariant newVal = szRelatedPath;
				CTreeItem::UpdateItemValue( nItemId, newVal );
				g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->UpdatePropView( this );
			}
			else
			{
				AfxMessageBox( "Error: file should be inside mission editor project directory" );
				CTreeItem::UpdateItemValue( nItemId, "" );
				g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->UpdatePropView( this );
			}
		}
	}

	if ( nItemId == 3 || nItemId == 4 )
	{
		CMissionFrame *pFrame = static_cast<CMissionFrame *> ( g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME ) );
		pFrame->SetActiveObjective( this );
	}
}

void CMissionObjectivePropsItem::MyLButtonClick()
{
	CMissionFrame *pFrame = static_cast<CMissionFrame *> ( g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME ) );
	pFrame->SetActiveObjective( this );
}

void CMissionMusicsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CMissionMusicsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_INSERT:
			CTreeItem *pItem = new CMissionMusicPropsItem;
			pItem->SetItemName( "Music" );
			AddChild( pItem );
			g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CMissionMusicsItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->DisplayInsertMenu();
	if ( nRes == ID_INSERT_TREE_ITEM )
	{
		CTreeItem *pItem = new CMissionMusicPropsItem;
		pItem->SetItemName( "Music" );
		AddChild( pItem );
		g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->SetChangedFlag( true );
	}
}

void CMissionMusicsItem::MyLButtonClick()
{
	CMissionFrame *pFrame = static_cast<CMissionFrame *> ( g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME ) );
	pFrame->SetActiveObjective( 0 );
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

void CMissionMusicPropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->ClearPropView();
			DeleteMeInParentTreeItem();
			g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CMissionMusicPropsItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->DisplayDeleteMenu();
	if ( nRes == ID_MENU_DELETE_TREE_ITEM )
	{
		g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->ClearPropView();
		DeleteMeInParentTreeItem();
		g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME )->SetChangedFlag( true );
	}
}

void CMissionMusicPropsItem::MyLButtonClick()
{
	CMissionFrame *pFrame = static_cast<CMissionFrame *> ( g_frameManager.GetFrame( CFrameManager::E_MISSION_FRAME ) );
	pFrame->SetActiveObjective( 0 );
}
