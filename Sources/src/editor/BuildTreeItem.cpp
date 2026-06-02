#include "StdAfx.h"
#include <io.h>

#include "..\image\image.h"
#include "..\main\rpgstats.h"
#include "editor.h"
#include "frames.h"
#include "BuildFrm.h"
#include "BuildTreeItem.h"
#include "BuildCompose.h"
#include "Reference.h"
#include "SpriteCompose.h"

void CBuildingTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
	
	defaultChilds.clear();
	SChildItem child;
	
	child.nChildItemType = E_BUILDING_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BUILDING_PASSES_ITEM;
	child.szDefaultName = "AI classes to pass";
	child.szDisplayName = "AI classes to pass";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BUILDING_DEFENCES_ITEM;
	child.szDefaultName = "Defence";
	child.szDisplayName = "Defence";
	defaultChilds.push_back( child );
	
	/*child.nChildItemType = E_BUILDING_ENTRANCES_ITEM;
	child.szDefaultName = "Entrances";
	child.szDisplayName = "Entrances";
	defaultChilds.push_back( child );*/
	
	child.nChildItemType = E_BUILDING_SLOTS_ITEM;
	child.szDefaultName = "Slots";
	child.szDisplayName = "Shoot slots";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BUILDING_FIRE_POINTS_ITEM;
	child.szDefaultName = "Fire points";
	child.szDisplayName = "Fire points";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BUILDING_DIR_EXPLOSIONS_ITEM;
	child.szDefaultName = "Direction explosions";
	child.szDisplayName = "Direction explosions";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BUILDING_SMOKES_ITEM;
	child.szDefaultName = "Smoke points";
	child.szDisplayName = "Smoke points";
	defaultChilds.push_back( child );

	child.nChildItemType = E_BUILDING_GRAPHICS_ITEM;
	child.szDefaultName = "Graphics Info";
	child.szDisplayName = "Graphics Info";
	defaultChilds.push_back( child );
}

void CBuildingTreeRootItem::ComposeAnimations( const char *pszProjectFileName, const char *pszResultingDir, const CVec2 &zeroPos, const CArray2D<BYTE> &pass, const CVec2 &vOrigin )
{
	string szDir = GetDirectory( pszProjectFileName );
	IImageProcessor *pIP = GetSingleton<IImageProcessor>();
	CTreeItem *pGraphItem = GetChildItem( E_BUILDING_GRAPHICS_ITEM );
	ASSERT( pGraphItem != 0 );
	int k = 0;		//это номер сезона
	for ( CTreeItem::CTreeItemList::const_iterator ext=pGraphItem->GetBegin(); ext!=pGraphItem->GetEnd(); ++ext )
	{
		CTreeItem *pSeasonProps = (*ext);
		int i = 0;								//i это номер спрайта в текущем сезоне
		for ( CTreeItem::CTreeItemList::const_iterator it=pSeasonProps->GetBegin(); it!=pSeasonProps->GetEnd(); ++it )
		{
			CBuildingGraphicPropsItem *pGraphPropsItem = static_cast<CBuildingGraphicPropsItem *> ( it->GetPtr() );
			ASSERT( pGraphPropsItem != 0 );
			
			string szSpriteFullName;
			string szSpriteRelName = pGraphPropsItem->GetFileName();
			if ( IsRelatedPath( szSpriteRelName.c_str() ) )
			{
				bool bRes = MakeFullPath( szDir.c_str(), szSpriteRelName.c_str(), szSpriteFullName );
				if ( !bRes )
				{
					string str = "Can not convert path to full path name  ";
					str += szSpriteRelName;
					AfxMessageBox( str.c_str() );
					return;
				}
			}
			else
				szSpriteFullName = szSpriteRelName;
			
			string szShadowFullName;
			string szShadowRelName = pGraphPropsItem->GetShadowFileName();
			if ( IsRelatedPath( szShadowRelName.c_str() ) )
			{
				bool bRes = MakeFullPath( szDir.c_str(), szShadowRelName.c_str(), szShadowFullName );
				if ( !bRes )
				{
					string str = "Can not convert path to full path name  ";
					str += szShadowRelName;
					AfxMessageBox( str.c_str() );
					return;
				}
			}
			else
				szShadowFullName = szShadowRelName;
			
			string szResultFileName = pszResultingDir;
			szResultFileName += NStr::Format( "%d", i+1);
			if ( k == 1 )
				szResultFileName += 'w';
			
			ComposeSingleObjectPack( szSpriteFullName.c_str(), szShadowFullName.c_str(), szResultFileName.c_str(), zeroPos, pass, vOrigin );

			if ( i == 1 || i == 2 )
			{
				string szNoiseFullName;
				bool bRes = MakeFullPath( szDir.c_str(), pGraphPropsItem->GetNoiseFileName(), szNoiseFullName );
				if ( !bRes )
					szNoiseFullName = pGraphPropsItem->GetNoiseFileName();

				CPtr<IDataStream> pBuildStream = OpenFileStream( szSpriteFullName.c_str(), STREAM_ACCESS_READ );
				if ( pBuildStream == 0 )
					goto label1;
				CPtr<IImage> pSpriteImage = pIP->LoadImage( pBuildStream );
				if ( pSpriteImage == 0 )
					goto label1;
				CPtr<IImage> pInverseSprite = pSpriteImage->Duplicate();
				pInverseSprite->SharpenAlpha( 128 );
				pInverseSprite->InvertAlpha();
				
				CPtr<IDataStream> pNoiseStream = OpenFileStream( szNoiseFullName.c_str(), STREAM_ACCESS_READ );
				if ( pNoiseStream == 0 )
					goto label1;
				CPtr<IImage> pNoiseImage = pIP->LoadImage( pNoiseStream );
				if ( pNoiseImage == 0 )
					goto label1;
				if ( pInverseSprite->GetSizeX() != pNoiseImage->GetSizeX() || pInverseSprite->GetSizeY() != pNoiseImage->GetSizeY() )
				{
					string szErr = "Error: The size of building image is not equal to the size of noise file:\n";
					szErr += szSpriteFullName;
					szErr += ",  ";
					szErr += szNoiseFullName;
					
					AfxMessageBox( szErr.c_str() );
					goto label1;
				}
				RECT rc;
				rc.left = 0;
				rc.top = 0;
				rc.right = pInverseSprite->GetSizeX();
				rc.bottom = pInverseSprite->GetSizeY();
				pNoiseImage->ModulateAlphaFrom( pInverseSprite, &rc, 0, 0 );

				string szResNoiseFile = pszResultingDir;
				szResNoiseFile += NStr::Format( "%d", i+1);
				if ( k == 1 )
					szResNoiseFile += 'w';
				szResNoiseFile += 'g';
				
				CSpritesPackBuilder::SPackParameter param;
				param.pImage = pNoiseImage;
				param.center = CTPoint<int>( zeroPos.x, zeroPos.y );
				param.lockedTiles.Clear();
				param.lockedTilesCenter = CTPoint<int>( 0, 0 );
				if ( BuildSpritesPack( param, szResNoiseFile.c_str() ) )
				{
					std::string szTGAName = szResNoiseFile + ".tga";
					CPtr<IDataStream> pStream = OpenFileStream( szTGAName.c_str(), STREAM_ACCESS_READ );
					NI_ASSERT( pStream != 0 );
					pNoiseImage = pIP->LoadImage( pStream );
					pStream = 0;
					
					SaveCompressedTexture( pNoiseImage, szResNoiseFile.c_str() );
					remove( szTGAName.c_str() );
				}
			}
			
label1:
			i++;
		}
		k++;
	}
}

FILETIME CBuildingTreeRootItem::FindMaximalSourceTime( const char *pszProjectFileName )
{
	FILETIME maxTime, currentTime;
	maxTime.dwHighDateTime = 0;
	maxTime.dwLowDateTime = 0;
	
	CTreeItem *pGraphItem = GetChildItem( E_BUILDING_GRAPHICS_ITEM );
	ASSERT( pGraphItem != 0 );
	for ( CTreeItem::CTreeItemList::const_iterator ext=pGraphItem->GetBegin(); ext!=pGraphItem->GetEnd(); ++ext )
	{
		CTreeItem *pSeasonProps = (*ext);
		string szDir = GetDirectory( pszProjectFileName );
		for ( CTreeItemList::const_iterator it=pSeasonProps->GetBegin(); it!=pSeasonProps->GetEnd(); ++it )
		{
			CBuildingGraphicPropsItem *pGraphPropsItem = static_cast<CBuildingGraphicPropsItem *> ( it->GetPtr() );
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
	}

	return maxTime;
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

int CBuildingCommonPropsItem::GetBuildingType()
{
	string szVal = values[1].value;
	if ( szVal == "building" || szVal == "Building" )
		return SBuildingRPGStats::TYPE_BULDING;
	if ( szVal == "main storage" || szVal == "Main storage" )
		return SBuildingRPGStats::TYPE_MAIN_RU_STORAGE;
	if ( szVal == "temporary storage" || szVal == "Temporary storage")
		return SBuildingRPGStats::TYPE_TEMP_RU_STORAGE;
	if ( szVal == "dot" || szVal == "DOT" )
		return SBuildingRPGStats::TYPE_DOT;
	NI_ASSERT( 0 );		//unknown building type
	return 0;
}

void CBuildingCommonPropsItem::SetBuildingType( int nVal )
{
	string szVal;
	switch ( nVal )
	{
		case SBuildingRPGStats::TYPE_BULDING:
			szVal = "building";
			break;
		case SBuildingRPGStats::TYPE_MAIN_RU_STORAGE:
			szVal = "main storage";
			break;
		case SBuildingRPGStats::TYPE_TEMP_RU_STORAGE:
			szVal = "temporary storage";
			break;
		case SBuildingRPGStats::TYPE_DOT:
			szVal = "dot";
			break;
		default:
			NI_ASSERT( 0 );		//unknown building type
	}
	values[1].value = szVal;
}


void CBuildingPassesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CBuildingPassesItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
	case VK_INSERT:
		CTreeItem *pItem = new CBuildingPassPropsItem;
		string szName = "AI class to pass";
		pItem->SetItemName( szName.c_str() );
		AddChild( pItem );
		g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME )->SetChangedFlag( true );
		break;
	}
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

void CBuildingPassPropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
	case VK_DELETE:
		g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME )->ClearPropView();
		DeleteMeInParentTreeItem();
		g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME )->SetChangedFlag( true );
		break;
	}
}

void CBuildingPassPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 )
	{
		std::string szName = value;
		ChangeItemName( szName.c_str() );
	}
}

int CBuildingPassPropsItem::GetPassAIClass()
{
	std::string szVal = values[0].value;
	return GetAIClassInfo( szVal.c_str() );
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

void CBuildingSlotsItem::MyLButtonClick()
{
	CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
	pFrame->SetActiveMode( CBuildingFrame::E_SHOOT_SLOT );
	pFrame->GFXDraw();
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

void CBuildingSlotPropsItem::MyLButtonClick()
{
	CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
	pFrame->SelectShootPoint( this );
	pFrame->SetActiveMode( CBuildingFrame::E_SHOOT_SLOT );
	pFrame->GFXDraw();
}

void CBuildingSlotPropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
			pFrame->DeleteShootPoint( this );
			DeleteMeInParentTreeItem();
			break;
	}
}

void CBuildingSlotPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
	if ( nItemId == 1 )
	{
		CTreeItem *pTemp = pFrame->GetActiveShootPointItem();
		if ( pTemp != this )
		{
			pFrame->SelectShootPoint( this );
			pFrame->SetConeDirection( value );
			pFrame->SelectShootPoint( pTemp );
		}
		else
			pFrame->SetConeDirection( value );
		return;
	}

	if ( nItemId == 2 )
	{
		CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
		CTreeItem *pTemp = pFrame->GetActiveShootPointItem();
		if ( pTemp != this )
		{
			pFrame->SelectShootPoint( this );
			pFrame->SetConeAngle( value );
			pFrame->SelectShootPoint( pTemp );
		}
		else
			pFrame->SetConeAngle( value );
		return;
	}
}

void CBuildingGraphicsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;
	
	child.nChildItemType = E_BUILDING_SUMMER_PROPS_ITEM;
	child.szDefaultName = "Summer";
	child.szDisplayName = "Summer";
	defaultChilds.push_back( child );

	child.nChildItemType = E_BUILDING_WINTER_PROPS_ITEM;
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
	
	child.nChildItemType = E_BUILDING_GRAPHIC1_PROPS_ITEM;
	child.szDefaultName = "Whole";
	child.szDisplayName = "Whole";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BUILDING_GRAPHIC2_PROPS_ITEM;
	child.szDefaultName = "Damaged";
	child.szDisplayName = "Damaged";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BUILDING_GRAPHIC3_PROPS_ITEM;
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
	
	child.nChildItemType = E_BUILDING_GRAPHICW1_PROPS_ITEM;
	child.szDefaultName = "Whole";
	child.szDisplayName = "Whole";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BUILDING_GRAPHICW2_PROPS_ITEM;
	child.szDefaultName = "Damaged";
	child.szDisplayName = "Damaged";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BUILDING_GRAPHICW3_PROPS_ITEM;
	child.szDefaultName = "Destroyed";
	child.szDisplayName = "Destroyed";
	defaultChilds.push_back( child );
}

void CBuildingGraphicPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	if ( nItemId == 1 || nItemId == 2 || nItemId == 3 )
	{
		CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
		if ( !IsRelatedPath( value ) )
		{
			string szProjectName = g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME )->GetProjectFileName();
			string szValue = value;
			string szRelatedPath;
			bool bRes = MakeRelativePath( szProjectName.c_str(), szValue.c_str(), szRelatedPath );
			if ( bRes )
			{
				CVariant newVal = szRelatedPath;
				CTreeItem::UpdateItemValue( nItemId, newVal );
				pFrame->UpdatePropView( this );
				if ( nItemId == 1 || nItemId == 2 )
					pFrame->UpdateActiveSprite();
			}
			else
			{
				if ( nItemId == 1 || nItemId == 2 )
				{
					AfxMessageBox( "Note, this project will not be portable on other computers,\nproject file name and sprite file name should be on the same drive" );
					pFrame->UpdateActiveSprite();
				}
				else if ( nItemId == 3 )
					AfxMessageBox( "Note, this project will not be portable on other computers,\nproject file name and noise file name should be on the same drive" );
			}
		}
	}
}

void CBuildingGraphicPropsItem::MyLButtonClick()
{
	CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
	pFrame->SetActiveGraphicPropsItem( this );
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
			
	child.nChildItemType = E_BUILDING_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Front";
	child.szDisplayName = "Front";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BUILDING_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Left";
	child.szDisplayName = "Left";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BUILDING_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Right";
	child.szDisplayName = "Right";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BUILDING_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Back";
	child.szDisplayName = "Back";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BUILDING_DEFENCE_PROPS_ITEM;
	child.szDefaultName = "Top";
	child.szDisplayName = "Top";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_BUILDING_DEFENCE_PROPS_ITEM;
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

void CBuildingFirePointsItem::MyLButtonClick()
{
	CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
	pFrame->SetActiveMode( CBuildingFrame::E_FIRE_POINT );
	pFrame->GFXDraw();
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

void CBuildingFirePointPropsItem::MyLButtonClick()
{
	CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
	pFrame->SelectFirePoint( this );
	pFrame->SetActiveMode( CBuildingFrame::E_FIRE_POINT );
	pFrame->GFXDraw();
}

void CBuildingFirePointPropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
			pFrame->DeleteFirePoint( this );
			DeleteMeInParentTreeItem();
			break;
	}
}

void CBuildingFirePointPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	if ( nItemId == 1 )
	{
		CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
		CTreeItem *pTemp = pFrame->GetActiveFirePointItem();
		if ( pTemp != this )
		{
			pFrame->SelectFirePoint( this );
			pFrame->SetFireDirection( value );
			pFrame->SelectFirePoint( pTemp );
		}
		else
			pFrame->SetFireDirection( value );
		return;
	}
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

	child.nChildItemType = E_BUILDING_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Front left";
	child.szDisplayName = "Front left";
	defaultChilds.push_back( child );

	child.nChildItemType = E_BUILDING_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Front right";
	child.szDisplayName = "Front right";
	defaultChilds.push_back( child );

	child.nChildItemType = E_BUILDING_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Back right";
	child.szDisplayName = "Back right";
	defaultChilds.push_back( child );

	child.nChildItemType = E_BUILDING_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Back left";
	child.szDisplayName = "Back left";
	defaultChilds.push_back( child );

	child.nChildItemType = E_BUILDING_DIR_EXPLOSION_PROPS_ITEM;
	child.szDefaultName = "Top center";
	child.szDisplayName = "Top center";
	defaultChilds.push_back( child );
}

void CBuildingDirExplosionsItem::MyLButtonClick()
{
	CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
	pFrame->SetActiveMode( CBuildingFrame::E_DIR_EXPLOSION );
	pFrame->GFXDraw();
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

void CBuildingDirExplosionPropsItem::MyLButtonClick()
{
	CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
	pFrame->SelectDirExpPoint( this );
	pFrame->SetActiveMode( CBuildingFrame::E_DIR_EXPLOSION );
	pFrame->GFXDraw();
}

void CBuildingDirExplosionPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	if ( nItemId == 1 )
	{
		CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
		pFrame->ComputeDirExpDirectionLines();
		return;
	}
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

void CBuildingSmokesItem::MyLButtonClick()
{
	CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
	pFrame->SetActiveMode( CBuildingFrame::E_SMOKE_POINT );
	pFrame->GFXDraw();
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

void CBuildingSmokePropsItem::MyLButtonClick()
{
	CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
	pFrame->SelectSmokePoint( this );
	pFrame->SetActiveMode( CBuildingFrame::E_SMOKE_POINT );
	pFrame->GFXDraw();
}

void CBuildingSmokePropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	if ( nItemId == 1 )
	{
		CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
		pFrame->ComputeSmokeLines();
		return;
	}
}

void CBuildingSmokePropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
	case VK_DELETE:
		CBuildingFrame *pFrame = static_cast<CBuildingFrame *> ( g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME ) );
		pFrame->DeleteSmokePoint();
		DeleteMeInParentTreeItem();
		break;
	}
}
