#include "StdAfx.h"
#include <io.h>

#include "editor.h"
#include "SpriteCompose.h"
#include "frames.h"
#include "SpriteFrm.h"
#include "SpriteTreeItem.h"


void CSpriteTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = E_SPRITES_ITEM;
	child.szDefaultName = "Sprites";
	child.szDisplayName = "Sprites";
	defaultChilds.push_back( child );
}

void CSpriteTreeRootItem::ComposeAnimations( const char *pszProjectFileName, const char *pszResultingDir, bool bSetCycledFlag )
{
	if ( GetChildsCount() == 0 )
		return;			//пусто

	CSpritesItem *pSpritesItem = static_cast<CSpritesItem *>( GetChildItem( E_SPRITES_ITEM ) );
	CTreeItemList::const_iterator spriteIt;
	int nError = 0;

	CVectorOfStrings fileNameVector;
	CVectorOfStrings invalidNameVector;

	vector<SAnimationDesc> animDescVector( 1 );
	SAnimationDesc &animDesc = animDescVector[0];
	if ( bSetCycledFlag )
		animDesc.bCycled = true;
	else
		animDesc.bCycled = false;
	animDesc.fSpeed = 0;
	const int nLastSprite = 0 + pSpritesItem->GetChildsCount();
	animDesc.nFrameTime = pSpritesItem->GetFrameTime();
	animDesc.ptFrameShift = pSpritesItem->GetPosition();
	animDesc.szName = "effect";

	fileNameVector.resize( nLastSprite );
	animDesc.dirs.resize( 1 );

	SAnimationDesc::SDirDesc &dirDesc = animDesc.dirs[ 0 ];
	dirDesc.ptFrameShift = pSpritesItem->GetPosition();
	dirDesc.frames.resize( pSpritesItem->GetChildsCount() );

	string szDirName;
	szDirName = pSpritesItem->GetDirName();
	if ( IsRelatedPath( szDirName.c_str() ) )
	{
		string szFull, szProjectDir;
		szProjectDir = GetDirectory( pszProjectFileName );
		MakeFullPath( szProjectDir.c_str(), szDirName.c_str(), szFull );
		szDirName = szFull;
	}

	int k = 0;
	for ( spriteIt=pSpritesItem->GetBegin(); spriteIt!=pSpritesItem->GetEnd(); ++spriteIt )
	{
		dirDesc.frames[ k ] = k;
		animDesc.frames[k] = pSpritesItem->GetPosition();

		string szTempFileName = szDirName + (*spriteIt)->GetItemName() + ".tga";
		if ( _access( szTempFileName.c_str(), 04 ) == 0 )
			fileNameVector[k] = szTempFileName;
		else
		{
			invalidNameVector.push_back( szTempFileName );
			szTempFileName = "editor\\invalid.tga";
			fileNameVector[k] = szTempFileName;
			nError++;
		}

		k++;
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

	if ( k == 0 )
	{
		AfxMessageBox( "Error: no valid animations" );
		return;
	}

	SSpriteAnimationFormat spriteAnimFmt;
	CPtr<IImage> pImage = BuildAnimations( &animDescVector, &spriteAnimFmt, fileNameVector );

	if ( !pImage )
	{
		AfxMessageBox( "Composing images failed!" );
		return;
	}

	CPtr<IDataStorage> pSaveStorage = CreateStorage( pszResultingDir, STREAM_ACCESS_WRITE, STORAGE_TYPE_FILE );
	std::string szTemp = pszResultingDir;
	szTemp += "1";
	SaveCompressedTexture( pImage, szTemp.c_str() );

	CPtr<IDataStream> pSaveSAFStream = pSaveStorage->CreateStream( "1.san", STREAM_ACCESS_WRITE );
	if ( !pSaveSAFStream )
		return;
	CPtr<IStructureSaver> pSS = CreateStructureSaver( pSaveSAFStream, IStructureSaver::WRITE );
	CSaverAccessor saver = pSS;
	saver.Add( 1, &spriteAnimFmt );
}

FILETIME CSpriteTreeRootItem::FindMaximalSourceTime( const char *pszProjectFileName )
{
	FILETIME zero;
	zero.dwHighDateTime = 0;
	zero.dwLowDateTime = 0;
	if ( GetChildsCount() == 0 )
		return zero;			//пусто

	CSpritesItem *pSpritesItem = static_cast<CSpritesItem *>( GetChildItem( E_SPRITES_ITEM ) );
	CVectorOfStrings fileNameVector;

	string szDirName;
	szDirName = pSpritesItem->GetDirName();
	if ( IsRelatedPath( szDirName.c_str() ) )
	{
		string szFull, szProjectDir;
		szProjectDir = GetDirectory( pszProjectFileName );
		MakeFullPath( pszProjectFileName, szDirName.c_str(), szFull );
		szDirName = szFull;
	}

	for ( CTreeItemList::const_iterator spriteIt=pSpritesItem->GetBegin(); spriteIt!=pSpritesItem->GetEnd(); ++spriteIt )
	{
		string szTempFileName = szDirName + (*spriteIt)->GetItemName() + ".tga";
		if ( _access( szTempFileName.c_str(), 04 ) == 0 )
			fileNameVector.push_back( szTempFileName );
	}

	if ( fileNameVector.empty() )
		return zero;
	
	FILETIME nMaxTime;
	nMaxTime.dwHighDateTime = 0;
	nMaxTime.dwLowDateTime = 0;
	for ( int i=0; i<fileNameVector.size(); i++ )
	{
		FILETIME current = GetFileChangeTime( fileNameVector[i].c_str() );
		if ( current > nMaxTime )
			nMaxTime = current;
	}
	return nMaxTime;
}

void CSpritePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CSpritePropsItem::MyLButtonClick()
{
/*
	CTreeItem *pPapa = GetParentTreeItem();
	NI_ASSERT( pPapa->GetItemType() == E_UNIT_ANIMATION_PROPS_ITEM );
	
	CUnitAnimationPropsItem *pAnimProps = (CUnitAnimationPropsItem *) pPapa;
	g_frameManager.GetAnimationsFrame()->SetActiveAnimItem( pAnimProps );
*/

	CSpriteFrame *pFrame = static_cast<CSpriteFrame *> ( g_frameManager.GetFrame( CFrameManager::E_SPRITE_FRAME ) );
	pFrame->SelectItemInSelectedThumbList( (long) this );
}

void CSpritePropsItem::MyKeyDown( int nChar )
{
	switch ( nChar )
	{
		case VK_DELETE:
/*
			HTREEITEM hNextSibling = pTreeCtrl->GetNextItem( hItem, TVGN_NEXT );
			if ( hNextSibling )
			{
				CTreeItem *pNextSelItem = (CTreeItem *) pTreeCtrl->GetItemData( hNextSibling );
				if ( pNextSelItem->GetItemType() == E_UNIT_FRAME_PROPS_ITEM )
					g_frameManager.GetFrame( CFrameManager::E_SPRITE_FRAME )->SelectItemInSelectedThumbList( (DWORD) pNextSelItem );
			}
*/
			CSpriteFrame *pFrame = static_cast<CSpriteFrame *> ( g_frameManager.GetFrame( CFrameManager::E_SPRITE_FRAME ) );
			pFrame->DeleteFrameInSelectedList( (DWORD) this );
			DeleteMeInParentTreeItem();
			break;
	}
}

void CSpritesItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSEDIR;
	prop.szDefaultName = "Directory";
	prop.szDisplayName = "Directory";
	prop.value = "_.";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Frame time";
	prop.szDisplayName = "Frame time";
	prop.value = 125;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "X position";
	prop.szDisplayName = "X position";
	prop.value = 32;
	defaultValues.push_back( prop );
	
	prop.nId = 4;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Y position";
	prop.szDisplayName = "Y position";
	prop.value = 32;
	defaultValues.push_back( prop );
	
	values = defaultValues;
}

const char *CSpritesItem::GetDirName()
{
	return values[0].value;
}

int CSpritesItem::GetFrameTime()
{
	return values[1].value;
}

CVec2 CSpritesItem::GetPosition()
{
	CVec2 res;
	res.x = values[2].value;
	res.y = values[3].value;
	return res;
}

bool CSpritesItem::CopyItemTo( CTreeItem *pToItem )
{
	NI_ASSERT( pToItem->GetItemType() == E_SPRITES_ITEM );
	CSpritesItem *pTo = static_cast<CSpritesItem *> (pToItem);
	if ( !CTreeItem::CopyItemTo( pTo ) )
		return false;

	pTo->RemoveAllChilds();
	for ( CTreeItemList::iterator it=treeItemList.begin(); it!=treeItemList.end(); ++it )
	{
		CSpritePropsItem *pSprite = new CSpritePropsItem();
		(*it)->CopyItemTo( pSprite );
		pSprite->SetItemName( (*it)->GetItemName() );
		pTo->AddChild( pSprite );
	}
	return true;
}

void CSpritesItem::InsertChildItems()
{
	CTreeItem::InsertChildItems();
	SThumbData thumbData;

	for ( CTreeItemList::iterator it=treeItemList.begin(); it!=treeItemList.end(); ++it )
	{
		thumbData.szThumbName = (*it)->GetItemName();
		m_selThumbItems.thumbDataList.push_back( thumbData );
	}
}

void CSpritesItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	string szOldDirName = GetDirName();
	CTreeItem::UpdateItemValue( nItemId, value );
	
	if ( nItemId == 1 )
	{
/*
		HTREEITEM hSelected = pTreeCtrl->GetSelectedItem();
		if ( hSelected != hItem )
		{
			SelectMeInTheTree();
			g_frameManager.GetFrame( CFrameManager::E_SPRITE_FRAME )->SetActiveDirTreeItem( this );
		}
*/

		if ( !IsRelatedPath( value ) )
		{
			string szProjectName = g_frameManager.GetFrame( CFrameManager::E_SPRITE_FRAME )->GetProjectFileName();
			string szValue = value;
			string szRelatedPath;
			bool bRes = MakeRelativePath( szProjectName.c_str(), szValue.c_str(), szRelatedPath );
			if ( bRes )
			{
				CVariant newVal = szRelatedPath;
				CTreeItem::UpdateItemValue( nItemId, newVal );
				g_frameManager.GetFrame( CFrameManager::E_SPRITE_FRAME )->UpdatePropView( this );
			}
			else
			{
				AfxMessageBox( "Note, this project will not be portable on other computers,\nproject file name and directories with animations should be on the same drive" );
			}
		}
		
		if ( strcmp( szOldDirName.c_str(), GetDirName()) )
		{
			CSpriteFrame *pFrame = static_cast<CSpriteFrame *> ( g_frameManager.GetFrame( CFrameManager::E_SPRITE_FRAME ) );
			pFrame->ActiveDirNameChanged();
		}
		return;
	}
	
	if ( nItemId == 2 )
	{
		CSpriteFrame *pFrame = static_cast<CSpriteFrame *> ( g_frameManager.GetFrame( CFrameManager::E_SPRITE_FRAME ) );
		pFrame->ClearComposedFlag();
	}
}

void CSpritesItem::MyLButtonClick()
{
}
