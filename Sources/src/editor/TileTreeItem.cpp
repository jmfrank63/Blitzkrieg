#include "StdAfx.h"
#include <io.h>

#include "..\GFX\gfx.h"
#include "..\Image\image.h"
#include "..\main\rpgstats.h"

#include "editor.h"
#include "frames.h"
#include "TileSetFrm.h"
#include "TileTreeItem.h"
#include "common.h"
#include "SpriteCompose.h"

void CTileSetTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
	
	defaultChilds.clear();

	SChildItem child;
	
	child.nChildItemType = E_TILESET_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );

	child.nChildItemType = E_TILESET_TERRAINS_ITEM;
	child.szDefaultName = "Terrains";
	child.szDisplayName = "Terrains";
	defaultChilds.push_back( child );

	child.nChildItemType = E_CROSSETS_ITEM;
	child.szDefaultName = "Crossets";
	child.szDisplayName = "Crossets";
	defaultChilds.push_back( child );
}

void CTileSetTreeRootItem::ComposeTiles( const char *pszProjectFileName, const char *pszResFileName )
{
	CTileSetTerrainsItem *pTerrainsItem = (CTileSetTerrainsItem *) GetChildItem( E_TILESET_TERRAINS_ITEM );
	string szFullDirName;
	{
		string szRelDirName = pTerrainsItem->GetTerrainsDirName();
		if ( IsRelatedPath(szRelDirName.c_str()) )
		{
			string szProjectDir;
			szProjectDir = GetDirectory( pszProjectFileName );
			MakeFullPath( szProjectDir.c_str(), szRelDirName.c_str(), szFullDirName );
		}
		else
			szFullDirName = szRelDirName;
	}
	CPtr<IDataStorage> pStorage = CreateStorage( szFullDirName.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );

	CTreeItemList::const_iterator it;
	int nMaxIndex = -1;
	for ( it=pTerrainsItem->GetBegin(); it!=pTerrainsItem->GetEnd(); ++it )
	{
		CTreeItem *pTiles = (*it)->GetChildItem( E_TILESET_TILES_ITEM );
		for ( CTreeItemList::const_iterator in=pTiles->GetBegin(); in!=pTiles->GetEnd(); ++in )
		{
			CTileSetTilePropsItem *pProps = static_cast<CTileSetTilePropsItem *> ( in->GetPtr() );
			if ( nMaxIndex < pProps->nTileIndex )
				nMaxIndex = pProps->nTileIndex;
		}
	}
	int nNumberOfRaws = ( nMaxIndex ) / 7;
	int nMinSizeY = nNumberOfRaws * 32 + 16;
	nMinSizeY = GetNextPow2( nMinSizeY );
	
	IImageProcessor *pImageProcessor = GetImageProcessor();
	CPtr<IImage> pTileSetImage = pImageProcessor->CreateImage( 64*4, nMinSizeY );
	pTileSetImage->Set( 0 );
	CPtr<IImage> pMaskImage;
	{
		string szMaskName = theApp.GetEditorDataDir();
		szMaskName += "editor\\terrain\\tilemask.tga";
		CPtr<IDataStream> pMaskStream = OpenFileStream( szMaskName.c_str(), STREAM_ACCESS_READ );
		if ( !pMaskStream )
		{
			string szErrorMsg = "Error: Cannot open terrain mask file: ";
			szErrorMsg += szMaskName;
			AfxMessageBox( szErrorMsg.c_str() );
			return;
		}
		pMaskImage = pImageProcessor->LoadImage( pMaskStream );
	}

	vector<string> vectorOfInvalidFiles;
	STilesetDesc tileSetDesc;
	CTileSetCommonPropsItem *pCommonProps = static_cast<CTileSetCommonPropsItem *> ( GetChildItem( E_TILESET_COMMON_PROPS_ITEM ) );
	tileSetDesc.szName = pCommonProps->GetTileSetName();
	for ( it=pTerrainsItem->GetBegin(); it!=pTerrainsItem->GetEnd(); ++it )
	{
		CTileSetTerrainPropsItem *pTerrainProps = (CTileSetTerrainPropsItem *) it->GetPtr();
		CTreeItemList::const_iterator in;		//internal iterator
		STerrTypeDesc terrType;
		terrType.szName = pTerrainProps->GetTerrainName();
		terrType.nCrosset = pTerrainProps->GetCrossetNumber();
		terrType.nPriority = pTerrainProps->GetMaskPriority();
		terrType.fPassability = pTerrainProps->GetPassability();
		terrType.bMicroTexture = pTerrainProps->GetMicrotextureFlag();
		terrType.fSoundVolume = pTerrainProps->GetSoundVolume();
		terrType.szSound = pTerrainProps->GetSoundRef();
		terrType.szLoopedSound = pTerrainProps->GetLoopedSoundRef();
		terrType.bCanEntrench = pTerrainProps->GetBuildFlag();
		if ( pTerrainProps->GetTraceFlag() )
			terrType.cSoilParams |= STerrTypeDesc::ESP_TRACE;
		if ( pTerrainProps->GetDustFlag() )
			terrType.cSoilParams |= STerrTypeDesc::ESP_DUST;
		
		terrType.dwAIClasses = 0;
		if ( pTerrainProps->GetPassForInfantry() )
			terrType.dwAIClasses |= AI_CLASS_HUMAN;
		if ( pTerrainProps->GetPassForWheels() )
			terrType.dwAIClasses |= AI_CLASS_WHEEL;
		if ( pTerrainProps->GetPassForHalfTracks() )
			terrType.dwAIClasses |= AI_CLASS_HALFTRACK;
		if ( pTerrainProps->GetPassForTracks() )
			terrType.dwAIClasses |= AI_CLASS_TRACK;
		terrType.dwAIClasses = ~terrType.dwAIClasses;
		if ( pTerrainProps->GetWaterFlag() )
			terrType.dwAIClasses |= 0x80000000;
		else
			terrType.dwAIClasses &= 0x7fffffff;

/*
		CTreeItem *pASounds = pTerrainProps->GetChildItem( E_TILESET_ASOUNDS_ITEM );
		NI_ASSERT( pASounds != 0 );
		for ( CTreeItemList::const_iterator ss=pASounds->GetBegin(); ss!=pASounds->GetEnd(); ++ss )
		{
			CTileSetASoundPropsItem *pSound = static_cast<CTileSetASoundPropsItem *> ( ss->GetPtr() );
			STerrTypeDesc::STerrainSound snd;
			snd.szName = pSound->GetSoundName();
			snd.bPeaceful = pSound->GetPeaceFlag();
			snd.fProbability = pSound->GetProbability();
			terrType.sounds.push_back( snd );
		}

		CTreeItem *pLSounds = pTerrainProps->GetChildItem( E_TILESET_LSOUNDS_ITEM );
		NI_ASSERT( pLSounds != 0 );
		for ( CTreeItemList::const_iterator ss=pLSounds->GetBegin(); ss!=pLSounds->GetEnd(); ++ss )
		{
			CTileSetLSoundPropsItem *pSound = static_cast<CTileSetLSoundPropsItem *> ( ss->GetPtr() );
			STerrTypeDesc::STerrainLoopedSound snd;
			snd.szName = pSound->GetSoundName();
			snd.bPeaceful = pSound->GetPeaceFlag();
			terrType.loopedSounds.push_back( snd );
		}
*/

		CTreeItem *pTiles = pTerrainProps->GetChildItem( E_TILESET_TILES_ITEM );
		NI_ASSERT( pTiles != 0 );
		for ( in=pTiles->GetBegin(); in!=pTiles->GetEnd(); ++in )
		{
			CTileSetTilePropsItem *pTileProps = (CTileSetTilePropsItem *) in->GetPtr();

			string szName = pTileProps->GetItemName();
			szName += ".tga";
			CPtr<IDataStream> pTGAStream = pStorage->CreateStream( szName.c_str(), STREAM_ACCESS_READ );
			CPtr<IImage> pCurrentTileImage;
			if ( !pTGAStream )
			{
				string szFullName = pTerrainsItem->GetTerrainsDirName();
				szFullName += pTileProps->GetItemName();
				szFullName += ".tga";
				vectorOfInvalidFiles.push_back( szFullName );
				continue;
			}
			else
			{
				pCurrentTileImage = pImageProcessor->LoadImage( pTGAStream );
				if ( !pCurrentTileImage )
				{
					string szFullName = pTerrainsItem->GetTerrainsDirName();
					szFullName += pTileProps->GetItemName();
					szFullName += ".tga";
					vectorOfInvalidFiles.push_back( szFullName );
					continue;
				}
			}

			RECT rc;
			rc.left = 0;
			rc.top = 0;
			rc.right = pMaskImage->GetSizeX();
			rc.bottom = pMaskImage->GetSizeY();
			pCurrentTileImage->ModulateColorFrom( pMaskImage, &rc, 0, 0 );

			int nMod7 = pTileProps->nTileIndex % 7;
			int nPosX, nPosY;
			if ( nMod7 < 4 )
			{
				nPosX = nMod7 * 64;
				nPosY = (pTileProps->nTileIndex / 7) * 32;
			}
			else
			{
				nPosX = (nMod7 - 4) * 64 + 32;
				nPosY = (pTileProps->nTileIndex / 7) * 32 + 16;
			}
			pTileSetImage->CopyFromAB( pCurrentTileImage, &rc, nPosX, nPosY );

			SMainTileDesc mainTileDesc;
			mainTileDesc.fProbFrom = 0;
			mainTileDesc.fProbTo = pTileProps->GetProbability();
			switch ( pTileProps->GetFlippedState() )
			{
			case CTileSetTilePropsItem::E_NORMAL:
				mainTileDesc.nIndex = pTileProps->nTileIndex * 2;
				terrType.tiles.push_back( mainTileDesc );
				break;
			case CTileSetTilePropsItem::E_FLIPPED:
				mainTileDesc.nIndex = pTileProps->nTileIndex * 2 + 1;
				terrType.tiles.push_back( mainTileDesc );
				break;
			case CTileSetTilePropsItem::E_BOTH:
				mainTileDesc.nIndex = pTileProps->nTileIndex * 2;
				terrType.tiles.push_back( mainTileDesc );
				mainTileDesc.nIndex = pTileProps->nTileIndex * 2 + 1;
				terrType.tiles.push_back( mainTileDesc );
				break;
			}
		}
		tileSetDesc.terrtypes.push_back( terrType );
	}
	
	if ( vectorOfInvalidFiles.size() > 0 )
	{
		std::string szErr = "Error: Some of files can not be opened\n";
		for ( int i=0; i<vectorOfInvalidFiles.size(); i++ )
		{
			szErr += vectorOfInvalidFiles[i];
			szErr += "\n";
		}
		AfxMessageBox( szErr.c_str() );
	}
	
	std::string szTemp = pszResFileName;
	szTemp = szTemp.substr( 0, szTemp.rfind( '.' ) );
	FillTileMaps( 256, nMinSizeY, tileSetDesc.tilemaps, true );
	SaveCompressedTexture( pTileSetImage, szTemp.c_str() );
	
	string szName = pszResFileName;
	int nPos = szName.rfind( '\\' );
	if ( nPos != std::string::npos )
	szName = szName.substr( nPos + 1 );
	CPtr<IDataStorage> pSaveStorage = CreateStorage( GetDirectory(pszResFileName).c_str(), STREAM_ACCESS_WRITE, STORAGE_TYPE_FILE );
	CPtr<IDataStream> pSaveXMLStream = pSaveStorage->CreateStream( szName.c_str(), STREAM_ACCESS_WRITE );
	CPtr<IDataTree> pDT = CreateDataTreeSaver( pSaveXMLStream, IDataTree::WRITE );
	CTreeAccessor tree = pDT;
	tree.Add( "tileset", &tileSetDesc );


	CCrossetsItem *pCrossetsItem = (CCrossetsItem *) GetChildItem( E_CROSSETS_ITEM );
	{
		string szRelDirName = pCrossetsItem->GetCrossetsDirName();
		if ( IsRelatedPath(szRelDirName.c_str()) )
		{
			string szProjectDir;
			szProjectDir = GetDirectory( pszProjectFileName );
			MakeFullPath( szProjectDir.c_str(), szRelDirName.c_str(), szFullDirName );
		}
		else
			szFullDirName = szRelDirName;
	}
	pStorage = OpenStorage( szFullDirName.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
	
	int nCrossCount = 0;
	for ( it=pCrossetsItem->GetBegin(); it!=pCrossetsItem->GetEnd(); ++it )
		nCrossCount += (*it)->GetChildsCount();
	if ( nCrossCount == 0 )
		return;

	nNumberOfRaws = ( nCrossCount + 6 ) / 7;
	nMinSizeY = nNumberOfRaws * 32 + 16;
	nMinSizeY = GetNextPow2( nMinSizeY );

	CPtr<IImage> pCrossSetImage = pImageProcessor->CreateImage( 64*4, nMinSizeY );
	pCrossSetImage->Set( 0 );

	vectorOfInvalidFiles.clear();
	SCrossetDesc crossSetDesc;
	for ( it=pCrossetsItem->GetBegin(); it!=pCrossetsItem->GetEnd(); ++it )
	{
		CCrossetPropsItem *pCrossetProps = (CCrossetPropsItem *) it->GetPtr();
		CTreeItemList::const_iterator in;		//internal iterator
		SCrossDesc crossDesc;
		crossDesc.szName = pCrossetProps->GetCrossetName();
		
		for ( in=pCrossetProps->GetBegin(); in!=pCrossetProps->GetEnd(); ++in )
		{
			CCrossetTilesItem *pCrossTilesItem = (CCrossetTilesItem *) in->GetPtr();
			SCrossTileTypeDesc crossTileDesc;
			crossTileDesc.szName = pCrossTilesItem->GetItemName();

			CTreeItemList::const_iterator z;
			for ( z=pCrossTilesItem->GetBegin(); z!=pCrossTilesItem->GetEnd(); ++z )
			{
				CCrossetTilePropsItem *pTileProps = (CCrossetTilePropsItem *) z->GetPtr();
				string szName = pTileProps->GetItemName();

				szName += ".tga";
				CPtr<IDataStream> pTGAStream = pStorage->OpenStream( szName.c_str(), STREAM_ACCESS_READ );
				CPtr<IImage> pCurrentTileImage;
				if ( !pTGAStream )
				{
					string szFullName = pCrossetsItem->GetCrossetsDirName();
					szFullName += pTileProps->GetItemName();
					szFullName += ".tga";
					vectorOfInvalidFiles.push_back( szFullName );
					break;
				}
				else
				{
					pCurrentTileImage = pImageProcessor->LoadImage( pTGAStream );
					if ( !pCurrentTileImage )
					{
						string szFullName = pCrossetsItem->GetCrossetsDirName();
						szFullName += pTileProps->GetItemName();
						szFullName += ".tga";
						vectorOfInvalidFiles.push_back( szFullName );
						break;
					}
				}
				
				RECT rc;
				rc.left = 0;
				rc.top = 0;
				rc.right = pMaskImage->GetSizeX();
				rc.bottom = pMaskImage->GetSizeY();
				pCurrentTileImage->ModulateColorFrom( pMaskImage, &rc, 0, 0 );
				
				int nMod7 = (pTileProps->nCrossIndex / 2) % 7;
				int nPosX, nPosY;
				if ( nMod7 < 4 )
				{
					nPosX = nMod7 * 64;
					nPosY = (pTileProps->nCrossIndex / 14) * 32;
				}
				else
				{
					nPosX = (nMod7 - 4) * 64 + 32;
					nPosY = (pTileProps->nCrossIndex / 14) * 32 + 16;
				}
				pCrossSetImage->CopyFromAB( pCurrentTileImage, &rc, nPosX, nPosY );
				
				SMainTileDesc mainTileDesc;
				mainTileDesc.fProbFrom = 0;
				mainTileDesc.fProbTo = pTileProps->GetProbability();
				mainTileDesc.nIndex = pTileProps->nCrossIndex;
				crossTileDesc.tiles.push_back( mainTileDesc );
			}

			crossDesc.tiles.push_back( crossTileDesc );
		}
		crossSetDesc.crosses.push_back( crossDesc );
	}

	if ( vectorOfInvalidFiles.size() > 0 )
	{
		std::string szErr = "Error: Some of files can not be opened\n";
		for ( int i=0; i<vectorOfInvalidFiles.size(); i++ )
		{
			szErr += vectorOfInvalidFiles[i];
			szErr += "\n";
		}
		AfxMessageBox( szErr.c_str() );
	}

	{
		SColor *p = pCrossSetImage->GetLFB();
		int nX = pCrossSetImage->GetSizeX();
		int nY = pCrossSetImage->GetSizeY();
		for ( int y=0; y<nY; y++ )
		{
			for ( int x=0; x<nX; x++ )
			{
				p[y*256+x].r = p[y*256+x].g = p[y*256+x].b = 255;
			}
		}
	}
	
	
	FillTileMaps( 256, nMinSizeY, crossSetDesc.tilemaps, true );
	szTemp = GetDirectory(pszResFileName);
	szTemp += "crosset";

	CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
	int nC = pFrame->m_nCompressedFormat;
	int nL = pFrame->m_nLowFormat;
	pFrame->m_nCompressedFormat = GFXPF_DXT5;
	pFrame->m_nLowFormat = GFXPF_ARGB4444;
	SaveCompressedTexture( pCrossSetImage, szTemp.c_str() );
	pFrame->m_nCompressedFormat = nC;
	pFrame->m_nLowFormat = nL;

	szName = "crosset.xml";
	pSaveXMLStream = pSaveStorage->CreateStream( szName.c_str(), STREAM_ACCESS_WRITE );
	pDT = CreateDataTreeSaver( pSaveXMLStream, IDataTree::WRITE );
	tree = pDT;
	tree.Add( "crosset", &crossSetDesc );	
}

void CTileSetCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown Tile Set";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CTileSetTerrainsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_BROWSEDIR;
	prop.szDefaultName = "Tiles directory";
	prop.szDisplayName = "Tiles directory";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CTileSetTerrainsItem::MyLButtonClick()
{
	values[0].szStrings.resize( 1 );
	values[0].szStrings[0] = GetDirectory( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->GetProjectFileName().c_str() );

	CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
	pFrame->SwitchToEditCrossetsMode( false );
}

void CTileSetTerrainsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	string szOldDirName = GetTerrainsDirName();
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 )
	{
		if ( !IsRelatedPath( value ) )
		{
			string szProjectName = g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->GetProjectFileName();
			string szValue = value;
			string szRelatedPath;
			bool bRes = MakeRelativePath( szProjectName.c_str(), szValue.c_str(), szRelatedPath );
			if ( bRes )
			{
				CVariant newVal = szRelatedPath;
				CTreeItem::UpdateItemValue( nItemId, newVal );
				g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->UpdatePropView( this );
			}
			else
			{
				AfxMessageBox( "Note, this project will not be portable on other computers,\nproject file name and directories with animations should be on the same drive" );
			}
		}
		
		if ( strcmp( szOldDirName.c_str(), GetTerrainsDirName()) )
		{
			CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
			pFrame->TerrainsDirChanged();
		}
		return;
	}
}

void CTileSetTerrainsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_INSERT:
			CTreeItem *pItem = new CTileSetTerrainPropsItem;
			string szName = NStr::Format( "Terrain", GetChildsCount() );
			pItem->SetItemName( szName.c_str() );
			AddChild( pItem );
			g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CTileSetTerrainsItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->DisplayInsertMenu();
	if ( nRes == ID_INSERT_TREE_ITEM )
	{
		CTreeItem *pItem = new CTileSetTerrainPropsItem;
		string szName = NStr::Format( "Terrain", GetChildsCount() );
		pItem->SetItemName( szName.c_str() );
		AddChild( pItem );
		g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->SetChangedFlag( true );
	}
}

void CTileSetTerrainPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown terrain";
	defaultValues.push_back( prop );
	
	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Crosset number";
	prop.szDisplayName = "Crosset number";
	prop.value = 1;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Mask priority";
	prop.szDisplayName = "Mask priority";
	prop.value = 3;
	defaultValues.push_back( prop );
	
	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Passability coefficient";
	prop.szDisplayName = "Passability coefficient";
	prop.value = 1.0f;
	defaultValues.push_back( prop );
	
	prop.nId = 5;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for infantry";
	prop.szDisplayName = "Passability for infantry";
	prop.value = true;
	defaultValues.push_back( prop );
	
	prop.nId = 6;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for wheels";
	prop.szDisplayName = "Passability for wheels";
	prop.value = true;
	defaultValues.push_back( prop );
	
	prop.nId = 7;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for halftracks";
	prop.szDisplayName = "Passability for halftracks";
	prop.value = true;
	defaultValues.push_back( prop );
	
	prop.nId = 8;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for tracks";
	prop.szDisplayName = "Passability for tracks";
	prop.value = true;
	defaultValues.push_back( prop );
	
	prop.nId = 9;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Has microtexture?";
	prop.szDisplayName = "Has microtexture?";
	prop.value = true;
	defaultValues.push_back( prop );
	
	prop.nId = 10;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Sound volume";
	prop.szDisplayName = "Sound volume";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 11;
	prop.nDomenType = DT_SOUND_REF;
	prop.szDefaultName = "Sound";
	prop.szDisplayName = "Sound";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 12;
	prop.nDomenType = DT_SOUND_REF;
	prop.szDefaultName = "Looped sound";
	prop.szDisplayName = "Looped sound";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 13;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Can build entrenchment?";
	prop.szDisplayName = "Can build entrenchment?";
	prop.value = true;
	defaultValues.push_back( prop );

	prop.nId = 14;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Is water?";
	prop.szDisplayName = "Is water?";
	prop.value = false;
	defaultValues.push_back( prop );

	prop.nId = 15;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Leave tracks flag";
	prop.szDisplayName = "Leave tracks flag";
	prop.value = false;
	defaultValues.push_back( prop );
	
	prop.nId = 16;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Dust flag";
	prop.szDisplayName = "Dust flag";
	prop.value = false;
	defaultValues.push_back( prop );
	
	values = defaultValues;

	SChildItem child;
	
	child.nChildItemType = E_TILESET_TILES_ITEM;
	child.szDefaultName = "Tiles";
	child.szDisplayName = "Tiles";
	defaultChilds.push_back( child );
}

void CTileSetTerrainPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 )
	{
		ChangeItemName( value );
		return;
	}

	if ( nItemId == 4 )
	{
		float fVal = GetPassability();
		if ( fVal < 0 || fVal > 1 )
		{
			AfxMessageBox( "Error: passability value must be in range [0..1]" );
			SetPassability( 1.0f );
			g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->UpdatePropView( this );
		}
	}

	if ( nItemId == 10 )
	{
		float fVal = GetSoundVolume();
		if ( fVal < 0 || fVal > 1 )
		{
			AfxMessageBox( "Error: sound volume value must be in range [0..1]" );
			SetSoundVolume( 1.0f );
			g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->UpdatePropView( this );
		}
	}
}

void CTileSetTerrainPropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->ClearPropView();
			DeleteMeInParentTreeItem();
			g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CTileSetTerrainPropsItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->DisplayDeleteMenu();
	if ( nRes == ID_MENU_DELETE_TREE_ITEM )
	{
		g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->ClearPropView();
		DeleteMeInParentTreeItem();
		g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->SetChangedFlag( true );
	}
}

void CTileSetTerrainPropsItem::MyLButtonClick()
{
	CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
	pFrame->SetActiveTerrainItem( this );
}

void CTileSetTilesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CTileSetTilesItem::InsertChildItems()
{
	CTreeItem::InsertChildItems();
	SThumbData thumbData;
	
	for ( CTreeItemList::iterator it=treeItemList.begin(); it!=treeItemList.end(); ++it )
	{
		thumbData.szThumbName = (*it)->GetItemName();
		m_thumbItems.thumbDataList.push_back( thumbData );
	}
}

void CTileSetTilesItem::MyLButtonClick()
{
	CTileSetTerrainPropsItem *pPapa = static_cast<CTileSetTerrainPropsItem *> ( GetParentTreeItem() );
	CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
	pFrame->SetActiveTerrainItem( pPapa );
}

void CTileSetASoundsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CTileSetASoundsItem::MyLButtonClick()
{
	CTileSetTerrainPropsItem *pPapa = static_cast<CTileSetTerrainPropsItem *> ( GetParentTreeItem() );
	CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
	pFrame->SetActiveTerrainItem( pPapa );
}

void CTileSetASoundsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_INSERT:
			CTreeItem *pItem = new CTileSetASoundPropsItem;
			pItem->SetItemName( "Sound" );
			AddChild( pItem );
			g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CTileSetASoundPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sound";
	prop.szDisplayName = "Sound";
	prop.value = "";
	prop.szStrings.push_back( theApp.GetEditorDataDir() + "sounds\\" );
	prop.szStrings.push_back( szSoundFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();
	
	prop.nId = 2;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Peaceful flag";
	prop.szDisplayName = "Peaceful flag";
	prop.value = true;
	defaultValues.push_back( prop );
	
	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Probability";
	prop.szDisplayName = "Probability";
	prop.value = 15.0f;
	defaultValues.push_back( prop );
	
	values = defaultValues;
}

void CTileSetASoundPropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			CParentFrame *pFrame = g_frameManager.GetActiveFrame();
			pFrame->ClearPropView();
			DeleteMeInParentTreeItem();
			pFrame->SetChangedFlag( true );
			break;
	}
}

void CTileSetASoundPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 )
	{
		if ( !IsRelatedPath( value ) )
		{
			string szValue = value;
			string szRelatedPath;
			bool bRes =	MakeSubRelativePath( theApp.GetEditorDataDir().c_str(), szValue.c_str(), szRelatedPath );
			if ( bRes )
			{
				szRelatedPath = szRelatedPath.substr( 0, szRelatedPath.rfind( '.' ) );
				CVariant newVal = szRelatedPath;
				CTreeItem::UpdateItemValue( nItemId, newVal );
				g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->UpdatePropView( this );
			}
			else
			{
				AfxMessageBox( "Error: sound file should be inside DATA directory of the game" );
				CTreeItem::UpdateItemValue( nItemId, "" );
				g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->UpdatePropView( this );
			}
		}
	}
}

void CTileSetASoundPropsItem::MyLButtonClick()
{
	CTreeItem *pPapa = GetParentTreeItem();
	pPapa = pPapa->GetParentTreeItem();
	NI_ASSERT( pPapa->GetItemType() == E_TILESET_TERRAIN_PROPS_ITEM );
	
	CTileSetTerrainPropsItem *pTerrainProps = (CTileSetTerrainPropsItem *) pPapa;
	CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
	pFrame->SetActiveTerrainItem( pTerrainProps );
}

void CTileSetLSoundsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CTileSetLSoundsItem::MyLButtonClick()
{
	CTileSetTerrainPropsItem *pPapa = static_cast<CTileSetTerrainPropsItem *> ( GetParentTreeItem() );
	CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
	pFrame->SetActiveTerrainItem( pPapa );
}

void CTileSetLSoundsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_INSERT:
			CTreeItem *pItem = new CTileSetLSoundPropsItem;
			pItem->SetItemName( "Sound" );
			AddChild( pItem );
			g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CTileSetLSoundPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sound";
	prop.szDisplayName = "Sound";
	prop.value = "";
	prop.szStrings.push_back( theApp.GetEditorDataDir() + "sounds\\" );
	prop.szStrings.push_back( szSoundFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();
	
	prop.nId = 2;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Peaceful flag";
	prop.szDisplayName = "Peaceful flag";
	prop.value = true;
	defaultValues.push_back( prop );
	
	values = defaultValues;
}

void CTileSetLSoundPropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			CParentFrame *pFrame = g_frameManager.GetActiveFrame();
			pFrame->ClearPropView();
			DeleteMeInParentTreeItem();
			pFrame->SetChangedFlag( true );
			break;
	}
}

void CTileSetLSoundPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 )
	{
		if ( !IsRelatedPath( value ) )
		{
			string szValue = value;
			string szRelatedPath;
			bool bRes =	MakeSubRelativePath( theApp.GetEditorDataDir().c_str(), szValue.c_str(), szRelatedPath );
			if ( bRes )
			{
				szRelatedPath = szRelatedPath.substr( 0, szRelatedPath.rfind( '.' ) );
				CVariant newVal = szRelatedPath;
				CTreeItem::UpdateItemValue( nItemId, newVal );
				g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->UpdatePropView( this );
			}
			else
			{
				AfxMessageBox( "Error: sound file should be inside DATA directory of the game" );
				CTreeItem::UpdateItemValue( nItemId, "" );
				g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->UpdatePropView( this );
			}
		}
	}
}

void CTileSetLSoundPropsItem::MyLButtonClick()
{
	CTreeItem *pPapa = GetParentTreeItem();
	pPapa = pPapa->GetParentTreeItem();
	NI_ASSERT( pPapa->GetItemType() == E_TILESET_TERRAIN_PROPS_ITEM );
	
	CTileSetTerrainPropsItem *pTerrainProps = (CTileSetTerrainPropsItem *) pPapa;
	CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
	pFrame->SetActiveTerrainItem( pTerrainProps );
}

void CTileSetTilePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Probability";
	prop.szDisplayName = "Probability";
	prop.value = 25.0f;
	defaultValues.push_back( prop );
	
	prop.nId = 2;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Flipped state";
	prop.szDisplayName = "Flipped state";
	prop.value = "normal and flipped";
	prop.szStrings.push_back( "normal and flipped" );
	prop.szStrings.push_back( "normal" );
	prop.szStrings.push_back( "flipped" );
	defaultValues.push_back( prop );
	
	values = defaultValues;
}

int CTileSetTilePropsItem::GetFlippedState()
{
	std::string szVal = values[1].value;
	if ( szVal == "normal and flipped" || szVal == "Normal and flipped" )
		return E_BOTH;
	if ( szVal == "normal" || szVal == "Normal" )
		return E_NORMAL;
	if ( szVal == "flipped" || szVal == "Flipped" )
		return E_FLIPPED;
	NI_ASSERT_T( 0, "Unknown flipped state" );
	return 0;
}

void CTileSetTilePropsItem::SetFippedState( int nVal )
{
	std::string szVal;
	switch ( nVal )
	{
		case E_BOTH:
			szVal = "normal and flipped";
			break;
		case E_NORMAL:
			szVal = "normal";
			break;
		case E_FLIPPED:
			szVal = "flipped";
			break;
		default:
			NI_ASSERT_T( 0, "Unknown flipped state" );
			return;
	}
	values[1].value = szVal;
}

void CTileSetTilePropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
			pFrame->RemoveTerrainIndex( nTileIndex );
			pFrame->DeleteFrameInSelectedList( (DWORD) this );
			DeleteMeInParentTreeItem();
			break;
	}
}

void CTileSetTilePropsItem::MyLButtonClick()
{
	CTreeItem *pPapa = GetParentTreeItem();
	pPapa = pPapa->GetParentTreeItem();
	NI_ASSERT( pPapa->GetItemType() == E_TILESET_TERRAIN_PROPS_ITEM );
	
	CTileSetTerrainPropsItem *pTerrainProps = (CTileSetTerrainPropsItem *) pPapa;
	CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
	pFrame->SetActiveTerrainItem( pTerrainProps );
	
	pFrame->SelectItemInSelectedThumbList( (long) this );
}

int CTileSetTilePropsItem::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CTreeItem*>(this) );
	saver.Add( "TileIndex", &nTileIndex );
	return 0;
}


void CCrossetsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_BROWSEDIR;
	prop.szDefaultName = "Crossets directory";
	prop.szDisplayName = "Crossets directory";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CCrossetsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	string szOldDirName = GetCrossetsDirName();
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 )
	{
		if ( !IsRelatedPath( value ) )
		{
			string szProjectName = g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->GetProjectFileName();
			string szValue = value;
			string szRelatedPath;
			bool bRes = MakeRelativePath( szProjectName.c_str(), szValue.c_str(), szRelatedPath );
			if ( bRes )
			{
				CVariant newVal = szRelatedPath;
				CTreeItem::UpdateItemValue( nItemId, newVal );
				g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->UpdatePropView( this );
			}
			else
			{
				AfxMessageBox( "Note, this project will not be portable on other computers,\nproject file name and directories with animations should be on the same drive" );
			}
		}
		
		if ( strcmp( szOldDirName.c_str(), GetCrossetsDirName()) )
		{
			CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
			pFrame->CrossetsDirChanged();
		}
		return;
	}
}

void CCrossetsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_INSERT:
			CTreeItem *pItem = new CCrossetPropsItem;
			string szName = NStr::Format( "Crosset", GetChildsCount() );
			pItem->SetItemName( szName.c_str() );
			AddChild( pItem );
			g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CCrossetsItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->DisplayInsertMenu();
	if ( nRes == ID_INSERT_TREE_ITEM )
	{
		CTreeItem *pItem = new CCrossetPropsItem;
		string szName = NStr::Format( "Crosset", GetChildsCount() );
		pItem->SetItemName( szName.c_str() );
		AddChild( pItem );
		g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->SetChangedFlag( true );
	}
}

void CCrossetsItem::MyLButtonClick()
{
	CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
	values[0].szStrings.resize( 1 );
	values[0].szStrings[0] = GetDirectory( pFrame->GetProjectFileName().c_str() );
	pFrame->SwitchToEditCrossetsMode( true );
}

void CCrossetPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown crosset";
	defaultValues.push_back( prop );
	
	values = defaultValues;
	
	defaultChilds.clear();
	SChildItem child;
	
	child.nChildItemType = E_CROSSET_TILES_ITEM;
	child.szDefaultName = "a";
	child.szDisplayName = "a";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_CROSSET_TILES_ITEM;
	child.szDefaultName = "b";
	child.szDisplayName = "b";
	defaultChilds.push_back( child );

	child.nChildItemType = E_CROSSET_TILES_ITEM;
	child.szDefaultName = "c";
	child.szDisplayName = "c";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_CROSSET_TILES_ITEM;
	child.szDefaultName = "d";
	child.szDisplayName = "d";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_CROSSET_TILES_ITEM;
	child.szDefaultName = "e";
	child.szDisplayName = "e";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_CROSSET_TILES_ITEM;
	child.szDefaultName = "f";
	child.szDisplayName = "f";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_CROSSET_TILES_ITEM;
	child.szDefaultName = "a'";
	child.szDisplayName = "a'";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_CROSSET_TILES_ITEM;
	child.szDefaultName = "b'";
	child.szDisplayName = "b'";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_CROSSET_TILES_ITEM;
	child.szDefaultName = "c'";
	child.szDisplayName = "c'";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_CROSSET_TILES_ITEM;
	child.szDefaultName = "d'";
	child.szDisplayName = "d'";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_CROSSET_TILES_ITEM;
	child.szDefaultName = "e'";
	child.szDisplayName = "e'";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_CROSSET_TILES_ITEM;
	child.szDefaultName = "f'";
	child.szDisplayName = "f'";
	defaultChilds.push_back( child );
}

void CCrossetPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 )
	{
		ChangeItemName( value );
		return;
	}
}

void CCrossetPropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->ClearPropView();
			DeleteMeInParentTreeItem();
			g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->SetChangedFlag( true );
			break;
	}
}

void CCrossetPropsItem::MyRButtonClick()
{
	int nRes = g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->DisplayDeleteMenu();
	if ( nRes == ID_MENU_DELETE_TREE_ITEM )
	{
		g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->ClearPropView();
		DeleteMeInParentTreeItem();
		g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->SetChangedFlag( true );
	}
}

void CCrossetPropsItem::MyLButtonClick()
{
	CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
	pFrame->SwitchToEditCrossetsMode( true );
}

void CCrossetTilesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CCrossetTilesItem::InsertChildItems()
{
	CTreeItem::InsertChildItems();
	SThumbData thumbData;
	
	for ( CTreeItemList::iterator it=treeItemList.begin(); it!=treeItemList.end(); ++it )
	{
		thumbData.szThumbName = (*it)->GetItemName();
		m_thumbItems.thumbDataList.push_back( thumbData );
	}
}

void CCrossetTilesItem::MyLButtonClick()
{
	CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
	pFrame->SetActiveCrossetItem( this );
}

void CCrossetTilePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	
	prop.nId = 1;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Probability";
	prop.szDisplayName = "Probability";
	prop.value = 25.0f;
	defaultValues.push_back( prop );
	
	values = defaultValues;
}

void CCrossetTilePropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
			CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
			pFrame->RemoveCrossetIndex( nCrossIndex );
			pFrame->DeleteFrameInSelectedList( (DWORD) this );
			DeleteMeInParentTreeItem();
			break;
	}
}

void CCrossetTilePropsItem::MyLButtonClick()
{
	CTreeItem *pPapa = GetParentTreeItem();
	NI_ASSERT( pPapa->GetItemType() == E_CROSSET_TILES_ITEM );
	
	CCrossetTilesItem *pCrossetProps = (CCrossetTilesItem *) pPapa;
	CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
	pFrame->SetActiveCrossetItem( pCrossetProps );
	
	pFrame->SelectItemInSelectedThumbList( (long) this );
}

int CCrossetTilePropsItem::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CTreeItem*>(this) );
	saver.Add( "CrossIndex", &nCrossIndex );
	return 0;
}
