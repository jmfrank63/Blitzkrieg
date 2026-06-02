#include "StdAfx.h"
#include <io.h>

#include "editor.h"
#include "frames.h"
#include "ObjectFrm.h"
#include "ObjTreeItem.h"
#include "BuildCompose.h"
#include "Reference.h"

void CObjectTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = E_OBJECT_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );

	child.nChildItemType = E_OBJECT_PASSES_ITEM;
	child.szDefaultName = "AI classes to pass";
	child.szDisplayName = "AI classes to pass";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_OBJECT_EFFECTS_ITEM;
	child.szDefaultName = "Effects";
	child.szDisplayName = "Effects";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_OBJECT_GRAPHICS_ITEM;
	child.szDefaultName = "Graphics Info";
	child.szDisplayName = "Graphics Info";
	defaultChilds.push_back( child );
}

void CObjectTreeRootItem::ComposeAnimations( const char *pszProjectFileName, const char *pszResultingDir, const CVec2 &zeroPos, const CArray2D<BYTE> &pass, const CVec2 &vOrigin )
{
	CTreeItem *pGraphItem = GetChildItem( E_OBJECT_GRAPHICS_ITEM );
	ASSERT( pGraphItem != 0 );
	string szDir = GetDirectory( pszProjectFileName );
	int i = 0;		//это номер сезона
	for ( CTreeItem::CTreeItemList::const_iterator it=pGraphItem->GetBegin(); it!=pGraphItem->GetEnd(); ++it )
	{
		CObjectGraphicPropsItem *pGraphPropsItem = static_cast<CObjectGraphicPropsItem *> ( it->GetPtr() );
		ASSERT( pGraphPropsItem != 0 );
		
		string szSpriteFullName;
		string szSpriteRelName = pGraphPropsItem->GetFileName();
		bool bRes = MakeFullPath( szDir.c_str(), szSpriteRelName.c_str(), szSpriteFullName );
		if ( !bRes )
			szSpriteFullName = szSpriteRelName;
		
		string szShadowFullName;
		string szShadowRelName = pGraphPropsItem->GetShadowFileName();
		bRes = MakeFullPath( szDir.c_str(), szShadowRelName.c_str(), szShadowFullName );
		if ( !bRes )
			szShadowFullName = szShadowRelName;
		
		string szResultFileName = pszResultingDir;
		szResultFileName += '1';
		if ( i == 1 )
			szResultFileName += 'w';
		if ( i == 2 )
			szResultFileName += 'a';
		ComposeSingleObjectPack( szSpriteFullName.c_str(), szShadowFullName.c_str(), szResultFileName.c_str(), zeroPos, pass, vOrigin );
		i++;
	}
}

FILETIME CObjectTreeRootItem::FindMaximalSourceTime( const char *pszProjectFileName )
{
	FILETIME maxTime, currentTime;
	maxTime.dwHighDateTime = 0;
	maxTime.dwLowDateTime = 0;
	
	CTreeItem *pGraphItem = GetChildItem( E_OBJECT_GRAPHICS_ITEM );
	ASSERT( pGraphItem != 0 );
	string szDir = GetDirectory( pszProjectFileName );
		
	for ( CTreeItemList::const_iterator it = pGraphItem->GetBegin(); it != pGraphItem->GetEnd(); ++it )
	{
		CObjectGraphicPropsItem *pGraphPropsItem = static_cast<CObjectGraphicPropsItem *>( it->GetPtr() );
		ASSERT( pGraphPropsItem != 0 );
		
		string szSpriteFullName;
		string szSpriteRelName = pGraphPropsItem->GetFileName();
		bool bRes = MakeFullPath( szDir.c_str(), szSpriteRelName.c_str(), szSpriteFullName );
		if ( !bRes )
			szSpriteFullName = szSpriteRelName;
		
		string szShadowFullName;
		string szShadowRelName = pGraphPropsItem->GetShadowFileName();
		bRes = MakeFullPath( szDir.c_str(), szShadowRelName.c_str(), szShadowFullName );
		if ( !bRes )
			szShadowFullName = szShadowRelName;
		
		currentTime = GetFileChangeTime( szSpriteFullName.c_str() );
		if ( currentTime > maxTime )
			maxTime = currentTime;
		currentTime = GetFileChangeTime( szShadowFullName.c_str() );
		if ( currentTime > maxTime )
			maxTime = currentTime;
	}

	return maxTime;
}

void CObjectCommonPropsItem::InitDefaultValues()
{
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Health";
	prop.szDisplayName = "Health";
	prop.value = 100;
	defaultValues.push_back( prop );
	
	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Armor";
	prop.szDisplayName = "Armor";
	prop.value = 2;
	defaultValues.push_back( prop );
	
	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Silhouette";
	prop.szDisplayName = "Silhouette";
	prop.value = 1;
	defaultValues.push_back( prop );
	
	prop.nId = 4;
	prop.nDomenType = DT_SOUND_REF;
	prop.szDefaultName = "Ambient sound";
	prop.szDisplayName = "Sound";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_SOUND_REF;
	prop.szDefaultName = "Cycled sound";
	prop.szDisplayName = "Cycled sound";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CObjectPassesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CObjectPassesItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
	case VK_INSERT:
		CTreeItem *pItem = new CObjectPassPropsItem;
		string szName = "AI class to pass";
		pItem->SetItemName( szName.c_str() );
		AddChild( pItem );
		g_frameManager.GetFrame( CFrameManager::E_OBJECT_FRAME )->SetChangedFlag( true );
		break;
	}
}

void CObjectPassPropsItem::InitDefaultValues()
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

void CObjectPassPropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
	case VK_DELETE:
		g_frameManager.GetFrame( CFrameManager::E_OBJECT_FRAME )->ClearPropView();
		DeleteMeInParentTreeItem();
		g_frameManager.GetFrame( CFrameManager::E_OBJECT_FRAME )->SetChangedFlag( true );
		break;
	}
}

void CObjectPassPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 )
	{
		std::string szName = value;
		ChangeItemName( szName.c_str() );
	}
}

int CObjectPassPropsItem::GetPassAIClass()
{
	std::string szVal = values[0].value;
	return GetAIClassInfo( szVal.c_str() );
}

/*
void CObjectPassPropsItem::SetPassAIClass( int nVal )
{
	values[0].value = GetAIClassInfo( nVal );
}
*/

void CObjectGraphicsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
	
	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = E_OBJECT_GRAPHIC1_PROPS_ITEM;
	child.szDefaultName = "Summer picture";
	child.szDisplayName = "Summer picture";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_OBJECT_GRAPHICW1_PROPS_ITEM;
	child.szDefaultName = "Winter picture";
	child.szDisplayName = "Winter picture";
	defaultChilds.push_back( child );

	child.nChildItemType = E_OBJECT_GRAPHICA1_PROPS_ITEM;
	child.szDefaultName = "Africa picture";
	child.szDisplayName = "Africa picture";
	defaultChilds.push_back( child );
}

void CObjectGraphicPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	if ( nItemId == 1 || nItemId == 2 )
	{
		CObjectFrame *pFrame = static_cast<CObjectFrame *> ( g_frameManager.GetFrame( CFrameManager::E_OBJECT_FRAME ) );
		if ( !IsRelatedPath( value ) )
		{
			string szProjectName = g_frameManager.GetFrame( CFrameManager::E_OBJECT_FRAME )->GetProjectFileName();
			string szValue = value;
			string szRelatedPath;
			bool bRes = MakeRelativePath( szProjectName.c_str(), szValue.c_str(), szRelatedPath );
			if ( bRes )
			{
				CVariant newVal = szRelatedPath;
				CTreeItem::UpdateItemValue( nItemId, newVal );
				pFrame->UpdatePropView( this );
				pFrame->UpdateActiveSprite();
			}
			else
			{
				AfxMessageBox( "Note, this project will not be portable on other computers,\nproject file name and sprite file name should be on the same drive" );
				pFrame->UpdateActiveSprite();
			}
		}
	}
}

void CObjectGraphicPropsItem::MyLButtonClick()
{
	CObjectFrame *pFrame = static_cast<CObjectFrame *> ( g_frameManager.GetFrame( CFrameManager::E_OBJECT_FRAME ) );
	pFrame->SetActiveGraphicPropsItem( this );
}

void CObjectGraphic1PropsItem::InitDefaultValues()
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

void CObjectGraphicW1PropsItem::InitDefaultValues()
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

void CObjectGraphicA1PropsItem::InitDefaultValues()
{
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sprite";
	prop.szDisplayName = "Sprite";
	prop.value = "1a.tga";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();
	
	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Shadow";
	prop.szDisplayName = "Shadow";
	prop.value = "1as.tga";
	prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();
	
	values = defaultValues;
}

void CObjectEffectsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Death with explosion";
	prop.szDisplayName = "Death with explosion";
	prop.value = "";
	defaultValues.push_back( prop );
	
	prop.nId = 2;
	prop.nDomenType = DT_EFFECT_REF;
	prop.szDefaultName = "Silent death";
	prop.szDisplayName = "Silent death";
	prop.value = "";
	defaultValues.push_back( prop );
	
	values = defaultValues;
}
