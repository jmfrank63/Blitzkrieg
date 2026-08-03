#include "StdAfx.h"

#include "../GFX/GFX.h"
#include "../Scene/Scene.h"
#include "../Anim/Animation.h"

#include "editor.h"
#include "PropView.h"
#include "TreeItem.h"
#include "TreeDockWnd.h"
#include "SpriteFrm.h"
#include "GameWnd.h"
#include "frames.h"

#define ID_ALL_DIR_THUMB_ITEMS  2000
#define ID_SELECTED_THUMB_ITEMS 2001



#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static const int THUMB_LIST_WIDTH = 145;


IMPLEMENT_DYNCREATE(CSpriteFrame, CParentFrame)

BEGIN_MESSAGE_MAP(CSpriteFrame, CParentFrame)
	ON_COMMAND(ID_FILE_CLOSE, OnFileClose)
	ON_WM_SETFOCUS()
	ON_WM_CREATE()
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
	ON_COMMAND(ID_RUN_BUTTON, OnRunButton)
	ON_COMMAND(ID_STOP_BUTTON, OnStopButton)
	ON_UPDATE_COMMAND_UI(ID_STOP_BUTTON, OnUpdateStopButton)
	ON_UPDATE_COMMAND_UI(ID_RUN_BUTTON, OnUpdateRunButton)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_FILE_EXPORTFILES, OnFileExportFiles)
	ON_COMMAND(ID_FILE_CREATENEWPROJECT, OnFileCreateNewProject)
	ON_UPDATE_COMMAND_UI(ID_FILE_EXPORTFILES, OnUpdateFileExportFiles)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE, OnUpdateFileSave)
	ON_WM_SETCURSOR()
	ON_COMMAND(ID_FILE_SETDIRECTORIES, OnFileSetdirectories)
	ON_COMMAND(ID_FILE_BATCH_MODE, OnFileBatchMode)
	ON_COMMAND(ID_EDIT_SETBACKGROUNDCOLOR, OnEditSetbackgroundcolor)
END_MESSAGE_MAP()


CSpriteFrame::CSpriteFrame() : m_wndSelectedThumbItems( true )
{
	szComposerName = "Sprite Editor";
	szExtension = "*.spt";
	szComposerSaveName = "Sprite_Composer_Project";
	nTreeRootItemID = E_SPRITE_ROOT_ITEM;
	nFrameType = CFrameManager::E_SPRITE_FRAME;
	pWndView = new CSpriteView;
	szAddDir = "effects\\sprites\\";

	m_pActiveSpritesItem = 0;
	bRunning = false;
	bComposed = false;
	szExportExtension = ".san";

	m_nCompressedFormat = GFXPF_DXT5;
	m_nLowFormat = GFXPF_ARGB4444;
}

CSpriteFrame::~CSpriteFrame()
{
}

int CSpriteFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CParentFrame::OnCreate(lpCreateStruct) == -1)
		return -1;
	g_frameManager.AddFrame( this );

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

void CSpriteFrame::ShowFrameWindows( int nCommand )
{
	if ( bRunning )
		OnStopButton();
	CParentFrame::ShowFrameWindows( nCommand );
	if ( nCommand == SW_SHOW )
		g_frameManager.GetGameWnd()->ShowWindow( SW_HIDE );
}

BOOL CSpriteFrame::Run()
{
	if ( !bRunning )
		return FALSE;

	GFXDraw();
	g_frameManager.GetGameWnd()->ValidateRect( 0 );
	return TRUE;
}

void CSpriteFrame::GFXDraw()
{
	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER, m_backgroundColor );
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

void CSpriteFrame::ViewSizeChanged()
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
	}

	rc.bottom = rc.top;
	rc.top = viewRect.top;
	if ( m_wndAllDirThumbItems.GetSafeHwnd() )
	{
		m_wndAllDirThumbItems.MoveWindow( &rc );
	}
}

void CSpriteFrame::ClickOnThumbList( int nID )
{
	if ( nID == ID_ALL_DIR_THUMB_ITEMS )
	{
		if ( m_pActiveSpritesItem )
			m_pActiveSpritesItem->SelectMeInTheTree();
	}
	else if ( nID == ID_SELECTED_THUMB_ITEMS )
	{
		int nSel = m_wndSelectedThumbItems.GetSelectedItemIndex();
		if ( nSel == -1 )
			return;

		CTreeItem *pTreeItem = (CTreeItem *) m_wndSelectedThumbItems.GetUserDataForItem( nSel );
		pTreeItem->SelectMeInTheTree();
	}
}

void CSpriteFrame::DoubleClickOnThumbList( int nID )
{
	if ( nID == ID_ALL_DIR_THUMB_ITEMS )
	{
		if ( !m_pActiveSpritesItem )
			return;
		SetChangedFlag( true );
		bComposed = false;

		int nAllIndex = m_wndAllDirThumbItems.GetSelectedItemIndex();
		if ( nAllIndex == -1 )
			return;
		string szItemName = m_wndAllDirThumbItems.GetItemName( nAllIndex );
		int nImage = m_wndAllDirThumbItems.GetItemImageIndex( nAllIndex );

		int nNewItemIndex = m_wndSelectedThumbItems.InsertItemToEnd( szItemName.c_str(), nImage );
		NI_ASSERT( nNewItemIndex != -1 );
		
		CSpritePropsItem *pSprite = new CSpritePropsItem();
		pSprite->SetItemName( szItemName.c_str() );
		m_pActiveSpritesItem->AddChild( pSprite );
		
		NI_ASSERT( pSprite != 0 );
		m_wndSelectedThumbItems.SetUserDataForItem( nNewItemIndex, (long) pSprite );
	}
}

void CSpriteFrame::SelectItemInSelectedThumbList( DWORD dwData )
{
	int nIndex = m_wndSelectedThumbItems.GetItemIndexWithUserData( dwData );
	m_wndSelectedThumbItems.SelectItem( nIndex );
}

void CSpriteFrame::DeleteFrameInSelectedList( DWORD dwData )
{
	SetChangedFlag( true );
	bComposed = false;
	int nIndex = m_wndSelectedThumbItems.GetItemIndexWithUserData( dwData );
	m_wndSelectedThumbItems.DeleteItem( nIndex );
}

void CSpriteFrame::DeleteFrameInTree( int nID )
{
	if ( nID == ID_ALL_DIR_THUMB_ITEMS )
		return;
	
	SetChangedFlag( true );
	bComposed = false;
	
	int nSel = m_wndSelectedThumbItems.GetSelectedItemIndex();
	if ( nSel == -1 )
		return;
	DWORD dwData = m_wndSelectedThumbItems.GetUserDataForItem( nSel );
	ASSERT( dwData != 0 );
	
	CTreeItem *pFrame = (CTreeItem *) dwData;
	NI_ASSERT( pFrame->GetItemType() == E_SPRITE_PROPS_ITEM );
	pFrame->DeleteMeInParentTreeItem();
	
	m_wndSelectedThumbItems.SelectItem( nSel + 1 );
	
	m_wndSelectedThumbItems.DeleteItem( nSel );
}

void CSpriteFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();

	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRoot = pTree->GetRootItem();
	CSpritesItem *pSpritesItem = static_cast<CSpritesItem *> ( pRoot->GetBegin()->GetPtr() );
	SetActiveSpritesItem( pSpritesItem );			//прогружаем все items из директории
}

void CSpriteFrame::SpecificClearBeforeBatchMode()
{
	m_wndAllDirThumbItems.SetActiveThumbItems( 0, 0 );
	m_wndSelectedThumbItems.SetActiveThumbItems( 0, 0 );
	m_pActiveSpritesItem = 0;
	bComposed = false;
}

bool CSpriteFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem->GetItemType() == E_SPRITE_ROOT_ITEM );
	CSpriteTreeRootItem *pSpriteRoot = (CSpriteTreeRootItem *) pRootItem;
	pSpriteRoot->ComposeAnimations( pszProjectName, GetDirectory(pszResultFileName).c_str(), false );
	return true;
}

void CSpriteFrame::OnRunButton() 
{
	if ( !bComposed )
		ComposeAnimations();

	if ( bRunning )
		return;
	bRunning = !bRunning;

	g_frameManager.GetGameWnd()->ShowWindow( SW_SHOW );
	m_wndAllDirThumbItems.ShowWindow( SW_HIDE );
	m_wndSelectedThumbItems.ShowWindow( SW_HIDE );


	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	NI_ASSERT( pTree != 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	NI_ASSERT( pRootItem != 0 );
	NI_ASSERT( pRootItem->GetItemType() == E_SPRITE_ROOT_ITEM );

	CTreeItem *pSpritesItem = pRootItem->GetChildItem( E_SPRITES_ITEM );
	NI_ASSERT( pSpritesItem != 0 );
	if ( pSpritesItem->GetChildsCount() == 0 )
		return;				//нечего отображать

	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	IScene *pSG = GetSingleton<IScene>();
	GetSingleton<ICamera>()->Update();
	const CVec3 vCameraAnchor = GetSingleton<ICamera>()->GetAnchor();

	string szObjName = theApp.GetEditorTempResourceDir();
	CPtr<IObjVisObj> pObj = static_cast<IObjVisObj*>( pVOB->BuildObject( (szObjName + "\\1").c_str(), 0, SGVOT_SPRITE ) );
	pObj->SetPosition( CVec3(vCameraAnchor.x, vCameraAnchor.y, 0) );
	pObj->SetDirection( 0 );
	pObj->GetAnimation()->SetAnimation( 0 );
	pSG->AddObject( pObj, SGVOGT_UNIT );
}

void CSpriteFrame::OnStopButton() 
{
	if ( !bRunning )
		return;

	bRunning = !bRunning;

	g_frameManager.GetGameWnd()->ShowWindow( SW_HIDE );
	m_wndAllDirThumbItems.ShowWindow( SW_SHOW );
	m_wndSelectedThumbItems.ShowWindow( SW_SHOW );

	IScene *pSG = GetSingleton<IScene>();
	pSG->Clear();
}

void CSpriteFrame::OnUpdateRunButton(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 )			//Если проект не был создан
	{
		pCmdUI->Enable( false );
		return;
	}

	if ( bRunning )
		pCmdUI->Enable( false );
	else
		pCmdUI->Enable( true );
}

void CSpriteFrame::OnUpdateStopButton(CCmdUI* pCmdUI) 
{
	if ( bRunning )
		pCmdUI->Enable( true );
	else
		pCmdUI->Enable( false );
}

void CSpriteFrame::ComposeAnimations()
{
	bComposed = true;
	
	BeginWaitCursor();
	
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	NI_ASSERT( pTree != 0 );
	
	CTreeItem *pRootItem = pTree->GetRootItem();
	NI_ASSERT( pRootItem != 0 );
	
	NI_ASSERT( pRootItem->GetItemType() == E_SPRITE_ROOT_ITEM );
	CSpriteTreeRootItem *pAnimRoot = (CSpriteTreeRootItem *) pRootItem;
	string szTempDir = theApp.GetEditorTempDir();
	pAnimRoot->ComposeAnimations( szProjectFileName.c_str(), szTempDir.c_str(), true );
	
	EndWaitCursor();
}

void CSpriteFrame::ActiveDirNameChanged()
{
	SetChangedFlag( true );
	bComposed = false;

	if ( m_pActiveSpritesItem )
	{
		string szDir = GetDirectory( szProjectFileName.c_str() );
		string szFull;
		bool bRes = MakeFullPath( szDir.c_str(), m_pActiveSpritesItem->GetDirName(), szFull );

		if ( bRes )
			m_wndAllDirThumbItems.LoadAllImagesFromDir( m_pActiveSpritesItem->GetAllThumbItems(), m_pActiveSpritesItem->GetImageList(), szFull.c_str() );
		else
			m_wndAllDirThumbItems.LoadAllImagesFromDir( m_pActiveSpritesItem->GetAllThumbItems(), m_pActiveSpritesItem->GetImageList(), m_pActiveSpritesItem->GetDirName() );

		m_wndAllDirThumbItems.SetActiveThumbItems( m_pActiveSpritesItem->GetAllThumbItems(), m_pActiveSpritesItem->GetImageList() );
		m_wndSelectedThumbItems.LoadImageIndexFromThumbs( m_pActiveSpritesItem->GetAllThumbItems(), m_pActiveSpritesItem->GetImageList() );
	}
}

void CSpriteFrame::SetActiveSpritesItem( CSpritesItem *pSpritesItem )
{
	if ( pSpritesItem == m_pActiveSpritesItem )
		return;

	m_pActiveSpritesItem = pSpritesItem;
	if ( m_pActiveSpritesItem )
	{
		m_wndSelectedThumbItems.SetActiveThumbItems( m_pActiveSpritesItem->GetSelThumbItems(), 0 );
		m_wndSelectedThumbItems.LoadImageIndexFromThumbs( m_pActiveSpritesItem->GetAllThumbItems(), m_pActiveSpritesItem->GetImageList() );

		if ( !m_pActiveSpritesItem->GetLoadedFlag() )
		{
			string szEditorDataDir = theApp.GetEditorDataDir();
			szEditorDataDir += "editor\\";

			m_wndAllDirThumbItems.LoadImageToImageList( m_pActiveSpritesItem->GetImageList(), "invalid.tga", szEditorDataDir.c_str() );
			std::string szFull;
			MakeFullPath( GetDirectory( szProjectFileName.c_str() ).c_str(), m_pActiveSpritesItem->GetDirName(), szFull );
			m_wndAllDirThumbItems.LoadAllImagesFromDir( m_pActiveSpritesItem->GetAllThumbItems(), m_pActiveSpritesItem->GetImageList(), szFull.c_str() );

			m_wndAllDirThumbItems.SetActiveThumbItems( m_pActiveSpritesItem->GetAllThumbItems(), m_pActiveSpritesItem->GetImageList() );
			m_wndSelectedThumbItems.LoadImageIndexFromThumbs( m_pActiveSpritesItem->GetAllThumbItems(), m_pActiveSpritesItem->GetImageList() );


			NI_ASSERT( m_wndSelectedThumbItems.GetThumbsCount() == m_pActiveSpritesItem->GetChildsCount() );
			CTreeItem::CTreeItemList::const_iterator it;
			int i = 0;
			for ( it=m_pActiveSpritesItem->GetBegin(); it!=m_pActiveSpritesItem->GetEnd(); ++it )
			{
				m_wndSelectedThumbItems.SetUserDataForItem( i, (DWORD) it->GetPtr() );
				i++;
			}
			m_pActiveSpritesItem->SetLoadedFlag( true );
		}
	}
}

BOOL CSpriteFrame::SpecificTranslateMessage( MSG *pMsg )
{
	if ( bRunning )
	{
		if ( pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE )
		{
			OnStopButton();
			return true;
		}
	}
	
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

LRESULT CSpriteFrame::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	if ( message == WM_THUMB_LIST_SELECT )
	{
		ClickOnThumbList( wParam );
		return true;
	}

	return CParentFrame::WindowProc(message, wParam, lParam);
}

FILETIME CSpriteFrame::FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem )
{
	FILETIME minTime, current;
	string szDestDir = GetDirectory( pszResultFileName );
	
	string szTempFileName = szDestDir;
	szTempFileName += "1.san";
	current = GetFileChangeTime( szTempFileName.c_str() );
	minTime = current;
	
	szTempFileName = szDestDir;
	szTempFileName += "1.tga";
	current = GetFileChangeTime( szTempFileName.c_str() );
	if ( current < minTime )
		minTime = current;
	
	return minTime;
}
