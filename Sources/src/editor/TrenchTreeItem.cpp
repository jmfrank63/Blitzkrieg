#include "StdAfx.h"
#include <io.h>

#include "../GFX/gfx.h"
#include "../Image/image.h"

#include "editor.h"
#include "frames.h"
#include "TrenchFrm.h"
#include "TrenchTreeItem.h"
#include "common.h"

const static string szModFilter = "Model files (*.mod)|*.mod||";

void CTrenchTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
	
	defaultChilds.clear();

	SChildItem child;
	
	child.nChildItemType = E_TRENCH_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );

	child.nChildItemType = E_TRENCH_DEFENCES_ITEM;
	child.szDefaultName = "Defences";
	child.szDisplayName = "Defences";
	defaultChilds.push_back( child );

	child.nChildItemType = E_TRENCH_SOURCES_ITEM;
	child.szDefaultName = "Trenches with embrasure";
	child.szDisplayName = "Trenches with embrasure";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_TRENCH_SOURCES_ITEM;
	child.szDefaultName = "Trenches line";
	child.szDisplayName = "Trenches line";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_TRENCH_SOURCES_ITEM;
	child.szDefaultName = "Trench ends";
	child.szDisplayName = "Trench ends";
	defaultChilds.push_back( child );

	child.nChildItemType = E_TRENCH_SOURCES_ITEM;
	child.szDefaultName = "Trench arcs";
	child.szDisplayName = "Trench arcs";
	defaultChilds.push_back( child );
}

void CTrenchCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown Trench";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Health";
	prop.szDisplayName = "Health";
	prop.value = 100;
	defaultValues.push_back( prop );
	
	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Rest slots";
	prop.szDisplayName = "Rest slots";
	prop.value = 4;
	defaultValues.push_back( prop );
	
	prop.nId = 4;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Medical slots";
	prop.szDisplayName = "Medical slots";
	prop.value = 1;
	defaultValues.push_back( prop );
	
	prop.nId = 5;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Cover";
	prop.szDisplayName = "Silhouette";
	prop.value = 1.0f;
	defaultValues.push_back( prop );
	
	values = defaultValues;
}

void CTrenchSourcesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CTrenchSourcesItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_INSERT:
			CTrenchSourcePropsItem *pNewTrenchSourceProps = new CTrenchSourcePropsItem;
			pNewTrenchSourceProps->SetItemName( "Trench" );
			CTrenchFrame *pFrame = static_cast<CTrenchFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TRENCH_FRAME ) );
			pNewTrenchSourceProps->nTrenchIndex = pFrame->GetFreeTrenchIndex();
			AddChild( pNewTrenchSourceProps );
			pFrame->SetChangedFlag( true );
	}
}

void CTrenchSourcesItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_TRENCH_FRAME )->DisplayInsertMenu();
	if ( nRes == ID_INSERT_TREE_ITEM )
	{
		CTrenchSourcePropsItem *pNewTrenchSourceProps = new CTrenchSourcePropsItem;
		pNewTrenchSourceProps->SetItemName( "Trench" );
		CTrenchFrame *pFrame = static_cast<CTrenchFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TRENCH_FRAME ) );
		pNewTrenchSourceProps->nTrenchIndex = pFrame->GetFreeTrenchIndex();
		AddChild( pNewTrenchSourceProps );
		pFrame->SetChangedFlag( true );
	}
}

int CTrenchSourcePropsItem::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CTreeItem*>(this) );
	saver.Add( "TrenchIndex", &nTrenchIndex );
	return 0;
}

void CTrenchSourcePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Source file";
	prop.szDisplayName = "Source file";
	prop.value = "";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szModFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 2;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Coverage";
	prop.szDisplayName = "Coverage";
	prop.value = 0.2f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CTrenchSourcePropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			CTrenchFrame *pFrame = static_cast<CTrenchFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TRENCH_FRAME ) );
			pFrame->RemoveTrenchIndex( nTrenchIndex );
			DeleteMeInParentTreeItem();
			pFrame->SetChangedFlag( true );
			break;
	}
}

void CTrenchSourcePropsItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_TRENCH_FRAME )->DisplayDeleteMenu();
	if ( nRes == ID_MENU_DELETE_TREE_ITEM )
	{
		CTrenchFrame *pFrame = static_cast<CTrenchFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TRENCH_FRAME ) );
		pFrame->RemoveTrenchIndex( nTrenchIndex );
		DeleteMeInParentTreeItem();
		pFrame->SetChangedFlag( true );
	}
}

void CTrenchSourcePropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 )
	{
		if ( !IsRelatedPath( value ) )
		{
			string szProjectName = g_frameManager.GetFrame( CFrameManager::E_TRENCH_FRAME )->GetProjectFileName();
			string szValue = value;
			string szRelatedPath;
			bool bRes = MakeRelativePath( szProjectName.c_str(), szValue.c_str(), szRelatedPath );
			if ( bRes )
			{
				CVariant newVal = szRelatedPath;
				CTreeItem::UpdateItemValue( nItemId, newVal );
				g_frameManager.GetFrame( CFrameManager::E_TRENCH_FRAME )->UpdatePropView( this );
			}
			else
			{
				AfxMessageBox( "Note, this project will not be portable on other computers,\nproject file name and model files should be on the same drive" );
			}
		}

		return;
	}
}

void CTrenchDefencesItem::InitDefaultValues()
{
	values.clear();
	defaultValues = values;

	defaultChilds.clear();
	SChildItem child;
			
	child.nChildItemType = E_TRENCH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Front";
	child.szDisplayName = "Front";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_TRENCH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Left";
	child.szDisplayName = "Left";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_TRENCH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Right";
	child.szDisplayName = "Right";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_TRENCH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Back";
	child.szDisplayName = "Back";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_TRENCH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Top";
	child.szDisplayName = "Top";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_TRENCH_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Bottom";
	child.szDisplayName = "Bottom";
	defaultChilds.push_back( child );
}

void CTrenchDefencePropsItem::InitDefaultValues()
{
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Min armor";
	prop.szDisplayName = "Min armor";
	prop.value = 300;
	defaultValues.push_back( prop );
	
	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Max armor";
	prop.szDisplayName = "Max armor";
	prop.value = 300;
	defaultValues.push_back( prop );
	
	values = defaultValues;
}
