#include "StdAfx.h"

#include "editor.h"
#include "frames.h"
#include "MedalFrm.h"
#include "MedalTreeItem.h"

void CMedalTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = E_MEDAL_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic info";
	child.szDisplayName = "Basic info";
	defaultChilds.push_back( child );

/*
	child.nChildItemType = E_MEDAL_PICTURE_PROPS_ITEM;
	child.szDefaultName = "Picture";
	child.szDisplayName = "Picture";
	defaultChilds.push_back( child );

	child.nChildItemType = E_MEDAL_TEXT_PROPS_ITEM;
	child.szDefaultName = "Description";
	child.szDisplayName = "Description";
	defaultChilds.push_back( child );
*/
}

void CMedalCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	if ( !g_frameManager.GetFrame( CFrameManager::E_MEDAL_FRAME ) )
	{
		values = defaultValues;
		return;
	}

	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "name";
	prop.szStrings.push_back( g_frameManager.GetFrame( CFrameManager::E_MEDAL_FRAME )->GetProjectFileName() );
	prop.szStrings.push_back( szTextFilter );
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.szDefaultName = "Description";
	prop.szDisplayName = "Description";
	prop.value = "desc";
	defaultValues.push_back( prop );
	prop.szStrings.clear();
	
	prop.nId = 3;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Medal image";
	prop.szDisplayName = "Medal image";
	prop.value = "medal";
	prop.szStrings.push_back( g_frameManager.GetFrame( CFrameManager::E_MEDAL_FRAME )->GetProjectFileName() );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();
	
	values = defaultValues;
}

void CMedalCommonPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 || nItemId == 2 || nItemId == 3 )
	{
		if ( !IsRelatedPath( value ) )
		{
			string szValue = value;
			string szRelatedPath;
			bool bRes =	MakeSubRelativePath( g_frameManager.GetFrame( CFrameManager::E_MEDAL_FRAME )->GetProjectFileName().c_str(), szValue.c_str(), szRelatedPath );
			if ( bRes )
			{
				szRelatedPath = szRelatedPath.substr( 0, szRelatedPath.rfind( '.' ) );
				CVariant newVal = szRelatedPath;
				CTreeItem::UpdateItemValue( nItemId, newVal );
				g_frameManager.GetFrame( CFrameManager::E_MEDAL_FRAME )->UpdatePropView( this );
			}
			else
			{
				AfxMessageBox( "Error: file should be inside medal editor project directory" );
				CTreeItem::UpdateItemValue( nItemId, "" );
				g_frameManager.GetFrame( CFrameManager::E_MEDAL_FRAME )->UpdatePropView( this );
			}
		}
	}
}

void CMedalPicturePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	if ( !g_frameManager.GetFrame( CFrameManager::E_MEDAL_FRAME ) )
	{
		values = defaultValues;
		return;
	}
	
	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Medal image";
	prop.szDisplayName = "Medal image";
	prop.value = "1";
	prop.szStrings.push_back( g_frameManager.GetFrame( CFrameManager::E_MEDAL_FRAME )->GetProjectFileName() );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();
	
	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Image position X";
	prop.szDisplayName = "Image position X";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Image position Y";
	prop.szDisplayName = "Image position Y";
	prop.value = 0;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CMedalPicturePropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 )
	{
		if ( !IsRelatedPath( value ) )
		{
			string szValue = value;
			string szRelatedPath;
			bool bRes =	MakeSubRelativePath( g_frameManager.GetFrame( CFrameManager::E_MEDAL_FRAME )->GetProjectFileName().c_str(), szValue.c_str(), szRelatedPath );
			if ( bRes )
			{
				szRelatedPath = szRelatedPath.substr( 0, szRelatedPath.rfind( '.' ) );
				CVariant newVal = szRelatedPath;
				CTreeItem::UpdateItemValue( nItemId, newVal );
				g_frameManager.GetFrame( CFrameManager::E_MEDAL_FRAME )->UpdatePropView( this );
			}
			else
			{
				AfxMessageBox( "Error: picture should be inside medal editor project directory" );
				CTreeItem::UpdateItemValue( nItemId, "" );
				g_frameManager.GetFrame( CFrameManager::E_MEDAL_FRAME )->UpdatePropView( this );
			}
		}
	}
}

void CMedalTextPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	if ( !g_frameManager.GetFrame( CFrameManager::E_MEDAL_FRAME ) )
	{
		values = defaultValues;
		return;
	}
	
	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Description text";
	prop.szDisplayName = "Description text";
	prop.value = "desc";
	prop.szStrings.push_back( g_frameManager.GetFrame( CFrameManager::E_MEDAL_FRAME )->GetProjectFileName() );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();
	
/*
	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Text center position X";
	prop.szDisplayName = "Text center position X";
	prop.value = 0;
	defaultValues.push_back( prop );
	
	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Text center position Y";
	prop.szDisplayName = "Text center position Y";
	prop.value = 0;
	defaultValues.push_back( prop );
*/
	
	values = defaultValues;
}

void CMedalTextPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 )
	{
		if ( !IsRelatedPath( value ) )
		{
			string szValue = value;
			string szRelatedPath;
			bool bRes =	MakeSubRelativePath( g_frameManager.GetFrame( CFrameManager::E_MEDAL_FRAME )->GetProjectFileName().c_str(), szValue.c_str(), szRelatedPath );
			if ( bRes )
			{
				szRelatedPath = szRelatedPath.substr( 0, szRelatedPath.rfind( '.' ) );
				CVariant newVal = szRelatedPath;
				CTreeItem::UpdateItemValue( nItemId, newVal );
				g_frameManager.GetFrame( CFrameManager::E_MEDAL_FRAME )->UpdatePropView( this );
			}
			else
			{
				AfxMessageBox( "Error: text file should be inside medal editor project directory" );
				CTreeItem::UpdateItemValue( nItemId, "" );
				g_frameManager.GetFrame( CFrameManager::E_MEDAL_FRAME )->UpdatePropView( this );
			}
		}
	}
}
