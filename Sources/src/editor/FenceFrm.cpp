#include "stdafx.h"
#include "..\Common\StingrayCompat.h"

#include "..\GFX\GFX.h"
#include "..\GFX\GFXHelper.h"
#include "..\Scene\Scene.h"
#include "..\Anim\Animation.h"

#include "common.h"
#include "editor.h"
#include "MainFrm.h"			//для работы с тулбаром
#include "SpriteCompose.h"
#include "PropView.h"
#include "TreeItem.h"
#include "ObjTreeItem.h"
#include "FenceFrm.h"
#include "GameWnd.h"
#include "frames.h"
#include "SetDirDialog.h"

#define ID_ALL_DIR_THUMB_ITEMS  2000
#define ID_SELECTED_THUMB_ITEMS 2001
#define ID_FENCE_TYPE_ICON			2002

static int zeroSizeX = 32;
static int zeroSizeY = 32;
static float zeroShiftX = 15.4f;
static float zeroShiftY = 15.4f;


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static const int THUMB_LIST_WIDTH = 145;

/////////////////////////////////////////////////////////////////////////////
// CFenceFrame

IMPLEMENT_DYNCREATE(CFenceFrame, CGridFrame)

BEGIN_MESSAGE_MAP(CFenceFrame, CGridFrame)
	//{{AFX_MSG_MAP(CFenceFrame)
	ON_WM_CREATE()
	ON_COMMAND(ID_MOVE_OBJECT, OnMoveObject)
	ON_UPDATE_COMMAND_UI(ID_MOVE_OBJECT, OnUpdateMoveObject)
	ON_COMMAND(ID_DRAW_GRID, OnDrawGrid)
	ON_UPDATE_COMMAND_UI(ID_DRAW_GRID, OnUpdateDrawGrid)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_COMMAND(ID_CENTER_FENCE_ABOUT_TILE, OnCenterFenceAboutTile)
	ON_UPDATE_COMMAND_UI(ID_CENTER_FENCE_ABOUT_TILE, OnUpdateCenterFenceAboutTile)
	ON_UPDATE_COMMAND_UI(ID_FENCE_TRANSPARENCE, OnUpdateDrawTransparence)
	ON_CBN_SETFOCUS( IDC_FENCE_TRANSPARENCE, OnSetFocusTranseparence )
	ON_CBN_SELCHANGE( IDC_FENCE_TRANSPARENCE, OnChangeTranseparence )
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFenceFrame construction/destruction

CFenceFrame::CFenceFrame() : m_wndSelectedThumbItems( true )
{
	szComposerName = "Fence Editor";
	szExtension = "*.fnc";
	szComposerSaveName = "Fence_Composer_Project";
	nTreeRootItemID = E_FENCE_ROOT_ITEM;
	nFrameType = CFrameManager::E_FENCE_FRAME;
	pWndView = new CFenceView;
	szAddDir = "fences\\";
	
	m_mode = -1;
	m_pActiveCommonPropsItem = 0;
	m_pActiveInsertItem = 0;
	bEditPassabilityMode = false;
	m_mode = 0;
	tbStyle = E_MOVE_OBJECT;
	m_pFencePropsItem = 0;
	m_pTransparenceCombo = 0;
	m_transValue = 0;

	m_nCompressedFormat = GFXPF_DXT5;
	m_nLowFormat = GFXPF_ARGB4444;
}

CFenceFrame::~CFenceFrame()
{
}

int CFenceFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CGridFrame::OnCreate(lpCreateStruct) == -1)
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
	
	if ( !m_fenceTypeIcon.Create( 0, "Fence Type Icon", WS_CHILD | WS_VISIBLE,
		CRect(0, 0, 0, 0), pWndView, ID_FENCE_TYPE_ICON) )
	{
		TRACE0("Failed to create Fence Type Icon window\n");
		return -1;
	}
	string szInvalidFileName = "editor\\invalid.tga";
	m_fenceTypeIcon.LoadBitmap( szInvalidFileName.c_str(), "" );
	
	//	m_wndSelectedThumbItems.TestInsertSomeItems();
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// CFenceFrame message handlers

CMenu *GetMenuIndexByID( CMenu *pMenu, int nID )
{
	int nNum = pMenu->GetMenuItemCount();
	for ( int i=0; i<nNum; i++ )
	{
		if ( pMenu->GetMenuItemID( i ) == nID )
			return pMenu;
	}
	
	return 0;
}

void CFenceFrame::ShowFrameWindows( int nCommand )
{
	CParentFrame::ShowFrameWindows( nCommand );
	if ( bEditPassabilityMode )
		g_frameManager.GetGameWnd()->ShowWindow( nCommand );
	else
		g_frameManager.GetGameWnd()->ShowWindow( SW_HIDE );
	
	if ( nCommand == SW_SHOW )
	{
		ICamera *pCamera = GetSingleton<ICamera>();
		pCamera->SetAnchor( CVec3(16*fWorldCellSize, 16*fWorldCellSize, 0) );
		pCamera->Update();
		
		IGFX *pGFX = GetSingleton<IGFX>();
		pGFX->SetViewTransform( pCamera->GetPlacement() );
		
		IScene *pSG = GetSingleton<IScene>();
		if ( pSprite )
			pSG->AddObject( pSprite, SGVOGT_UNIT );
	}
	
/*
	//TEST
//	CWnd *pMain = AfxGetMainWnd();
//	CMenu *pMenu = pMain->GetMenu();
	CMenu *pMenu = GetMenu();
	pMenu = pMenu->GetSubMenu( 0 );
	CMenu *pRecent = GetMenuIndexByID( pMenu, ID_FILE_RECENT_FILES );
	NI_ASSERT( pRecent != 0 );
	for ( int i=0; i<10; i++ )
	{
		pRecent->AppendMenu( MF_STRING, i+100, NStr::Format( "%d", i ) );
	}
*/
}

void CFenceFrame::GFXDraw()
{
	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER, m_backgroundColor );
	pGFX->BeginScene();
	
	GetSingleton<IGameTimer>()->Update( timeGetTime() );
	pGFX->SetShadingEffect( 2 );
	pGFX->SetTexture( 0, 0 );
	CGridFrame::GFXDraw();
	if ( m_pFencePropsItem )
	{
		if ( tbStyle != E_DRAW_TRANSEPARENCE )
		{
			for ( CListOfTiles::iterator it=m_pFencePropsItem->lockedTiles.begin(); it!=m_pFencePropsItem->lockedTiles.end(); ++it )
				pGFX->Draw( it->pVertices, pMarkerIndices );
		}
		else
		{
			for ( CListOfTiles::iterator it=m_pFencePropsItem->transeparences.begin(); it!=m_pFencePropsItem->transeparences.end(); ++it )
				pGFX->Draw( it->pVertices, pMarkerIndices );
		}
	}
	
	ICamera *pCamera = GetSingleton<ICamera>();
	IScene *pSG = GetSingleton<IScene>();
	pCamera->Update();
  pSG->Draw( pCamera );
	
	pGFX->EndScene();
	pGFX->Flip();
}

void CFenceFrame::ViewSizeChanged()
{
	CRect viewRect;
	pWndView->GetClientRect( viewRect );

	RECT rc;
	rc.left = viewRect.left;
	rc.top = viewRect.bottom - THUMB_LIST_WIDTH;
	rc.right = viewRect.left + THUMBNAIL_WIDTH;
	rc.bottom = viewRect.bottom;
	if ( m_fenceTypeIcon.GetSafeHwnd() )
		m_fenceTypeIcon.MoveWindow( &rc );

	rc.left = viewRect.left + THUMBNAIL_WIDTH;
	rc.right = viewRect.right;
	if ( m_wndSelectedThumbItems.GetSafeHwnd() )
		m_wndSelectedThumbItems.MoveWindow( &rc );

	rc.left = viewRect.left;
	rc.bottom = rc.top;
	rc.top = viewRect.top;
	if ( m_wndAllDirThumbItems.GetSafeHwnd() )
		m_wndAllDirThumbItems.MoveWindow( &rc );
}

void CFenceFrame::ClickOnThumbList( int nID )
{
	if ( nID == ID_ALL_DIR_THUMB_ITEMS )
	{
		//Выделяем в дереве текущую директорию с анимациями
		if ( m_pActiveInsertItem )
			m_pActiveInsertItem->SelectMeInTheTree();
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

void CFenceFrame::DoubleClickOnThumbList( int nID )
{
	if ( nID == ID_ALL_DIR_THUMB_ITEMS )
	{
		//Добавляем новый элемент в текущую диру дерева заборов и в список накиданных тайлов
		if ( !m_pActiveInsertItem )
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
//		int nNewItemIndex = m_wndSelectedThumbItems.InsertItemAfterSelection( szFileName, m_pActiveInsertItem->GetDirName() );
		NI_ASSERT( nNewItemIndex != -1 );
		
		//Добавляем sprite в дерево в m_pActiveInsertItem
		CFencePropsItem *pFenceProps = new CFencePropsItem();
		pFenceProps->SetItemName( szItemName.c_str() );
		pFenceProps->nSegmentIndex = GetFreeFenceIndex();
		m_pActiveInsertItem->AddChild( pFenceProps );
		m_wndSelectedThumbItems.SetUserDataForItem( nNewItemIndex, (long) pFenceProps );
	}
}

void CFenceFrame::SelectItemInSelectedThumbList( DWORD dwData )
{
	int nIndex = m_wndSelectedThumbItems.GetItemIndexWithUserData( dwData );
	m_wndSelectedThumbItems.SelectItem( nIndex );
}

void CFenceFrame::DeleteFrameInSelectedList( DWORD dwData )
{
	SetChangedFlag( true );
	ClearPropView();
	int nIndex = m_wndSelectedThumbItems.GetItemIndexWithUserData( dwData );
	m_wndSelectedThumbItems.DeleteItem( nIndex );
}

void CFenceFrame::DeleteFrameInTree( int nID )
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
	CFencePropsItem *pFrame = (CFencePropsItem *) dwData;
	NI_ASSERT( pFrame->GetItemType() == E_FENCE_PROPS_ITEM );
	RemoveFenceIndex( pFrame->nSegmentIndex );
	pFrame->DeleteMeInParentTreeItem();

	//Выделяем следующий элемент в списке
	m_wndSelectedThumbItems.SelectItem( nSel + 1 );
	
	//Удаляем элемент в списке
	m_wndSelectedThumbItems.DeleteItem( nSel );
}

void CFenceFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();
	m_mode = -1;

	InitActiveCommonPropsItem();
}

void CFenceFrame::SpecificClearBeforeBatchMode()
{
	freeIndexes.clear();
	freeIndexes.push_back( 0 );

	m_wndAllDirThumbItems.SetActiveThumbItems( 0, 0 );
	m_wndSelectedThumbItems.SetActiveThumbItems( 0, 0 );
	m_pActiveInsertItem = 0;
	m_pActiveCommonPropsItem = 0;
	m_pFencePropsItem = 0;
	
	if ( pSprite )
	{
		IScene *pSG = GetSingleton<IScene>();
		pSG->RemoveObject( pSprite );
		pSprite = 0;
	}
	SwitchToEditMode( false );
}

void CFenceFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	ASSERT( pDT->IsReading() );
	int nIndex = 0;
	std::set<int> indexSet;
	for ( int nCurrentFenceDirection=0; nCurrentFenceDirection<4; nCurrentFenceDirection++ )
	{
		CTreeItem *pFenceDirectionItem = pRootItem->GetChildItem( E_FENCE_DIRECTION_ITEM, nCurrentFenceDirection );
		for ( int nCurrentFenceType=0; nCurrentFenceType<4; nCurrentFenceType++ )
		{
			CTreeItem *pFenceInsertItem = pFenceDirectionItem->GetChildItem( E_FENCE_INSERT_ITEM, nCurrentFenceType );
			for ( CTreeItem::CTreeItemList::const_iterator it=pFenceInsertItem->GetBegin(); it!=pFenceInsertItem->GetEnd(); ++it )
			{
				CFencePropsItem *pFenceProps = (CFencePropsItem *) it->GetPtr();
				if ( pFenceProps->nSegmentIndex == -1 )
					pFenceProps->nSegmentIndex = nIndex;
				indexSet.insert( pFenceProps->nSegmentIndex );
				nIndex++;
			}
		}
	}

	freeIndexes.clear();
	int nPrev = -1;
	for ( std::set<int>::iterator it=indexSet.begin(); it!=indexSet.end(); ++it )
	{
		if ( *it != nPrev + 1 )				//если есть пустые индексы
		{
			for ( int i=nPrev+1; i!=*it; i++ )
				freeIndexes.push_back( i );
		}
		nPrev = *it;
	}
	freeIndexes.push_back( nPrev + 1 );			//это самый последний индекс
	//теперь freeIndexes должны быть отсортированы по возрастанию
}

void CFenceFrame::RemoveFenceIndex( int nIndex )
{
	NI_ASSERT( nIndex != -1 );
	for ( std::list<int>::iterator it=freeIndexes.begin(); it!=freeIndexes.end(); ++it )
	{
		if ( nIndex < *it )
		{
			freeIndexes.insert( it, nIndex );
			return;
		}
	}
}

int CFenceFrame::GetFreeFenceIndex()
{
	int nRes = -1;
	if ( freeIndexes.size() == 1 )
	{
		//возвращаем самый последний индекс
		nRes = freeIndexes.back()++;
	}
	else
	{
		nRes = freeIndexes.front();
		freeIndexes.pop_front();
	}
	
	return nRes;
}

int CFenceFrame::GetMaxFenceIndex()
{
	return freeIndexes.back();
}

bool CFenceFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	if ( freeIndexes.size() > 1 )
	{
		AfxMessageBox( "Error: You have deleted some fence items.\nThe resulting animations will include holes but they are not supported.\n"
			"You need to add some fence items before continue export files." );
		return false;
	}
	
	NI_ASSERT( pRootItem->GetItemType() == E_FENCE_ROOT_ITEM );
	SFenceRPGStats rpgStats;
	CFenceTreeRootItem *pFenceRoot = (CFenceTreeRootItem *) pRootItem;
	CFenceCommonPropsItem *pCommonProps = static_cast<CFenceCommonPropsItem *>( pRootItem->GetChildItem( E_FENCE_COMMON_PROPS_ITEM ) );
	rpgStats.szKeyName = pCommonProps->GetFenceName();
	rpgStats.fMaxHP = pCommonProps->GetFenceHealth();
	for ( int i=0; i<6; i++ )
	{
		rpgStats.defences[ i ].nArmorMin = pCommonProps->GetFenceAbsorbtion();
		rpgStats.defences[ i ].nArmorMax = pCommonProps->GetFenceAbsorbtion();
		rpgStats.defences[ i ].fSilhouette = 0;
	}
	
	if ( pCommonProps->GetPassForInfantry() )
		rpgStats.dwAIClasses |= AI_CLASS_HUMAN;
	if ( pCommonProps->GetPassForWheels() )
		rpgStats.dwAIClasses |= AI_CLASS_WHEEL;
	if ( pCommonProps->GetPassForHalfTracks() )
		rpgStats.dwAIClasses |= AI_CLASS_HALFTRACK;
	if ( pCommonProps->GetPassForTracks() )
		rpgStats.dwAIClasses |= AI_CLASS_TRACK;

	//запишем эффекты
	CObjectEffectsItem *pEffects = static_cast<CObjectEffectsItem *> ( pRootItem->GetChildItem( E_OBJECT_EFFECTS_ITEM ) );
	rpgStats.szEffectExplosion = pEffects->GetEffectExplosion();
	rpgStats.szEffectDeath = pEffects->GetEffectDeath();

	pFenceRoot->ComposeFences( pszProjectName, GetDirectory(pszResultFileName).c_str(), rpgStats );
	
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
	
	//создадим файл icon.tga с изображением забора
	CTreeItem *pFenceDirectionItem = pRootItem->GetChildItem( E_FENCE_DIRECTION_ITEM, 0 );
	CTreeItem *pFenceInsertItem = pFenceDirectionItem->GetChildItem( E_FENCE_INSERT_ITEM, 0 );
	CFencePropsItem *pFenceProps = static_cast<CFencePropsItem *>( pFenceInsertItem->GetChildItem( E_FENCE_PROPS_ITEM ) );
	NI_ASSERT( pFenceProps != 0 );

	std::string szShortName = pCommonProps->GetDirName();
	szShortName += pFenceProps->GetItemName();
	szShortName += ".tga";
	std::string szFullName;
	MakeFullPath( GetDirectory(pszProjectName).c_str(), szShortName.c_str(), szFullName );
		
	std::string szResName = GetDirectory( pszResultFileName );
	szResName += "icon.tga";
	SaveIconFile( szFullName.c_str(), szResName.c_str() );

	return true;
}
	
void CFenceFrame::ActiveDirNameChanged()
{
	ASSERT( m_pActiveCommonPropsItem != 0 );
	SetChangedFlag( true );

	if ( m_pActiveCommonPropsItem )
	{
		//так как директория задается относительно, здесь я должен собрать полный путь
		string szDir = GetDirectory( szProjectFileName.c_str() );
		string szFull;
		bool bRes = MakeFullPath( szDir.c_str(), m_pActiveCommonPropsItem->GetDirName(), szFull );

		if ( bRes )
			m_wndAllDirThumbItems.LoadAllImagesFromDir( m_pActiveCommonPropsItem->GetThumbItems(), m_pActiveCommonPropsItem->GetImageList(), szFull.c_str() );
		else
			m_wndAllDirThumbItems.LoadAllImagesFromDir( m_pActiveCommonPropsItem->GetThumbItems(), m_pActiveCommonPropsItem->GetImageList(), m_pActiveCommonPropsItem->GetDirName() );

		m_wndAllDirThumbItems.SetActiveThumbItems( m_pActiveCommonPropsItem->GetThumbItems(), m_pActiveCommonPropsItem->GetImageList() );
		m_wndSelectedThumbItems.LoadImageIndexFromThumbs( m_pActiveCommonPropsItem->GetThumbItems(), m_pActiveCommonPropsItem->GetImageList() );
	}
}

void CFenceFrame::InitActiveCommonPropsItem()
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	NI_ASSERT( pTree != 0 );
	
	CTreeItem *pRootItem = pTree->GetRootItem();
	NI_ASSERT( pRootItem != 0 );
	m_pActiveCommonPropsItem = (CFenceCommonPropsItem *) ( pRootItem->GetChildItem( E_FENCE_COMMON_PROPS_ITEM ) );
	ASSERT( m_pActiveCommonPropsItem != 0 );

	//Сперва загружаем невалидную иконку, она всегда будет под индексом 0
	string szEditorDataDir = theApp.GetEditorDataDir();
	szEditorDataDir += "editor\\";
	
	m_wndAllDirThumbItems.LoadImageToImageList( m_pActiveCommonPropsItem->GetImageList(), "invalid.tga", szEditorDataDir.c_str() );
/*
m_wndAllDirThumbItems.LoadAllImagesFromDir( m_pActiveCommonPropsItem->GetThumbItems(), m_pActiveCommonPropsItem->GetImageList(), m_pActiveCommonPropsItem->GetDirName() );
	
	m_wndAllDirThumbItems.SetActiveThumbItems( m_pActiveCommonPropsItem->GetThumbItems(), m_pActiveCommonPropsItem->GetImageList() );
	m_wndSelectedThumbItems.LoadImageIndexFromThumbs( m_pActiveCommonPropsItem->GetThumbItems(), m_pActiveCommonPropsItem->GetImageList() );
*/
	ActiveDirNameChanged();
	SetChangedFlag( false );
}

void CFenceFrame::SetActiveFenceInsertItem( CFenceInsertItem *pFenceItem )
{
	SwitchToEditMode( false );
	if ( pFenceItem == m_pActiveInsertItem )
		return;

	m_pActiveInsertItem = pFenceItem;
	if ( !m_pActiveInsertItem )
		return;

	m_wndSelectedThumbItems.SetActiveThumbItems( m_pActiveInsertItem->GetThumbItems(), 0 );
	m_wndSelectedThumbItems.LoadImageIndexFromThumbs( m_pActiveCommonPropsItem->GetThumbItems(), m_pActiveCommonPropsItem->GetImageList() );

	if ( !m_pActiveInsertItem->GetLoadedFlag() )
	{
		//Привязываем items в списке к items в дереве
		NI_ASSERT( m_wndSelectedThumbItems.GetThumbsCount() == m_pActiveInsertItem->GetChildsCount() );
		CTreeItem::CTreeItemList::const_iterator it;
		int i = 0;
		for ( it=m_pActiveInsertItem->GetBegin(); it!=m_pActiveInsertItem->GetEnd(); ++it )
		{
			m_wndSelectedThumbItems.SetUserDataForItem( i, (DWORD) it->GetPtr() );
			i++;
		}
		m_pActiveInsertItem->SetLoadedFlag( true );
	}
	
	//Определяем, что это за InsertItem. Для разных типов Insert Item существуют разные вспомогательные иконки.
	//Их три вида - Blocks, Gates, Ends
	/*
	CTreeItem *pPapa = m_pActiveInsertItem->GetParentTreeItem();
	NI_ASSERT( pPapa != 0 );
	int nType = pPapa->GetItemType();
	if ( nType == E_FENCE_END_PARTS_ITEM )
	{
		pPapa = m_pActiveInsertItem->GetParentTreeItem();
		NI_ASSERT( pPapa != 0 );
		nType = pPapa->GetItemType();
		}
		
			string szInvalidFileName = theApp.GetEditorDataDir();
			szInvalidFileName += "editor\\invalid.tga";
			
				string szFullIconName = theApp.GetEditorDataDir();
				szFullIconName += "editor\\Terrain\\";
				switch ( nType )
				{
				case E_FENCE_BLOCKS_ITEM:
				szFullIconName += "block.tga";
				break;
				case E_FENCE_GATES_ITEM:
				szFullIconName += "gate.tga";
				break;
				case E_FENCE_ENDS_ITEM:
				szFullIconName += "end.tga";
				break;
				}
				
					m_fenceTypeIcon.LoadBitmap( szFullIconName.c_str(), szInvalidFileName.c_str() );
					m_fenceTypeIcon.Invalidate();
	*/
}

BOOL CFenceFrame::SpecificTranslateMessage( MSG *pMsg )
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

LRESULT CFenceFrame::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	if ( message == WM_THUMB_LIST_SELECT )
	{
		ClickOnThumbList( wParam );
		return true;
	}
	
	return CGridFrame::WindowProc(message, wParam, lParam);
}

void CFenceFrame::OnUpdateDrawTransparence(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
		pCmdUI->Enable( true );
	else
		pCmdUI->Enable( false );
}

void CFenceFrame::SwitchToEditMode( bool bFlag )
{
	if ( bFlag == bEditPassabilityMode )
		return;

	bEditPassabilityMode = bFlag;
	if ( bEditPassabilityMode )
	{
		//скрываем thumb окошки, отображаем game window
		m_wndAllDirThumbItems.ShowWindow( SW_HIDE );
		m_wndSelectedThumbItems.ShowWindow( SW_HIDE );
		m_fenceTypeIcon.ShowWindow( SW_HIDE );
		g_frameManager.GetGameWnd()->ShowWindow( SW_SHOW );
	}
	else
	{
		//скрываем game window, показываем thumb окошки
		m_wndAllDirThumbItems.ShowWindow( SW_SHOW );
		m_wndSelectedThumbItems.ShowWindow( SW_SHOW );
		m_fenceTypeIcon.ShowWindow( SW_SHOW );
		g_frameManager.GetGameWnd()->ShowWindow( SW_HIDE );
	}
}

void CFenceFrame::EditFence( CFencePropsItem *pFencePropsItem )
{
	SwitchToEditMode( true );

	//получим имя .tga файла
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRoot = pTree->GetRootItem();
	CFenceCommonPropsItem *pCommonProps = static_cast<CFenceCommonPropsItem *>( pRoot->GetChildItem( E_FENCE_COMMON_PROPS_ITEM ) );
	string szName = pCommonProps->GetDirName();
	//сконвертируем к полному пути
	if ( IsRelatedPath( szName.c_str() ) )
	{
		string szFullName;
		MakeFullPath( GetDirectory( szProjectFileName.c_str() ).c_str(), szName.c_str(), szFullName );
		szName = szFullName;
	}
	szName += pFencePropsItem->GetItemName();
	szName += ".tga";

	//Скомпонуем спрайт в editor temp dir
	string szTempDir = theApp.GetEditorTempDir();
	if ( !ComposeSingleSprite( szName.c_str(), szTempDir.c_str(), "1", true ) )
		return;

	szTempDir = theApp.GetEditorTempResourceDir();
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	IScene *pSG = GetSingleton<IScene>();
	if ( pSprite )
		pSG->RemoveObject( pSprite );
	pSG->Clear();

	pSprite = static_cast<IObjVisObj *> ( pVOB->BuildObject( (szTempDir + "\\1").c_str(), 0, SGVOT_SPRITE ) );
	NI_ASSERT( pSprite != 0 );
	pSprite->SetPosition( pFencePropsItem->vSpritePos );
	pSprite->SetDirection( 0 );
	pSprite->SetAnimation( 0 );
	pSG->AddObject( pSprite, SGVOGT_UNIT );
	pSprite->SetOpacity( 140 );

	m_pFencePropsItem = pFencePropsItem;

	if ( !pFencePropsItem->bLoaded )
	{
		CenterSpriteAboutTile();			//если был создан новый проект, то автоматом центрирую тайлы
		pFencePropsItem->bLoaded = true;
	}
	
	GFXDraw();
}

void CFenceFrame::CenterSpriteAboutTile()
{
	NI_ASSERT( pSprite != 0 );
	NI_ASSERT( m_pFencePropsItem != 0 );
	IScene *pSG = GetSingleton<IScene>();
	CVec3 currentPos3 = pSprite->GetPosition();
	//получим тайловые координаты
	CVec2 currentPos2;
	pSG->GetPos2( &currentPos2, currentPos3 );
	POINT pt;
	pt.x = currentPos2.x;
	pt.y = currentPos2.y;

	float ftX, ftY;			//тайловые координаты в моей координатной системе
	ComputeGameTileCoordinates( pt, ftX, ftY );
	float fX1, fY1, fX2, fY2, fX3, fY3, fX4, fY4;
	GetGameTileCoordinates( ftX, ftY, fX1, fY1, fX2, fY2, fX3, fY3, fX4, fY4 );
	currentPos2.x = ( fX2 + fX4 ) / 2;
	currentPos2.y = ( fY1 + fY3 ) / 2;
	pSG->GetPos3( &currentPos3, currentPos2 );
	pSprite->SetPosition( currentPos3 );
	m_pFencePropsItem->vSpritePos = currentPos3;
	SetChangedFlag( true );
	
	GFXDraw();
}

void CFenceFrame::OnDrawGrid() 
{
	UINT nIndex = 0;
	tbStyle = E_DRAW_GRID;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+9);
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_MOVE_OBJECT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetFocus();
	GFXDraw();
}

void CFenceFrame::OnChangeTranseparence()
{
	NI_ASSERT( m_pTransparenceCombo != 0 );
 	m_transValue = m_pTransparenceCombo->GetCurSel();
}

void CFenceFrame::OnSetFocusTranseparence()
{
	UINT nIndex = 0;
	tbStyle = E_DRAW_TRANSEPARENCE;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+9);
	nIndex = pToolBar->CommandToIndex( ID_MOVE_OBJECT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	m_pTransparenceCombo->SetFocus();
	
	GFXDraw();
}

void CFenceFrame::OnMoveObject() 
{
	UINT nIndex = 0;
	tbStyle = E_MOVE_OBJECT;
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+9);
	nIndex = pToolBar->CommandToIndex( ID_MOVE_OBJECT );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	nIndex = pToolBar->CommandToIndex( ID_DRAW_GRID );
	pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
	
	SetFocus();
	GFXDraw();
}

void CFenceFrame::OnCenterFenceAboutTile() 
{
	CenterSpriteAboutTile();
}

void CFenceFrame::OnUpdateMoveObject(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 && bEditPassabilityMode )
	{
		//Если уже был создан проект и редактируем проходимость
		pCmdUI->Enable( true );
	}
	else
		pCmdUI->Enable( false );
}

void CFenceFrame::OnUpdateDrawGrid(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 && bEditPassabilityMode )
	{
		//Если уже был создан проект и редактируем проходимость
		pCmdUI->Enable( true );
	}
	else
		pCmdUI->Enable( false );
}

void CFenceFrame::OnUpdateCenterFenceAboutTile(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 && bEditPassabilityMode )
	{
		//Если уже был создан проект и редактируем проходимость
		pCmdUI->Enable( true );
	}
	else
		pCmdUI->Enable( false );
}

void CFenceFrame::OnLButtonDown(UINT nFlags, CPoint point) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 )			//Если проект не был создан
		return;
	if ( !bEditPassabilityMode )
		return;

	if ( tbStyle == E_DRAW_GRID )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( m_pFencePropsItem->lockedTiles, ftX, ftY, 1, E_LOCKED_TILE );
		GFXDraw();
	}

	else if ( tbStyle == E_DRAW_TRANSEPARENCE )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( m_pFencePropsItem->transeparences, ftX, ftY, m_transValue, E_TRANSEPARENCE_TILE );
		GFXDraw();
	}

	else if ( tbStyle == E_MOVE_OBJECT && pSprite )
	{
		CVec2 pt;
		pt.x = point.x;
		pt.y = point.y;
		if ( IsSpriteHit( pSprite, pt, &objShift ) )
		{
			SetChangedFlag( true );
			m_mode = E_MOVE_OBJECT;
			g_frameManager.GetGameWnd()->SetCapture();
		}
	}
	
	CGridFrame::OnLButtonDown(nFlags, point);
}

void CFenceFrame::OnLButtonUp(UINT nFlags, CPoint point) 
{
	ReleaseCapture();
	m_mode = -1;
	GFXDraw();
	
	CGridFrame::OnLButtonUp(nFlags, point);
}

void CFenceFrame::OnRButtonDown(UINT nFlags, CPoint point) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 )			//Если проект не был создан
		return;
	SetChangedFlag( true );
	
	if ( tbStyle == E_DRAW_GRID )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( m_pFencePropsItem->lockedTiles, ftX, ftY, 0, E_LOCKED_TILE );
		GFXDraw();
	}
	
	else if ( tbStyle == E_DRAW_TRANSEPARENCE )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( m_pFencePropsItem->transeparences, ftX, ftY, 0, E_TRANSEPARENCE_TILE );
		GFXDraw();
	}
	
	CGridFrame::OnRButtonDown(nFlags, point);
}

void CFenceFrame::OnMouseMove(UINT nFlags, CPoint point) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 )			//Если проект не был создан
		return;
	
	if ( tbStyle == E_DRAW_GRID && nFlags & MK_RBUTTON )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( m_pFencePropsItem->lockedTiles, ftX, ftY, 0, E_LOCKED_TILE );
		GFXDraw();
	}
	else if ( tbStyle == E_DRAW_GRID && nFlags & MK_LBUTTON )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( m_pFencePropsItem->lockedTiles, ftX, ftY, 1, E_LOCKED_TILE );
		GFXDraw();
	}
	else if ( tbStyle == E_DRAW_TRANSEPARENCE && nFlags & MK_RBUTTON )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( m_pFencePropsItem->transeparences, ftX, ftY, 0, E_TRANSEPARENCE_TILE );
		GFXDraw();
	}
	else if ( tbStyle == E_DRAW_TRANSEPARENCE && nFlags & MK_LBUTTON )
	{
		SetChangedFlag( true );
		float ftX, ftY;			//tile X and tile Y coords
		CGridFrame::ComputeGameTileCoordinates( point, ftX, ftY );
		SetTileInListOfTiles( m_pFencePropsItem->transeparences, ftX, ftY, m_transValue, E_TRANSEPARENCE_TILE );
		GFXDraw();
	}
	else if ( m_mode == E_MOVE_OBJECT && nFlags & MK_LBUTTON )
	{
		if ( !pSprite )
			return;
		
		IScene *pSG = GetSingleton<IScene>();
		CVec2 pos2;
		pos2.x = point.x + objShift.x;
		pos2.y = point.y + objShift.y;
		pSG->GetPos3( &m_pFencePropsItem->vSpritePos, pos2 );
		pSG->MoveObject( pSprite, m_pFencePropsItem->vSpritePos );

		GFXDraw();
	}
	
	CGridFrame::OnMouseMove(nFlags, point);
}

void CFenceFrame::SaveMyData( CFencePropsItem *pFrameProps, CTreeAccessor saver )
{
	saver.Add( "LockedTiles", &pFrameProps->lockedTiles );
	saver.Add( "Transparences", &pFrameProps->transeparences );
}

void CFenceFrame::LoadMyData( CFencePropsItem *pFrameProps, CTreeAccessor saver )
{
	saver.Add( "LockedTiles", &pFrameProps->lockedTiles );
	saver.Add( "Transparences", &pFrameProps->transeparences );

	for ( CListOfTiles::iterator it=pFrameProps->lockedTiles.begin(); it!=pFrameProps->lockedTiles.end(); ++it )
	{
		SetTileInListOfTiles( pFrameProps->lockedTiles, it->nTileX, it->nTileY, it->nVal, E_LOCKED_TILE );
	}
	
	for ( CListOfTiles::iterator it=pFrameProps->transeparences.begin(); it!=pFrameProps->transeparences.end(); ++it )
	{
		SetTileInListOfTiles( pFrameProps->transeparences, it->nTileX, it->nTileY, it->nVal, E_TRANSEPARENCE_TILE );
	}
}

void CFenceFrame::FillSegmentProps( CFencePropsItem *pFrameProps, SFenceRPGStats::SSegmentRPGStats &segment )
{
	segment.nIndex = pFrameProps->nSegmentIndex;
	IScene *pSG = GetSingleton<IScene>();

	//так как спрайт загружен уже с нулем в центре картинки, то не надо делать пересчет координат
	
	// Сохраняем данные о тайловой проходимости
	if ( pFrameProps->lockedTiles.empty() )
	{
		segment.passability.SetSizes( 0, 0 );
		segment.vOrigin.x = 0;
		segment.vOrigin.y = 0;
	}
	else
	{
		//Сперва найдем минимальные и максимальные координаты тайлов в pFrameProps->lockedTiles
		int nTileMinX = pFrameProps->lockedTiles.front().nTileX, nTileMaxX = pFrameProps->lockedTiles.front().nTileX;
		int nTileMinY = pFrameProps->lockedTiles.front().nTileY, nTileMaxY = pFrameProps->lockedTiles.front().nTileY;
		CListOfTiles::iterator it=pFrameProps->lockedTiles.begin();
		for ( ++it; it!=pFrameProps->lockedTiles.end(); ++it )
		{
			if ( nTileMinX > it->nTileX )
				nTileMinX = it->nTileX;
			else if ( nTileMaxX < it->nTileX )
				nTileMaxX = it->nTileX;
			
			if ( nTileMinY > it->nTileY )
				nTileMinY = it->nTileY;
			else if ( nTileMaxY < it->nTileY )
				nTileMaxY = it->nTileY;
		}

		segment.passability.SetSizes( nTileMaxX-nTileMinX+1, nTileMaxY-nTileMinY+1 );
		BYTE *pBuf = segment.passability.GetBuffer();
		for ( int y=0; y<nTileMaxY-nTileMinY+1; y++ )
		{
			for ( int x=0; x<nTileMaxX-nTileMinX+1; x++ )
			{
				for ( it=pFrameProps->lockedTiles.begin(); it!=pFrameProps->lockedTiles.end(); ++it )
				{
					if ( (x == it->nTileX - nTileMinX) && (y == it->nTileY - nTileMinY) )
						break;
				}
				
				if ( it != pFrameProps->lockedTiles.end() )
					pBuf[ x + y*(nTileMaxX - nTileMinX + 1) ] = it->nVal;
				else
					pBuf[ x + y*(nTileMaxX - nTileMinX + 1) ] = 0;
			}
		}

		float fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4;
		CGridFrame::GetGameTileCoordinates( nTileMinX, nTileMinY, fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4 );
		CVec2 mostLeft2(fx2, fy2);
		CVec3 mostLeft3(0, 0, 0);
		pSG->GetPos3( &mostLeft3, mostLeft2 );

		segment.vOrigin.x = pFrameProps->vSpritePos.x - mostLeft3.x;
		segment.vOrigin.y = pFrameProps->vSpritePos.y - mostLeft3.y;

		GFXDraw();
	}

	// Сохраняем данные о прозрачности объекта
	{
		if ( pFrameProps->transeparences.empty() )
		{
			segment.visibility.SetSizes( 0, 0 );
			segment.vVisOrigin.x = 0;
			segment.vVisOrigin.y = 0;
		}
		else
		{
			//Сперва найдем минимальные и максимальные координаты тайлов в pFrameProps->transeparences
			int nTileMinX = pFrameProps->transeparences.front().nTileX, nTileMaxX = pFrameProps->transeparences.front().nTileX;
			int nTileMinY = pFrameProps->transeparences.front().nTileY, nTileMaxY = pFrameProps->transeparences.front().nTileY;
			
			CListOfTiles::iterator it = pFrameProps->transeparences.begin();
			++it;
			for ( ; it!=pFrameProps->transeparences.end(); ++it )
			{
				int nTileX = it->nTileX;
				int nTileY = it->nTileY;
				
				if ( nTileMinX > nTileX )
					nTileMinX = nTileX;
				else if ( nTileMaxX < nTileX )
					nTileMaxX = nTileX;
				
				if ( nTileMinY > nTileY )
					nTileMinY = nTileY;
				else if ( nTileMaxY < nTileY )
					nTileMaxY = nTileY;
			}
			
			segment.visibility.SetSizes( nTileMaxX-nTileMinX+1, nTileMaxY-nTileMinY+1 );
			BYTE *pBuf = segment.visibility.GetBuffer();
			for ( int y=0; y<nTileMaxY-nTileMinY+1; y++ )
			{
				for ( int x=0; x<nTileMaxX-nTileMinX+1; x++ )
				{
					for ( it=pFrameProps->transeparences.begin(); it!=pFrameProps->transeparences.end(); ++it )
					{
						if ( (x == it->nTileX - nTileMinX) && (y == it->nTileY - nTileMinY) )
							break;
					}
					
					if ( it != pFrameProps->transeparences.end() )
						pBuf[ x + y*(nTileMaxX - nTileMinX + 1) ] = it->nVal;
					else
						pBuf[ x + y*(nTileMaxX - nTileMinX + 1) ] = 0;
				}
			}
			
			float fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4;
			CGridFrame::GetGameTileCoordinates( nTileMinX, nTileMinY, fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4 );
			CVec2 mostLeft2(fx2, fy2);
			CVec3 mostLeft3(0, 0, 0);
			pSG->GetPos3( &mostLeft3, mostLeft2 );
			
			segment.vVisOrigin.x = pFrameProps->vSpritePos.x - mostLeft3.x;
			segment.vVisOrigin.y = pFrameProps->vSpritePos.y - mostLeft3.y;
		}
	}
}
