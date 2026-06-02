#include "StdAfx.h"
#include <io.h>

#include "editor.h"
#include "SpriteCompose.h"
#include "frames.h"
#include "FenceFrm.h"
#include "FenceTreeItem.h"
#include "ObjTreeItem.h"
#include "common.h"


void CFenceTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
	
	defaultChilds.clear();

	SChildItem child;
	
	child.nChildItemType = E_FENCE_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );

	child.nChildItemType = E_OBJECT_EFFECTS_ITEM;
	child.szDefaultName = "Effects";
	child.szDisplayName = "Effects";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_FENCE_DIRECTION_ITEM;
	child.szDefaultName = "North-east";
	child.szDisplayName = "North-east";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_FENCE_DIRECTION_ITEM;
	child.szDefaultName = "North-west";
	child.szDisplayName = "North-west";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_FENCE_DIRECTION_ITEM;
	child.szDefaultName = "South-west";
	child.szDisplayName = "South-west";
	defaultChilds.push_back( child );

	child.nChildItemType = E_FENCE_DIRECTION_ITEM;
	child.szDefaultName = "South-east";
	child.szDisplayName = "South-east";
	defaultChilds.push_back( child );
}

void CFenceTreeRootItem::ComposeFences( const char *pszProjectFileName, const char *pszResultingDir, SFenceRPGStats &rpgStats )
{
	IImageProcessor *pIP = GetImageProcessor();
	CFenceCommonPropsItem *pCommonPropsItem = static_cast<CFenceCommonPropsItem *> ( GetChildItem( E_FENCE_COMMON_PROPS_ITEM ) );
	string szSourceDir = pCommonPropsItem->GetDirName();
	if ( IsRelatedPath( szSourceDir.c_str() ) )
	{
		string szFull, szProjectDir;
		szProjectDir = GetDirectory( pszProjectFileName );
		MakeFullPath( szProjectDir.c_str(), szSourceDir.c_str(), szFull );
		szSourceDir = szFull;
	}
	
	CFenceFrame *pFrame = static_cast<CFenceFrame *> ( g_frameManager.GetFrame( CFrameManager::E_FENCE_FRAME ) );
	int nError = 0;
	rpgStats.stats.resize( pFrame->GetMaxFenceIndex() );
	int nActiveFenceIndex = 0;

	CVectorOfStrings fileNameVector( pFrame->GetMaxFenceIndex() );
	CVectorOfStrings invalidNameVector;
	vector<SAnimationDesc> animDescVector( 1 );

	CVectorOfStrings shadowFileNameVector( pFrame->GetMaxFenceIndex() );
	vector<SAnimationDesc> shadowDescVector( 1 );

	SAnimationDesc &animDesc = animDescVector[0];
	animDesc.bCycled = false;
	animDesc.fSpeed = 0;
	animDesc.nFrameTime = 0;
	animDesc.ptFrameShift = CVec2( 0, 0 );
	animDesc.szName = "default";

	SAnimationDesc &shadowDesc = shadowDescVector[0];
	shadowDesc.bCycled = false;
	shadowDesc.fSpeed = 0;
	shadowDesc.nFrameTime = 0;
	shadowDesc.ptFrameShift = CVec2( 0, 0 );
	shadowDesc.szName = "default";

	animDesc.dirs.resize( 1 );
	SAnimationDesc::SDirDesc &dirDesc = animDesc.dirs[ 0 ];
	dirDesc.ptFrameShift = CVec2( 0, 0 );

	shadowDesc.dirs.resize( 1 );
	SAnimationDesc::SDirDesc &shadowDirDesc = shadowDesc.dirs[ 0 ];
	shadowDirDesc.ptFrameShift = CVec2( 0, 0 );
	
	for ( int nCurrentFenceDirection=0; nCurrentFenceDirection<4; nCurrentFenceDirection++ )
	{
		CTreeItem *pFenceDirectionItem = GetChildItem( E_FENCE_DIRECTION_ITEM, nCurrentFenceDirection );
		SFenceRPGStats::SDir dira;

		for ( int nCurrentFenceType=0; nCurrentFenceType<4; nCurrentFenceType++ )
		{
			SFenceRPGStats::SSegmentRPGStats segment;

			CTreeItem *pFenceInsertItem = pFenceDirectionItem->GetChildItem( E_FENCE_INSERT_ITEM, nCurrentFenceType );
			for ( CTreeItemList::const_iterator it=pFenceInsertItem->GetBegin(); it!=pFenceInsertItem->GetEnd(); ++it )
			{
				CFencePropsItem *pFenceProps = (CFencePropsItem *) it->GetPtr();

				string szTempFileName = szSourceDir + pFenceProps->GetItemName() + ".tga";
				if ( _access( szTempFileName.c_str(), 04 ) == 0 )
				{
					{
						string szShadowFileName = szSourceDir + pFenceProps->GetItemName() + "s.tga";
						string szTempShadow = theApp.GetEditorTempDir() + pFenceProps->GetItemName() + "s.tga";
						if ( !SaveShadowFile( szTempFileName, szShadowFileName, szTempShadow ) )
						{
							invalidNameVector.push_back( szShadowFileName );
							nError++;
						}
						else
						{
							CPtr<IDataStream> pStream = OpenFileStream( szShadowFileName.c_str(), STREAM_ACCESS_READ );
							if ( !pStream )
							{
								invalidNameVector.push_back( szShadowFileName );
								nError++;
								continue;
							}
							CPtr<IImage> pImage = pIP->LoadImage( pStream );
							
							shadowDirDesc.frames.push_back( nActiveFenceIndex );
							shadowDesc.frames[pFenceProps->nSegmentIndex] = CVec2( pImage->GetSizeX()/2, pImage->GetSizeY()/2 );
							shadowFileNameVector[pFenceProps->nSegmentIndex] = szTempShadow;
						}
					}

					CPtr<IDataStream> pStream = OpenFileStream( szTempFileName.c_str(), STREAM_ACCESS_READ );
					if ( !pStream )
					{
						invalidNameVector.push_back( szTempFileName );
						nError++;
						continue;
					}
					CPtr<IImage> pImage = pIP->LoadImage( pStream );

					pFrame->FillSegmentProps( pFenceProps, segment );
					rpgStats.stats[pFenceProps->nSegmentIndex] = segment;
					if ( nCurrentFenceType == 0 )
						dira.centers.push_back( pFenceProps->nSegmentIndex );
					else if ( nCurrentFenceType == 1 )
						dira.ldamages.push_back( pFenceProps->nSegmentIndex );
					else if ( nCurrentFenceType == 2 )
						dira.rdamages.push_back( pFenceProps->nSegmentIndex );
					else if ( nCurrentFenceType == 3 )
						dira.cdamages.push_back( pFenceProps->nSegmentIndex );
					else
						NI_ASSERT( 0 );			//WTF?
					
					dirDesc.frames.push_back( nActiveFenceIndex );
					animDesc.frames[pFenceProps->nSegmentIndex] = CVec2( pImage->GetSizeX()/2, pImage->GetSizeY()/2 );
					fileNameVector[pFenceProps->nSegmentIndex] = szTempFileName;
					nActiveFenceIndex++;
				}
				else
				{
					invalidNameVector.push_back( szTempFileName );
					nError++;
				}
			}
		}

		rpgStats.dirs.push_back( dira );
	}


	if ( nError > 0 )
	{
		CString szErrorStr;
		szErrorStr.Format( "Can not find files total count %d\n", nError );
		for ( int i=0; i<invalidNameVector.size(); i++ )
		{
			szErrorStr += invalidNameVector[i].c_str();
			szErrorStr += '\n';
		}
		
		AfxMessageBox( szErrorStr );
	}
	
	if ( fileNameVector.size() == 0 )
	{
		AfxMessageBox( "Error: no valid pictures" );
		return;
	}
	
	CPtr<IDataStorage> pSaveStorage = CreateStorage( pszResultingDir, STREAM_ACCESS_WRITE, STORAGE_TYPE_FILE );
	SSpriteAnimationFormat spriteAnimFmt;
	CPtr<IImage> pImage = BuildAnimations( &animDescVector, &spriteAnimFmt, fileNameVector, true, 1 );
	if ( !pImage )
	{
		AfxMessageBox( "Composing fences failed!" );
		return;
	}
	std::string szRes = pszResultingDir;
	szRes += "1";
	SaveCompressedTexture( pImage, szRes.c_str() );
	
	CPtr<IDataStream> pSaveSAFStream = pSaveStorage->CreateStream( "1.san", STREAM_ACCESS_WRITE );
	if ( !pSaveSAFStream )
		return;
	CPtr<IStructureSaver> pSS = CreateStructureSaver( pSaveSAFStream, IStructureSaver::WRITE );
	CSaverAccessor saver = pSS;
	saver.Add( 1, &spriteAnimFmt );

	if ( shadowFileNameVector.empty() )
	{
		AfxMessageBox( "Composing shadows failed, there is no valid shadows!" );
		return;
	}

	SSpriteAnimationFormat shadowAnimFmt;
	pImage = BuildAnimations( &shadowDescVector, &shadowAnimFmt, shadowFileNameVector );
	if ( !pImage )
	{
		AfxMessageBox( "Composing shadows failed!" );
		return;
	}
	szRes = pszResultingDir;
	szRes += "1s";
	SaveCompressedShadow( pImage, szRes.c_str() );
	
	pSaveSAFStream = pSaveStorage->CreateStream( "1s.san", STREAM_ACCESS_WRITE );
	pSS = CreateStructureSaver( pSaveSAFStream, IStructureSaver::WRITE );
	saver = pSS;
	saver.Add( 1, &shadowAnimFmt );

	for ( int i=0; i<shadowFileNameVector.size(); i++ )
	{
		remove( shadowFileNameVector[i].c_str() );
	}
}

bool CFenceTreeRootItem::SaveShadowFile( const string &szFenceFileName, const string &szShadowFileName, const string &szTempShadow )
{
	IImageProcessor *pIP = GetImageProcessor();

	CPtr<IDataStream> pFenceStream = OpenFileStream( szFenceFileName.c_str(), STREAM_ACCESS_READ );
	if ( pFenceStream == 0 )
		return false;
	CPtr<IImage> pSpriteImage = pIP->LoadImage( pFenceStream );
	if ( pSpriteImage == 0 )
		return false;
	CPtr<IImage> pInverseSprite = pSpriteImage->Duplicate();
	pInverseSprite->SharpenAlpha( 100 );
	pInverseSprite->InvertAlpha();

	CPtr<IDataStream> pShadowStream = OpenFileStream( szShadowFileName.c_str(), STREAM_ACCESS_READ );
	if ( pShadowStream == 0 )
		return false;
	CPtr<IImage> pShadowImage = pIP->LoadImage( pShadowStream );
	if ( pShadowImage == 0 )
		return false;
	if ( pInverseSprite->GetSizeX() != pShadowImage->GetSizeX() || pInverseSprite->GetSizeY() != pShadowImage->GetSizeY() )
	{
		string szErr = "The size of sprite is not equal the size of shadow: ";
		szErr += szFenceFileName;
		szErr += ",  ";
		szErr += szShadowFileName;

		NI_ASSERT_T( 0, szErr.c_str() );
		return false;
	}
	RECT rc;
	rc.left = 0;
	rc.top = 0;
	rc.right = pInverseSprite->GetSizeX();
	rc.bottom = pInverseSprite->GetSizeY();
	pShadowImage->ModulateAlphaFrom( pInverseSprite, &rc, 0, 0 );
	pShadowImage->SetColor( DWORD(0) );

	CPtr<IDataStream> pSaveShadowStream = OpenFileStream( szTempShadow.c_str(), STREAM_ACCESS_WRITE );
	pIP->SaveImageAsTGA( pSaveShadowStream, pShadowImage );

	return true;
}

void CFenceCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown Fence";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_BROWSEDIR;
	prop.szDefaultName = "Fences directory";
	prop.szDisplayName = "Fences directory";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Health";
	prop.szDisplayName = "Health";
	prop.value = 100;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Absorbtion";
	prop.szDisplayName = "Armor";
	prop.value = 20;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for infantry";
	prop.szDisplayName = "Passability for infantry";
	prop.value = false;
	defaultValues.push_back( prop );
	
	prop.nId = 6;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for wheels";
	prop.szDisplayName = "Passability for wheels";
	prop.value = false;
	defaultValues.push_back( prop );
	
	prop.nId = 7;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for halftracks";
	prop.szDisplayName = "Passability for halftracks";
	prop.value = false;
	defaultValues.push_back( prop );
	
	prop.nId = 8;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for tracks";
	prop.szDisplayName = "Passability for tracks";
	prop.value = false;
	defaultValues.push_back( prop );
	
	values = defaultValues;
}

void CFenceCommonPropsItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	string szOldDirName = GetDirName();
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 2 )
	{
		if ( !IsRelatedPath( value ) )
		{
			string szProjectName = g_frameManager.GetFrame( CFrameManager::E_FENCE_FRAME )->GetProjectFileName();
			string szValue = value;
			string szRelatedPath;
			bool bRes = MakeRelativePath( szProjectName.c_str(), szValue.c_str(), szRelatedPath );
			if ( bRes )
			{
				CVariant newVal = szRelatedPath;
				CTreeItem::UpdateItemValue( nItemId, newVal );
				g_frameManager.GetFrame( CFrameManager::E_FENCE_FRAME )->UpdatePropView( this );
			}
			else
			{
				AfxMessageBox( "Note, this project will not be portable on other computers,\nproject file name and directories with animations should be on the same drive" );
			}
		}
		
		if ( strcmp( szOldDirName.c_str(), GetDirName()) )
		{
			CFenceFrame *pFrame = static_cast<CFenceFrame *> ( g_frameManager.GetFrame( CFrameManager::E_FENCE_FRAME ) );
			pFrame->ActiveDirNameChanged();
		}
		return;
	}
}

void CFenceDirectionItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	
	SChildItem child;
	
	child.nChildItemType = E_FENCE_INSERT_ITEM;
	child.szDefaultName = "Safe";
	child.szDisplayName = "Safe";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_FENCE_INSERT_ITEM;
	child.szDefaultName = "Destroyed left";
	child.szDisplayName = "Destroyed left";
	defaultChilds.push_back( child );

	child.nChildItemType = E_FENCE_INSERT_ITEM;
	child.szDefaultName = "Destroyed right";
	child.szDisplayName = "Destroyed right";
	defaultChilds.push_back( child );
	
	child.nChildItemType = E_FENCE_INSERT_ITEM;
	child.szDefaultName = "Full destroyed";
	child.szDisplayName = "Full destroyed";
	defaultChilds.push_back( child );
}

void CFenceInsertItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CFenceInsertItem::InsertChildItems()
{
	CTreeItem::InsertChildItems();
	SThumbData thumbData;
	
	for ( CTreeItemList::iterator it=treeItemList.begin(); it!=treeItemList.end(); ++it )
	{
		thumbData.szThumbName = (*it)->GetItemName();
		m_thumbItems.thumbDataList.push_back( thumbData );
	}
}

void CFenceInsertItem::MyLButtonClick()
{
	CFenceFrame *pFrame = static_cast<CFenceFrame *> ( g_frameManager.GetFrame( CFrameManager::E_FENCE_FRAME ) );
	pFrame->SetActiveFenceInsertItem( this );
}

void CFencePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CFencePropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
/*
			HTREEITEM hNextSibling = pTreeCtrl->GetNextItem( hItem, TVGN_NEXT );
			if ( hNextSibling )
			{
				CTreeItem *pNextSelItem = (CTreeItem *) pTreeCtrl->GetItemData( hNextSibling );
				if ( pNextSelItem->GetItemType() == E_FENCE_PROPS_ITEM )
					g_frameManager.GetFrame( CFrameManager::E_FENCE_FRAME )->SelectItemInSelectedThumbList( (DWORD) pNextSelItem );
			}
*/

			CFenceFrame *pFrame = static_cast<CFenceFrame *> ( g_frameManager.GetFrame( CFrameManager::E_FENCE_FRAME ) );
			pFrame->RemoveFenceIndex( nSegmentIndex );
			pFrame->DeleteFrameInSelectedList( (DWORD) this );
			DeleteMeInParentTreeItem();
			break;
	}
}

void CFencePropsItem::MyLButtonClick()
{
	CTreeItem *pPapa = GetParentTreeItem();
	NI_ASSERT( pPapa->GetItemType() == E_FENCE_INSERT_ITEM );
	
	CFenceInsertItem *pFenceInsertItem = (CFenceInsertItem *) pPapa;
	CFenceFrame *pFrame = static_cast<CFenceFrame *> ( g_frameManager.GetFrame( CFrameManager::E_FENCE_FRAME ) );
	pFrame->SetActiveFenceInsertItem( pFenceInsertItem );
	
	pFrame->SelectItemInSelectedThumbList( (long) this );

	pFrame->EditFence( this );
}

int CFencePropsItem::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CTreeItem*>(this) );
	saver.Add( "SpritePos", &vSpritePos );
	saver.Add( "SegmentIndex", &nSegmentIndex );

	CFenceFrame *pFrame = static_cast<CFenceFrame *> ( g_frameManager.GetFrame( CFrameManager::E_FENCE_FRAME ) );
	if ( !saver.IsReading() )
	{
		pFrame->SaveMyData( this, saver );
	}
	else
	{
		bLoaded = true;
		pFrame->LoadMyData( this, saver );
	}
	return 0;
}
