#include "stdafx.h"
#include <io.h>
#include "..\Common\StingrayCompat.h"

#include "..\GFX\GFX.h"
#include "..\Scene\Scene.h"
#include "..\Anim\Animation.h"
#include "..\Main\rpgstats.h"
#include "..\Image\image.h"

#include "editor.h"
#include "PropView.h"
#include "TreeItem.h"
#include "TileSetFrm.h"
#include "TileSetView.h"
#include "GameWnd.h"
#include "frames.h"
#include "MainFrm.h"

#define ID_ALL_DIR_THUMB_ITEMS  2000
#define ID_SELECTED_THUMB_ITEMS 2001


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static const int THUMB_LIST_WIDTH = 145;

/////////////////////////////////////////////////////////////////////////////
// CTileSetFrame

IMPLEMENT_DYNCREATE(CTileSetFrame, CParentFrame)

BEGIN_MESSAGE_MAP(CTileSetFrame, CParentFrame)
	//{{AFX_MSG_MAP(CTileSetFrame)
	ON_WM_CREATE()
	ON_COMMAND(ID_IMPORT_TERRAINS, OnImportTerrains)
	ON_UPDATE_COMMAND_UI(ID_IMPORT_TERRAINS, OnUpdateImportTerrains)
	ON_COMMAND(ID_IMPORT_CROSSETS, OnImportCrossets)
	ON_UPDATE_COMMAND_UI(ID_IMPORT_CROSSETS, OnUpdateImportCrossets)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTileSetFrame construction/destruction

CTileSetFrame::CTileSetFrame() : m_wndSelectedThumbItems( true )
{
	szComposerName = "Terrain Editor";
	szExtension = "*.til";
	szComposerSaveName = "TileSet_Composer_Project";
	nTreeRootItemID = E_TILESET_ROOT_ITEM;
	nFrameType = CFrameManager::E_TILESET_FRAME;
	pWndView = new CTileSetView;
	szAddDir = "terrain\\sets\\";
	
	m_pTerrainsItem = 0;
	m_pActiveTerrainItem = 0;
	m_pCrossetsItem = 0;
	m_pActiveCrossetItem = 0;
	bEditCrossets = false;

	m_nCompressedFormat = GFXPF_DXT1;
	m_nLowFormat = GFXPF_ARGB0565;
}

CTileSetFrame::~CTileSetFrame()
{
}

int CTileSetFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CParentFrame::OnCreate(lpCreateStruct) == -1)
		return -1;
	g_frameManager.AddFrame( this );

	// create a view to occupy the client area of the frame
	if (!pWndView->Create(NULL, NULL,  WS_CHILD | WS_VISIBLE, 
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}

	if ( !m_wndAllDirThumbItems.Create( 0, "All dir thumb items", WS_CHILD | WS_VISIBLE,
		CRect(0, 0, 0, 0), pWndView, ID_ALL_DIR_THUMB_ITEMS) )
	{
		TRACE0("Failed to create All Dir Thumb Items window\n");
		return -1;
	}
	
	if ( !m_wndSelectedThumbItems.Create( 0, "Selected thumb items", WS_CHILD | WS_VISIBLE,
		CRect(0, 0, 0, 0), pWndView, ID_SELECTED_THUMB_ITEMS) )
	{
		TRACE0("Failed to create Selected thumb items window\n");
		return -1;
	}
	
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// CTileSetFrame message handlers

void CTileSetFrame::ShowFrameWindows( int nCommand )
{
	CParentFrame::ShowFrameWindows( nCommand );
	g_frameManager.GetGameWnd()->ShowWindow( SW_HIDE );
}

void CTileSetFrame::GFXDraw()
{
	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER, 0x80808080 );
	pGFX->BeginScene();

	GetSingleton<IGameTimer>()->Update( timeGetTime() );
	pGFX->SetShadingEffect( 2 );
	pGFX->SetTexture( 0, 0 );

	ICamera*	pCamera = GetSingleton<ICamera>();
	IScene *pSG = GetSingleton<IScene>();
	IGameTimer *pTimer = GetSingleton<IGameTimer>();
  pSG->Draw( pCamera );
	
	pGFX->EndScene();
	pGFX->Flip();
}

void CTileSetFrame::ViewSizeChanged()
{
	CRect viewRect;
	pWndView->GetClientRect( viewRect );

	RECT rc;
	rc.left = viewRect.left;
	rc.top = viewRect.bottom - THUMB_LIST_WIDTH;
	rc.right = viewRect.right;
	rc.bottom = viewRect.bottom;
	if ( m_wndSelectedThumbItems.GetSafeHwnd() )
	{
		m_wndSelectedThumbItems.MoveWindow( &rc );
//		m_wndSelectedThumbItems.SetWindowPos( &wndTopMost, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, SWP_NOZORDER|SWP_NOACTIVATE );
	}

	rc.bottom = rc.top;
	rc.top = viewRect.top;
	if ( m_wndAllDirThumbItems.GetSafeHwnd() )
	{
		m_wndAllDirThumbItems.MoveWindow( &rc );
//		m_wndAllDirThumbItems.SetWindowPos( &wndTopMost, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, SWP_NOZORDER|SWP_NOACTIVATE );
	}
}

void CTileSetFrame::ClickOnThumbList( int nID )
{
	if ( nID == ID_ALL_DIR_THUMB_ITEMS )
	{
		//Выделяем в дереве текущую директорию с анимациями
		if ( !bEditCrossets )
		{
			if ( m_pActiveTerrainItem )
				m_pActiveTerrainItem->SelectMeInTheTree();
		}
		else
		{
			if ( m_pActiveCrossetItem )
				m_pActiveCrossetItem->SelectMeInTheTree();
		}
	}
	else if ( nID == ID_SELECTED_THUMB_ITEMS )
	{
		//Выделяем в дереве item с user data в selected thumb list
		int nSel = m_wndSelectedThumbItems.GetSelectedItemIndex();
		if ( nSel == -1 )
			return;

		CTreeItem *pTreeItem = (CTreeItem *) m_wndSelectedThumbItems.GetUserDataForItem( nSel );
		pTreeItem->SelectMeInTheTree();
	}
}

void CTileSetFrame::DoubleClickOnThumbList( int nID )
{
	if ( nID != ID_ALL_DIR_THUMB_ITEMS )
		return;

	if ( !bEditCrossets )
	{
		//Добавляем новый элемент в текущую terrain диру дерева и в список накиданных тайлов
		if ( !m_pActiveTerrainItem )
			return;
		SetChangedFlag( true );
		
		int nAllIndex = m_wndAllDirThumbItems.GetSelectedItemIndex();
		if ( nAllIndex == -1 )
			return;
		string szItemName = m_wndAllDirThumbItems.GetItemName( nAllIndex );
		
		//так как не может быть двух одинаковых тайлов, отслеживаем это дело
		for ( int i=0; i<m_wndSelectedThumbItems.GetThumbsCount(); i++ )
		{
			string szExistName = m_wndSelectedThumbItems.GetItemName( i );
			if ( szItemName == szExistName )
				return;
		}
		int nImage = m_wndAllDirThumbItems.GetItemImageIndex( nAllIndex );
		
		int nNewItemIndex = m_wndSelectedThumbItems.InsertItemToEnd( szItemName.c_str(), nImage );
		//		int nNewItemIndex = m_wndSelectedThumbItems.InsertItemAfterSelection( szFileName, m_pActiveTerrainItem->GetDirName() );
		NI_ASSERT( nNewItemIndex != -1 );
		
		//Добавляем sprite в дерево в m_pActiveTerrainItem
		CTileSetTilePropsItem *pTileProps = new CTileSetTilePropsItem();
		pTileProps->SetItemName( szItemName.c_str() );
		pTileProps->nTileIndex = GetFreeTerrainIndex();
		m_pActiveTerrainItem->AddChild( pTileProps );
		m_wndSelectedThumbItems.SetUserDataForItem( nNewItemIndex, (long) pTileProps );
	}
	else
	{
		//Добавляем новый элемент в текущую crosset диру дерева и в список накиданных тайлов
		if ( !m_pActiveCrossetItem )
			return;
		SetChangedFlag( true );
		
		int nAllIndex = m_wndAllDirThumbItems.GetSelectedItemIndex();
		if ( nAllIndex == -1 )
			return;
		string szItemName = m_wndAllDirThumbItems.GetItemName( nAllIndex );
		
		//так как не может быть двух одинаковых тайлов, отслеживаем это дело
		for ( int i=0; i<m_wndSelectedThumbItems.GetThumbsCount(); i++ )
		{
			string szExistName = m_wndSelectedThumbItems.GetItemName( i );
			if ( szItemName == szExistName )
				return;
		}
		int nImage = m_wndAllDirThumbItems.GetItemImageIndex( nAllIndex );
		
		int nNewItemIndex = m_wndSelectedThumbItems.InsertItemToEnd( szItemName.c_str(), nImage );
		//		int nNewItemIndex = m_wndSelectedThumbItems.InsertItemAfterSelection( szFileName, m_pActiveCrossetItem->GetDirName() );
		NI_ASSERT( nNewItemIndex != -1 );
		
		//Добавляем sprite в дерево в m_pActiveCrossetItem
		CCrossetTilePropsItem *pTileProps = new CCrossetTilePropsItem();
		pTileProps->SetItemName( szItemName.c_str() );
		pTileProps->nCrossIndex = GetFreeCrossetIndex();
		m_pActiveCrossetItem->AddChild( pTileProps );
		m_wndSelectedThumbItems.SetUserDataForItem( nNewItemIndex, (long) pTileProps );
	}
}

void CTileSetFrame::SelectItemInSelectedThumbList( DWORD dwData )
{
	int nIndex = m_wndSelectedThumbItems.GetItemIndexWithUserData( dwData );
	m_wndSelectedThumbItems.SelectItem( nIndex );
}

void CTileSetFrame::DeleteFrameInSelectedList( DWORD dwData )
{
	SetChangedFlag( true );
	ClearPropView();
	int nIndex = m_wndSelectedThumbItems.GetItemIndexWithUserData( dwData );
	m_wndSelectedThumbItems.DeleteItem( nIndex );
}

void CTileSetFrame::DeleteFrameInTree( int nID )
{
	if ( nID == ID_ALL_DIR_THUMB_ITEMS )
		return;
	
	SetChangedFlag( true );
	
	//Находим выделенный элемент
	int nSel = m_wndSelectedThumbItems.GetSelectedItemIndex();
	if ( nSel == -1 )
		return;
	DWORD dwData = m_wndSelectedThumbItems.GetUserDataForItem( nSel );
	ASSERT( dwData != 0 );
	
	//Удаляем frame из дерева
	CTreeItem *pFrame = (CTreeItem *) dwData;
	//	NI_ASSERT( pFrame->GetItemType() == E_TILESET_TILE_PROPS_ITEM );
	if ( pFrame->GetItemType() == E_TILESET_TILE_PROPS_ITEM )
	{
		CTileSetTilePropsItem *pProps = static_cast<CTileSetTilePropsItem *> ( pFrame );
		RemoveTerrainIndex( pProps->nTileIndex );
	}
	else
	{
		CCrossetTilePropsItem *pProps = static_cast<CCrossetTilePropsItem *> ( pFrame );
		RemoveCrossetIndex( pProps->nCrossIndex );
	}

	pFrame->DeleteMeInParentTreeItem();

	//Выделяем следующий элемент в списке
	m_wndSelectedThumbItems.SelectItem( nSel + 1 );

	//Удаляем элемент в списке
	m_wndSelectedThumbItems.DeleteItem( nSel );
}

void CTileSetFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();
	
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	NI_ASSERT( pTree != 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	NI_ASSERT( pRootItem != 0 );
	
	m_pTerrainsItem = (CTileSetTerrainsItem *) ( pRootItem->GetChildItem( E_TILESET_TERRAINS_ITEM ) );
	ASSERT( m_pTerrainsItem != 0 );
	
	//Сперва загружаем невалидную иконку, она всегда будет под индексом 0
	string szEditorDataDir = theApp.GetEditorDataDir();
	szEditorDataDir += "editor\\";
	m_wndAllDirThumbItems.LoadImageToImageList( m_pTerrainsItem->GetImageList(), "invalid.tga", szEditorDataDir.c_str() );
	
	//загружаем все тайлы из директории
	string szFullName;
	MakeFullPath( GetDirectory(szProjectFileName.c_str()).c_str(), m_pTerrainsItem->GetTerrainsDirName(), szFullName );
	m_wndAllDirThumbItems.LoadAllImagesFromDir( m_pTerrainsItem->GetThumbItems(), m_pTerrainsItem->GetImageList(), szFullName.c_str() );
	
	m_wndAllDirThumbItems.SetActiveThumbItems( m_pTerrainsItem->GetThumbItems(), m_pTerrainsItem->GetImageList() );
	m_wndSelectedThumbItems.LoadImageIndexFromThumbs( m_pTerrainsItem->GetThumbItems(), m_pTerrainsItem->GetImageList() );
	
	m_pCrossetsItem = (CCrossetsItem *) ( pRootItem->GetChildItem( E_CROSSETS_ITEM ) );
	ASSERT( m_pCrossetsItem != 0 );
	//загружаем невалидную иконку, она всегда будет под индексом 0
	m_wndAllDirThumbItems.LoadImageToImageList( m_pCrossetsItem->GetImageList(), "invalid.tga", szEditorDataDir.c_str() );

	InitFreeTerrainIndexes( pRootItem );
	InitFreeCrossetIndexes( pRootItem );
}

void CTileSetFrame::SpecificClearBeforeBatchMode()
{
	m_wndAllDirThumbItems.SetActiveThumbItems( 0, 0 );
	m_wndSelectedThumbItems.SetActiveThumbItems( 0, 0 );
	m_pTerrainsItem = 0;
	m_pActiveTerrainItem = 0;
	m_pCrossetsItem = 0;
	m_pActiveCrossetItem = 0;
	freeTerrainIndexes.clear();
	freeCrossetIndexes.clear();
	bEditCrossets = false;
}

void CTileSetFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	//обновляем индексы terrains, если они еще не были проиндексированы
	InitFreeTerrainIndexes( pRootItem );

	//обновляем индексы crossets, если они еще не были проиндексированы
	InitFreeCrossetIndexes( pRootItem );
}

void CTileSetFrame::InitFreeTerrainIndexes( CTreeItem *pRootItem )
{
	CTileSetTerrainsItem *pTerrainsItem = static_cast<CTileSetTerrainsItem *> ( pRootItem->GetChildItem( E_TILESET_TERRAINS_ITEM ) );
	NI_ASSERT( pTerrainsItem != 0 );
	
	int nIndex = 0;
	std::set<int> indexSet;
	for ( CTreeItem::CTreeItemList::const_iterator it=pTerrainsItem->GetBegin(); it!=pTerrainsItem->GetEnd(); ++it )
	{
		CTreeItem *pTiles = (*it)->GetChildItem( E_TILESET_TILES_ITEM );
		for ( CTreeItem::CTreeItemList::const_iterator in=pTiles->GetBegin(); in!=pTiles->GetEnd(); ++in )
		{
			CTileSetTilePropsItem *pTileProps = static_cast<CTileSetTilePropsItem *> ( in->GetPtr() );
			if ( pTileProps->nTileIndex == -1 )
				pTileProps->nTileIndex = nIndex;
			indexSet.insert( pTileProps->nTileIndex );
			nIndex++;
		}
	}
	
	freeTerrainIndexes.clear();
	int nPrev = -1;
	for ( std::set<int>::iterator it=indexSet.begin(); it!=indexSet.end(); ++it )
	{
		if ( *it != nPrev + 1 )				//если есть пустые индексы
		{
			for ( int i=nPrev+1; i!=*it; i++ )
				freeTerrainIndexes.push_back( i );
		}
		nPrev = *it;
	}
	freeTerrainIndexes.push_back( nPrev + 1 );			//это самый последний индекс
	//теперь freeTerrainIndexes должны быть отсортированы по возрастанию
}

void CTileSetFrame::InitFreeCrossetIndexes( CTreeItem *pRootItem )
{
	CCrossetsItem *pCrossetsItem = static_cast<CCrossetsItem *> ( pRootItem->GetChildItem( E_CROSSETS_ITEM ) );
	NI_ASSERT( pCrossetsItem != 0 );
	
	int nIndex = 0;
	std::set<int> indexSet;
	for ( CTreeItem::CTreeItemList::const_iterator it=pCrossetsItem->GetBegin(); it!=pCrossetsItem->GetEnd(); ++it )
	{
		//		CTileSetCrossetPropsItem *pCrossetsProps = static_cast<CTileSetCrossetPropsItem *> ( it->GetPtr() );
		for ( CTreeItem::CTreeItemList::const_iterator in=(*it)->GetBegin(); in!=(*it)->GetEnd(); ++in )
		{
			for ( CTreeItem::CTreeItemList::const_iterator z=(*in)->GetBegin(); z!=(*in)->GetEnd(); ++z )
			{
				CCrossetTilePropsItem *pCrossProps = static_cast<CCrossetTilePropsItem *> ( z->GetPtr() );
				if ( pCrossProps->nCrossIndex == -1 )
					pCrossProps->nCrossIndex = nIndex;
				indexSet.insert( pCrossProps->nCrossIndex );
				nIndex++;
			}
		}
	}
	
	freeCrossetIndexes.clear();
	int nPrev = -1;
	for ( std::set<int>::iterator it=indexSet.begin(); it!=indexSet.end(); ++it )
	{
		if ( *it != nPrev + 1 )				//если есть пустые индексы
		{
			for ( int i=nPrev+1; i!=*it; i++ )
				freeCrossetIndexes.push_back( i );
		}
		nPrev = *it;
	}
	freeCrossetIndexes.push_back( nPrev + 1 );			//это самый последний индекс
	//теперь freeCrossetIndexes должны быть отсортированы по возрастанию
}

void CTileSetFrame::SwitchToEditCrossetsMode( bool bMode )
{
	if ( bEditCrossets == bMode )
		return;
	
	bEditCrossets = bMode;
	if ( bEditCrossets )
	{
		if ( !m_pCrossetsItem->GetLoadedFlag() )
		{
			//загружаем все тайлы из директории
			string szFullName;
			MakeFullPath( GetDirectory(szProjectFileName.c_str()).c_str(), m_pCrossetsItem->GetCrossetsDirName(), szFullName );
			m_wndAllDirThumbItems.LoadAllImagesFromDir( m_pCrossetsItem->GetThumbItems(), m_pCrossetsItem->GetImageList(), szFullName.c_str(), true );
			
			m_wndAllDirThumbItems.SetActiveThumbItems( m_pCrossetsItem->GetThumbItems(), m_pCrossetsItem->GetImageList() );
//			m_wndSelectedThumbItems.LoadImageIndexFromThumbs( m_pCrossetsItem->GetThumbItems(), m_pCrossetsItem->GetImageList() );
			m_pCrossetsItem->SetLoadedFlag( true );
		}
		else
		{
			m_wndAllDirThumbItems.SetActiveThumbItems( m_pCrossetsItem->GetThumbItems(), m_pCrossetsItem->GetImageList() );
		}
		m_wndSelectedThumbItems.SetActiveThumbItems( 0, 0 );
		m_pActiveCrossetItem = 0;
	}
	else
	{
		m_wndAllDirThumbItems.SetActiveThumbItems( m_pTerrainsItem->GetThumbItems(), m_pTerrainsItem->GetImageList() );
		m_wndSelectedThumbItems.SetActiveThumbItems( 0, 0 );
		m_pActiveTerrainItem = 0;
	}
}

bool CTileSetFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	//Составляем один большой .tga, пользуясь данными всех анимаций
	NI_ASSERT( pRootItem->GetItemType() == E_TILESET_ROOT_ITEM );
	CTileSetTreeRootItem *pTileSetRoot = (CTileSetTreeRootItem *) pRootItem;
	pTileSetRoot->ComposeTiles( pszProjectName, pszResultFileName );
	return true;
}

void CTileSetFrame::TerrainsDirChanged()
{
	ASSERT( m_pTerrainsItem != 0 );
	SetChangedFlag( true );

	//так как директория задается относительно, здесь я должен собрать полный путь
	string szDir = GetDirectory( szProjectFileName.c_str() );
	string szFull;
	bool bRes = MakeFullPath( szDir.c_str(), m_pTerrainsItem->GetTerrainsDirName(), szFull );
	if ( !bRes )
		szFull = m_pTerrainsItem->GetTerrainsDirName();

	m_wndAllDirThumbItems.LoadAllImagesFromDir( m_pTerrainsItem->GetThumbItems(), m_pTerrainsItem->GetImageList(), szFull.c_str() );
	m_wndAllDirThumbItems.SetActiveThumbItems( m_pTerrainsItem->GetThumbItems(), m_pTerrainsItem->GetImageList() );
	m_wndSelectedThumbItems.LoadImageIndexFromThumbs( m_pTerrainsItem->GetThumbItems(), m_pTerrainsItem->GetImageList() );
}

void CTileSetFrame::CrossetsDirChanged()
{
	ASSERT( m_pCrossetsItem != 0 );
	SetChangedFlag( true );
	
	//так как директория задается относительно, здесь я должен собрать полный путь
	string szDir = GetDirectory( szProjectFileName.c_str() );
	string szFull;
	bool bRes = MakeFullPath( szDir.c_str(), m_pCrossetsItem->GetCrossetsDirName(), szFull );
	if ( !bRes )
		szFull = m_pCrossetsItem->GetCrossetsDirName();
	
	m_wndAllDirThumbItems.LoadAllImagesFromDir( m_pCrossetsItem->GetThumbItems(), m_pCrossetsItem->GetImageList(), szFull.c_str(), true );
	m_wndAllDirThumbItems.SetActiveThumbItems( m_pCrossetsItem->GetThumbItems(), m_pCrossetsItem->GetImageList() );
	m_wndSelectedThumbItems.LoadImageIndexFromThumbs( m_pCrossetsItem->GetThumbItems(), m_pCrossetsItem->GetImageList() );
}

void CTileSetFrame::SetActiveTerrainItem( CTileSetTerrainPropsItem *pTerrainItem )
{
	SwitchToEditCrossetsMode( false );
	if ( pTerrainItem == m_pActiveTerrainItem )
		return;
	
	m_pActiveTerrainItem = pTerrainItem;
	if ( !m_pActiveTerrainItem )
		return;
	
	CTileSetTilesItem *pTilesItem = static_cast<CTileSetTilesItem *> ( m_pActiveTerrainItem->GetChildItem( E_TILESET_TILES_ITEM ) );
	
	m_wndSelectedThumbItems.SetActiveThumbItems( pTilesItem->GetThumbItems(), 0 );
	m_wndSelectedThumbItems.LoadImageIndexFromThumbs( m_pTerrainsItem->GetThumbItems(), m_pTerrainsItem->GetImageList() );
	
	if ( !pTilesItem->GetLoadedFlag() )
	{
		//Привязываем items в списке к items в дереве
		NI_ASSERT( m_wndSelectedThumbItems.GetThumbsCount() == pTilesItem->GetChildsCount() );
		CTreeItem::CTreeItemList::const_iterator it;
		int i = 0;
		for ( it=pTilesItem->GetBegin(); it!=pTilesItem->GetEnd(); ++it )
		{
			m_wndSelectedThumbItems.SetUserDataForItem( i, (DWORD) it->GetPtr() );
			i++;
		}
		pTilesItem->SetLoadedFlag( true );
	}
}

void CTileSetFrame::SetActiveCrossetItem( CCrossetTilesItem *pCrossetItem )
{
	SwitchToEditCrossetsMode( true );
	if ( pCrossetItem == m_pActiveCrossetItem )
		return;
	
	m_pActiveCrossetItem = pCrossetItem;
	if ( !m_pActiveCrossetItem )
		return;
	
	m_wndSelectedThumbItems.SetActiveThumbItems( m_pActiveCrossetItem->GetThumbItems(), 0 );
	m_wndSelectedThumbItems.LoadImageIndexFromThumbs( m_pCrossetsItem->GetThumbItems(), m_pCrossetsItem->GetImageList() );
	
	if ( !m_pActiveCrossetItem->GetLoadedFlag() )
	{
		//Привязываем items в списке к items в дереве
		NI_ASSERT( m_wndSelectedThumbItems.GetThumbsCount() == m_pActiveCrossetItem->GetChildsCount() );
		CTreeItem::CTreeItemList::const_iterator it;
		int i = 0;
		for ( it=m_pActiveCrossetItem->GetBegin(); it!=m_pActiveCrossetItem->GetEnd(); ++it )
		{
			m_wndSelectedThumbItems.SetUserDataForItem( i, (DWORD) it->GetPtr() );
			i++;
		}
		m_pActiveCrossetItem->SetLoadedFlag( true );
	}
}

BOOL CTileSetFrame::SpecificTranslateMessage( MSG *pMsg )
{
	switch ( pMsg->message )
	{
		case WM_THUMB_LIST_DBLCLK:
			DoubleClickOnThumbList( pMsg->wParam );
			return true;
			
		case WM_THUMB_LIST_DELETE:
			DeleteFrameInTree( pMsg->wParam );
			return true;
	}
	
	return false;
}

LRESULT CTileSetFrame::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	if ( message == WM_THUMB_LIST_SELECT )
	{
		ClickOnThumbList( wParam );
		return true;
	}
	
	return CParentFrame::WindowProc(message, wParam, lParam);
}

void CTileSetFrame::OnImportTerrains() 
{
	std::string szXMLImportFile;
	//Спрашиваем у пользователя имя файла
	if ( !ShowFileDialog( szXMLImportFile, theApp.GetEditorDataDir().c_str(), "Import tileset editor file", TRUE, ".xml", 0, szXMLFilter.c_str() ) )
		return;
	
	CPtr<IDataStream> pXMLStream = OpenFileStream( szXMLImportFile.c_str(), STREAM_ACCESS_READ );
	if ( !pXMLStream )
		return;
	
	CPtr<IDataTree> pDT = CreateDataTreeSaver( pXMLStream, IDataTree::READ );
	if ( !pDT )
		return;
	
	STilesetDesc tileSetDesc;
	CTreeAccessor tree = pDT;
	tree.Add( "tileset", &tileSetDesc );
	
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	NI_ASSERT( pTree != 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	CTreeItem *pTerrainsItem = pRootItem->GetChildItem( E_TILESET_TERRAINS_ITEM );
	NI_ASSERT( pTerrainsItem != 0 );
	pTerrainsItem->RemoveAllChilds();

	IImageProcessor *pIP = GetImageProcessor();
	CPtr<IImage> pSrcImage;
	{
		string szName = szXMLImportFile.substr( 0, szXMLImportFile.rfind('.') );
		szName += ".tga";
		CPtr<IDataStream> pSrcStream = OpenFileStream( szName.c_str(), STREAM_ACCESS_READ );
		NI_ASSERT( pSrcStream != 0 );
		if ( pSrcStream == 0 )
		{
			string szErrorMsg = "Error: Cannot open file: ";
			szErrorMsg += szName;
			AfxMessageBox( szErrorMsg.c_str() );
			return;
		}
		pSrcImage = pIP->LoadImage( pSrcStream );
		if ( pSrcImage == 0 )
		{
			string szErrorMsg = "Error: Cannot load image file: ";
			szErrorMsg += szName;
			AfxMessageBox( szErrorMsg.c_str() );
			return;
		}
	}
	const SColor *pSrc = pSrcImage->GetLFB();
	
	CPtr<IImage> pMaskImage;
	{
		//загружаю картинку маски
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
		pMaskImage = pIP->LoadImage( pMaskStream );
	}
	CPtr<IImage> pTileImage;

	CPtr<IDataStorage> pStorage;
	{
		//Создаю директорию для хранения картинок
		string szPostfix = "terrains\\";
		string szName = GetDirectory( szProjectFileName.c_str() );
		szName += szPostfix;
		pStorage = CreateStorage( szName.c_str(), STREAM_ACCESS_WRITE );

		CTileSetTerrainsItem *pTerrainProps = static_cast<CTileSetTerrainsItem *>( pRootItem->GetChildItem( E_TILESET_TERRAINS_ITEM ) );
		pTerrainProps->SetTerrainsDirName( szPostfix.c_str() );
//		UpdatePropView( pTerrainProps );
	}

	string szFileName;
	string szShortFileName;
	for ( int i=0; i<tileSetDesc.terrtypes.size(); i++ )
	{
		STerrTypeDesc &terrDesc = tileSetDesc.terrtypes[i];
		CTileSetTerrainPropsItem *pTerrainProps = new CTileSetTerrainPropsItem;
		pTerrainProps->SetTerrainName( terrDesc.szName.c_str() );
		pTerrainProps->SetCrossetNumber( terrDesc.nCrosset );
		pTerrainProps->SetMaskPriority( terrDesc.nPriority );
		pTerrainProps->SetPassability( terrDesc.fPassability );

		pTerrainProps->SetPassForInfantry( !(terrDesc.dwAIClasses & AI_CLASS_HUMAN) );
		pTerrainProps->SetPassForWheels( !(terrDesc.dwAIClasses & AI_CLASS_WHEEL) );
		pTerrainProps->SetPassForHalfTracks( !(terrDesc.dwAIClasses & AI_CLASS_HALFTRACK) );
		pTerrainProps->SetPassForTracks( !(terrDesc.dwAIClasses & AI_CLASS_TRACK) );

		pTerrainProps->SetMicrotextureFlag( terrDesc.bMicroTexture );
		pTerrainProps->SetSoundVolume( terrDesc.fSoundVolume );
		pTerrainProps->SetSoundRef( terrDesc.szSound.c_str() );
		pTerrainProps->SetLoopedSoundRef( terrDesc.szLoopedSound.c_str() );
		pTerrainProps->SetBuildFlag( terrDesc.bCanEntrench );
		pTerrainProps->SetTraceFlag( terrDesc.cSoilParams & STerrTypeDesc::ESP_TRACE );
		pTerrainProps->SetDustFlag( terrDesc.cSoilParams & STerrTypeDesc::ESP_DUST );
		pTerrainProps->SetWaterFlag( terrDesc.dwAIClasses & 0x80000000 );
		
		pTerrainsItem->AddChild( pTerrainProps );
		pTerrainProps->ChangeItemName( terrDesc.szName.c_str() );

/*
		//запишем ambient звуки
		CTreeItem *pASounds = pTerrainProps->GetChildItem( E_TILESET_ASOUNDS_ITEM );
		NI_ASSERT( pASounds != 0 );
		for ( int k=0; k<terrDesc.sounds.size(); k++ )
		{
			CTileSetASoundPropsItem *pProps = new CTileSetASoundPropsItem;
			std::string szTemp = terrDesc.sounds[k].szName;
			pProps->SetSoundName( szTemp.c_str() );
			int nPos = szTemp.rfind( '\\' );
			if ( nPos != std::string::npos )
				szTemp = szTemp.substr( nPos + 1 );
			pProps->SetItemName( szTemp.c_str() );
			pProps->SetPeaceFlag( terrDesc.sounds[k].bPeaceful );
			pProps->SetProbability( terrDesc.sounds[k].fProbability );
			pASounds->AddChild( pProps );
		}

		//запишем looped ambient звуки
		CTreeItem *pLSounds = pTerrainProps->GetChildItem( E_TILESET_LSOUNDS_ITEM );
		NI_ASSERT( pLSounds != 0 );
		for ( int k=0; k<terrDesc.loopedSounds.size(); k++ )
		{
			CTileSetLSoundPropsItem *pProps = new CTileSetLSoundPropsItem;
			std::string szTemp = terrDesc.loopedSounds[k].szName;
			pProps->SetSoundName( szTemp.c_str() );
			int nPos = szTemp.rfind( '\\' );
			if ( nPos != std::string::npos )
				szTemp = szTemp.substr( nPos + 1 );
			pProps->SetItemName( szTemp.c_str() );
			pProps->SetPeaceFlag( terrDesc.loopedSounds[k].bPeaceful );
			pLSounds->AddChild( pProps );
		}
*/

		SThumbData thumbData;
		for ( int k=0; k<terrDesc.tiles.size(); k++ )
		{
			CTileSetTilePropsItem *pTileProps = new CTileSetTilePropsItem;
			SMainTileDesc &tileDesc = terrDesc.tiles[k];
			pTileProps->SetProbability( tileDesc.fProbTo - tileDesc.fProbFrom );
			if ( tileDesc.nIndex & 0x01 )
				pTileProps->SetFippedState( CTileSetTilePropsItem::E_FLIPPED );
			else
				pTileProps->SetFippedState( CTileSetTilePropsItem::E_NORMAL );
			int nTileIndex = tileDesc.nIndex / 2;
			pTileProps->nTileIndex = nTileIndex;
			szFileName = NStr::Format( "%.3d.tga", nTileIndex );
			szShortFileName = NStr::Format( "%.3d", nTileIndex );
			pTileProps->SetItemName( szShortFileName.c_str() );

			CTreeItem *pTiles = pTerrainProps->GetChildItem( E_TILESET_TILES_ITEM );
			pTiles->AddChild( pTileProps );

			if ( !(k & 0x01) && k+1 < terrDesc.tiles.size() )
			{
				//проверяю, вдруг это both flipped tile
				SMainTileDesc &nextTileDesc = terrDesc.tiles[k+1];
				if ( nextTileDesc.nIndex == tileDesc.nIndex + 1 )
				{
					pTileProps->SetFippedState( CTileSetTilePropsItem::E_BOTH );
					k++;
				}
			}

			//нарезаю текущий тайл в отдельный файл
			int nSizeX = 64, nSizeY = 32;
			pTileImage = pIP->CreateImage( nSizeX, nSizeY );
			pTileImage->Set( SColor( 255, 0, 0, 0 ) );		//argb, хотя позже сверху пишется инфа из основной картинки
			SColor *pRes = pTileImage->GetLFB();
			
			int nBeginX, nBeginY;
			int nMod7 = nTileIndex % 7;
			if ( nMod7 < 4 )
			{
				// по 4 тайла в срочке
				nBeginX = nMod7 * 64;
				nBeginY = (nTileIndex / 7) * 32;
			}
			else
			{
				// по 3 тайла в строчке
				nBeginX = (nMod7 - 4) * 64 + 32;
				nBeginY = (nTileIndex / 7) * 32 + 16;
			}
			
			for ( int y=nBeginY; y<nBeginY+nSizeY; y++ )
			{
				for ( int x=nBeginX; x<nBeginX+nSizeX; x++ )
				{
					pRes[(y - nBeginY)*nSizeX + x - nBeginX].r = pSrc[y*256 + x].r;
					pRes[(y - nBeginY)*nSizeX + x - nBeginX].g = pSrc[y*256 + x].g;
					pRes[(y - nBeginY)*nSizeX + x - nBeginX].b = pSrc[y*256 + x].b;
					pRes[(y - nBeginY)*nSizeX + x - nBeginX].a = 255;		//альфу устанавливаю всегда максимальной
				}
			}
			RECT rc;
			rc.left = 0;
			rc.top = 0;
			rc.right = pMaskImage->GetSizeX();
			rc.bottom = pMaskImage->GetSizeY();
			pTileImage->ModulateAlphaFrom( pMaskImage, &rc, 0, 0 );
			
			{
				CPtr<IDataStream> pResStream = pStorage->CreateStream( szFileName.c_str(), STREAM_ACCESS_WRITE );
				NI_ASSERT( pResStream != 0 );
				pIP->SaveImageAsTGA( pResStream, pTileImage );
			}
			
			thumbData.szThumbName = szShortFileName;
			CTileSetTilesItem *pTilesItem = static_cast<CTileSetTilesItem *> ( pTerrainProps->GetChildItem( E_TILESET_TILES_ITEM ) );
			pTilesItem->GetThumbItems()->thumbDataList.push_back( thumbData );
		}
	}
	
	SpecificInit();
	InitFreeTerrainIndexes( pRootItem );
	SetChangedFlag( true );
}

void CTileSetFrame::OnUpdateImportTerrains(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	pCmdUI->Enable( pTree != 0 );
}

void CTileSetFrame::OnImportCrossets() 
{
	std::string szXMLImportFile;
	//Спрашиваем у пользователя имя файла
	if ( !ShowFileDialog( szXMLImportFile, theApp.GetEditorDataDir().c_str(), "Import crosset file", TRUE, ".xml", 0, szXMLFilter.c_str() ) )
		return;
	
	CPtr<IDataStream> pXMLStream = OpenFileStream( szXMLImportFile.c_str(), STREAM_ACCESS_READ );
	if ( !pXMLStream )
		return;
	
	CPtr<IDataTree> pDT = CreateDataTreeSaver( pXMLStream, IDataTree::READ );
	if ( !pDT )
		return;
	
	SCrossetDesc crossSetDesc;
	CTreeAccessor tree = pDT;
	tree.Add( "crosset", &crossSetDesc );
	
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	NI_ASSERT( pTree != 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	CTreeItem *pCrossetsItem = pRootItem->GetChildItem( E_CROSSETS_ITEM );
	NI_ASSERT( pCrossetsItem != 0 );
	pCrossetsItem->RemoveAllChilds();

	IImageProcessor *pIP = GetImageProcessor();
	CPtr<IImage> pSrcImage;
	{
		string szName = szXMLImportFile.substr( 0, szXMLImportFile.rfind('.') );
		szName += ".tga";
		CPtr<IDataStream> pSrcStream = OpenFileStream( szName.c_str(), STREAM_ACCESS_READ );
		NI_ASSERT( pSrcStream != 0 );
		if ( pSrcStream == 0 )
		{
			string szErrorMsg = "Error: Cannot open file: ";
			szErrorMsg += szName;
			AfxMessageBox( szErrorMsg.c_str() );
			return;
		}
		pSrcImage = pIP->LoadImage( pSrcStream );
		if ( pSrcImage == 0 )
		{
			string szErrorMsg = "Error: Cannot load image file: ";
			szErrorMsg += szName;
			AfxMessageBox( szErrorMsg.c_str() );
			return;
		}
	}
	const SColor *pSrc = pSrcImage->GetLFB();
	
	CPtr<IImage> pMaskImage;
	{
		//загружаю картинку маски
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
		pMaskImage = pIP->LoadImage( pMaskStream );
	}
	CPtr<IImage> pTileImage;

	CPtr<IDataStorage> pStorage;
	{
		//Создаю директорию для хранения картинок
		string szPostfix = "crossets\\";
		string szName = GetDirectory( szProjectFileName.c_str() );
		szName += szPostfix;
		pStorage = CreateStorage( szName.c_str(), STREAM_ACCESS_WRITE );

		CCrossetsItem *pCrossetItem = static_cast<CCrossetsItem *>( pRootItem->GetChildItem( E_CROSSETS_ITEM ) );
		pCrossetItem->SetCrossetsDirName( szPostfix.c_str() );
	}

	string szFileName;
	string szShortFileName;
	for ( int i=0; i<crossSetDesc.crosses.size(); i++ )
	{
		SCrossDesc &crossDesc = crossSetDesc.crosses[i];
		CCrossetPropsItem *pCrossetProps = new CCrossetPropsItem;
//		pCrossetProps->CreateDefaultChilds();
		pCrossetProps->SetCrossetName( crossDesc.szName.c_str() );
		pCrossetsItem->AddChild( pCrossetProps );
		
		for ( int j=0; j<crossDesc.tiles.size(); j++ )
		{
			SCrossTileTypeDesc &crossTileTypeDesc = crossDesc.tiles[j];
			CCrossetTilesItem *pCrossetTilesItem = 0;
			int t = 0;
			for ( ; t<12; t++ )
			{
				pCrossetTilesItem = static_cast<CCrossetTilesItem *> ( pCrossetProps->GetChildItem( E_CROSSET_TILES_ITEM, t ) );
				if ( crossTileTypeDesc.szName == pCrossetTilesItem->GetItemName() )
					break;
			}

			NI_ASSERT( t != 12 );
			NI_ASSERT( pCrossetTilesItem != 0 );

			SThumbData thumbData;
			for ( int k=0; k<crossTileTypeDesc.tiles.size(); k++ )
			{
				CCrossetTilePropsItem *pTileProps = new CCrossetTilePropsItem;
				SMainTileDesc &tileDesc = crossTileTypeDesc.tiles[k];
				pTileProps->SetProbability( tileDesc.fProbTo - tileDesc.fProbFrom );
				int nTileIndex = tileDesc.nIndex;
				pTileProps->nCrossIndex = nTileIndex;

				szFileName = NStr::Format( "%.3d.tga", nTileIndex );
				szShortFileName = NStr::Format( "%.3d", nTileIndex );
				pTileProps->SetItemName( szShortFileName.c_str() );
				pCrossetTilesItem->AddChild( pTileProps );

				//нарезаю текущий тайл в отдельный файл
				int nSizeX = 64, nSizeY = 32;
				pTileImage = pIP->CreateImage( nSizeX, nSizeY );
				pTileImage->Set( 0 );
				SColor *pRes = pTileImage->GetLFB();
				
				int nBeginX, nBeginY;
				int nMod7 = (nTileIndex / 2) % 7;
				if ( nMod7 < 4 )
				{
					// по 4 тайла в срочке
					nBeginX = nMod7 * 64;
					nBeginY = (nTileIndex / 14) * 32;
				}
				else
				{
					// по 3 тайла в строчке
					nBeginX = (nMod7 - 4) * 64 + 32;
					nBeginY = (nTileIndex / 14) * 32 + 16;
				}
				
				for ( int y=nBeginY; y<nBeginY+nSizeY; y++ )
				{
					for ( int x=nBeginX; x<nBeginX+nSizeX; x++ )
					{
						pRes[(y - nBeginY)*nSizeX + x - nBeginX] = pSrc[y*256 + x];
					}
				}
				RECT rc;
				rc.left = 0;
				rc.top = 0;
				rc.right = pMaskImage->GetSizeX();
				rc.bottom = pMaskImage->GetSizeY();
				pTileImage->ModulateAlphaFrom( pMaskImage, &rc, 0, 0 );
				
				{
					CPtr<IDataStream> pResStream = pStorage->CreateStream( szFileName.c_str(), STREAM_ACCESS_WRITE );
					NI_ASSERT( pResStream != 0 );
					pIP->SaveImageAsTGA( pResStream, pTileImage );
				}
				
				thumbData.szThumbName = szShortFileName;
				pCrossetTilesItem->GetThumbItems()->thumbDataList.push_back( thumbData );
			}
		}

		pCrossetProps->ChangeItemName( crossDesc.szName.c_str() );
	}
	
	SpecificInit();
	InitFreeCrossetIndexes( pRootItem );
	SetChangedFlag( true );
	CrossetsDirChanged();
}

void CTileSetFrame::OnUpdateImportCrossets(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	pCmdUI->Enable( pTree != 0 );
}

void CTileSetFrame::RemoveTerrainIndex( int nIndex )
{
	NI_ASSERT( nIndex != -1 );
	for ( std::list<int>::iterator it=freeTerrainIndexes.begin(); it!=freeTerrainIndexes.end(); ++it )
	{
		if ( nIndex < *it )
		{
			freeTerrainIndexes.insert( it, nIndex );
			return;
		}
	}
}

int CTileSetFrame::GetFreeTerrainIndex()
{
	int nRes = -1;
	if ( freeTerrainIndexes.size() == 1 )
	{
		//возвращаем самый последний индекс
		nRes = freeTerrainIndexes.back()++;
	}
	else
	{
		nRes = freeTerrainIndexes.front();
		freeTerrainIndexes.pop_front();
	}
	
	return nRes;
}

void CTileSetFrame::RemoveCrossetIndex( int nIndex )
{
	NI_ASSERT( nIndex != -1 );
	for ( std::list<int>::iterator it=freeCrossetIndexes.begin(); it!=freeCrossetIndexes.end(); ++it )
	{
		if ( nIndex < *it )
		{
			freeCrossetIndexes.insert( it, nIndex );
			return;
		}
	}
}

int CTileSetFrame::GetFreeCrossetIndex()
{
	int nRes = -1;
	if ( freeCrossetIndexes.size() == 1 )
	{
		//возвращаем самый последний индекс
		nRes = freeCrossetIndexes.back()++;
	}
	else
	{
		nRes = freeCrossetIndexes.front();
		freeCrossetIndexes.pop_front();
	}
	
	return nRes;
}
