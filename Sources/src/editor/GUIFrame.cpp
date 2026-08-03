#include "StdAfx.h"

#include "..\Scene\Scene.h"
#include "..\main\imain.h"
#include "..\Input\Input.h"
#include "..\Input\InputHelper.h"
#include "..\Main\iMainCommands.h"

#include "editor.h"
#include "TreeDockWnd.h"
#include "PropertyDockBar.h"
#include "MainFrm.h"
#include "PropView.h"
#include "TreeItem.h"
#include "GameWnd.h"
#include "GUIFrame.h"
#include "frames.h"
#include "RefDlg.h"
#include "GUITreeItem.h"

static const NInput::SRegisterCommandEntry stdCommands[] = 
{
	{ "begin_action1"	,	CMD_BEGIN_ACTION1		},
	{ "end_action1"		,	CMD_END_ACTION1			},
	{ "begin_action2"	,	CMD_BEGIN_ACTION2		},
	{ "end_action2"		,	CMD_END_ACTION2			},
/*
	{ "mouse_button0_down", CMD_MOUSE_BUTTON0_DOWN },
	{ "mouse_button0_up"	, CMD_MOUSE_BUTTON0_UP	 },
	{ "mouse_button1_down", CMD_MOUSE_BUTTON1_DOWN },
	{ "mouse_button1_up"	, CMD_MOUSE_BUTTON1_UP	 },
	{ "mouse_button2_down", CMD_MOUSE_BUTTON2_DOWN },
	{ "mouse_button2_up"	, CMD_MOUSE_BUTTON2_UP	 },
*/
	{ 0,						0								}
};


IMPLEMENT_DYNCREATE(CGUIFrame, CParentFrame)

BEGIN_MESSAGE_MAP(CGUIFrame, CParentFrame)
ON_WM_CREATE()
ON_COMMAND(ID_RUN_BUTTON, OnRunButton)
ON_COMMAND(ID_STOP_BUTTON, OnStopButton)
ON_UPDATE_COMMAND_UI(ID_STOP_BUTTON, OnUpdateStopButton)
ON_UPDATE_COMMAND_UI(ID_RUN_BUTTON, OnUpdateRunButton)
ON_UPDATE_COMMAND_UI(ID_INSERT_TREE_ITEM, OnUpdateInsertTreeItem)
ON_WM_LBUTTONDOWN()
ON_WM_RBUTTONDOWN()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONUP()
ON_WM_KEYDOWN()
ON_WM_RBUTTONUP()
ON_COMMAND(ID_CREATENEWTEMPLATE, OnCreatenewtemplate)
ON_COMMAND(ID_TESTBUTTON, OnTestbutton)
ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
ON_COMMAND(ID_EDIT_CUT, OnEditCut)
ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
END_MESSAGE_MAP()


CGUIFrame::CGUIFrame()
{
	szComposerName = "GUI Editor";
	szExtension = "*.gui";
	szComposerSaveName = "GUI_Composer_Project";
	nTreeRootItemID = E_GUI_ROOT_ITEM;
	nFrameType = CFrameManager::E_GUI_FRAME;
	
	bRunning = false;
	pWndView = new CGUIView;
	pPropertyDockBar = 0;
	
	pTemplatePropsItem = 0;
	m_pScreen = static_cast<IUIScreen*>( GetCommonFactory()->CreateObject( UI_SCREEN ) );
	CTRect<float> rcScreen;
	rcScreen.left = 0;
	rcScreen.top = 0;
	rcScreen.right = 800;
	rcScreen.bottom = 600;
	m_pScreen->Reposition( rcScreen );
	m_pContainer = m_pScreen;
	m_pHigh = 0;
	m_mode = MODE_FREE;
	mouseState = E_MOUSE_FREE;
}

CGUIFrame::~CGUIFrame()
{
}

int CGUIFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
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

	GenerateProjectName();

/*
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+6)->SetButtonStyle( 0, TBBS_CHECKBOX | TBBS_CHECKED );
	pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+6)->SetButtonStyle( 1, TBBS_CHECKBOX );
*/
	
	return 0;
}

void CGUIFrame::Init( IGFX *_pGFX )
{
	pGFX = _pGFX;

	IInput *pInput = GetSingleton<IInput>();
	pInput->SetBindSection( "game_mission" );
	standardMsgs.Init( pInput, stdCommands );
}

void CGUIFrame::ShowFrameWindows( int nCommand )
{
	if ( bRunning )
		OnStopButton();
	CParentFrame::ShowFrameWindows( nCommand );
	if ( pPropertyDockBar )
		theApp.ShowSECControlBar( pPropertyDockBar, nCommand );
	g_frameManager.GetGameWnd()->ShowWindow( SW_SHOW );

	if ( nCommand == SW_SHOW )
	{
		GetSingleton<ICursor>()->Show( true );
		GetSingleton<ICursor>()->SetMode( 2 );
	}
	else
		GetSingleton<ICursor>()->Show( false );
}

BOOL CGUIFrame::Run()
{
	if ( !bRunning )
		return FALSE;

	IInput *pInput = GetSingleton<IInput>();
	pInput->PumpMessages( true );
	CVec2 vPos2 = GetSingleton<ICursor>()->GetPos();
	SGameMessage msg;

	while ( pInput->GetMessage( &msg ) )
	{
		switch( msg.nEventID )
		{
		case CMD_BEGIN_ACTION1:
			mouseState |= E_LBUTTONDOWN;
			m_pScreen->OnLButtonDown( vPos2, (EMouseState) mouseState );
			break;
		case CMD_END_ACTION1:
			mouseState &= ~E_LBUTTONDOWN;
			m_pScreen->OnLButtonUp( vPos2, (EMouseState) mouseState );
			break;
			/*
			case CMD_BEGIN_ACTION2:
			mouseState |= E_RBUTTONDOWN;
			m_pScreen->OnRButtonDown( vPos2, (EMouseState) mouseState );
			break;
			case CMD_END_ACTION2:
			mouseState &= ~E_RBUTTONDOWN;
			m_pScreen->OnRButtonUp( vPos2, (EMouseState) mouseState );
			break;
			*/
		}
	}
	m_pScreen->OnMouseMove( vPos2, (EMouseState) mouseState );
	
	GFXDraw();
	g_frameManager.GetGameWnd()->ValidateRect( 0 );
	return TRUE;
}

BOOL CGUIFrame::SpecificTranslateMessage( MSG *pMsg )
{
	if ( bRunning )
	{
		if ( pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE )
		{
			OnStopButton();
			return true;
		}
	}
	return false;
}

void CGUIFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();

	int nFirstChildId = m_pScreen->Load( szProjectFileName.c_str(), false );
	if ( nFirstChildId != 0 )
	{
		IUIElement *pFirstChild = m_pScreen->GetChildByID( nFirstChildId );
		if ( pFirstChild != 0 )
		{
			IUIContainer *pContainer = dynamic_cast<IUIContainer *> ( pFirstChild );
			if ( pContainer != 0 )
				m_pContainer = pContainer;
		}
	}
	if ( m_pContainer == 0 )
		m_pContainer = m_pScreen;
	CTRect<float> clientRc = GetElementRect( m_pScreen );
	m_pScreen->Reposition( clientRc );
}

void CGUIFrame::SpecificClearBeforeBatchMode()
{
	m_pContainer = 0;
}

void CGUIFrame::SpecificSave( IDataTree *pDT )
{
	m_pScreen->operator &( *pDT );
	if ( !m_undoStack.empty() )
		pUnchanged = m_undoStack.back();
	else
		pUnchanged = 0;
}

bool CGUIFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem->GetItemType() == E_GUI_ROOT_ITEM );
	m_pScreen->operator&( *pDT );

	return true;
}

void CGUIFrame::OnRunButton() 
{
	if ( bRunning )
		return;
	bRunning = !bRunning;
	
	pWndView->SetFocus();
}

void CGUIFrame::OnStopButton() 
{
	if ( !bRunning )
		return;
	
	bRunning = !bRunning;
	GetSingleton<IInput>()->PumpMessages( false );
	pGFX->SetShadingEffect( 3 );
	GFXDraw();
}

void CGUIFrame::OnUpdateRunButton(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
	{
		if ( !bRunning )
			pCmdUI->Enable( true );
		else
			pCmdUI->Enable( false );
	}
	else
		pCmdUI->Enable( false );
}

void CGUIFrame::OnUpdateStopButton(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )
	{
		if ( bRunning )
			pCmdUI->Enable( true );
		else
			pCmdUI->Enable( false );
	}
	else
		pCmdUI->Enable( false );
}

int CGUIFrame::DisplayInsertMenu()
{
	POINT point;
	GetCursorPos( &point );

	CMenu menu;
	menu.LoadMenu( IDR_INSERT_TREE_ITEM_MENU );
	CMenu *popup = menu.GetSubMenu( 0 );
	int nRes = popup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD, point.x, point.y, this );
	return nRes;
}

void CGUIFrame::OnUpdateInsertTreeItem(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable( true );
}

const char *CGUIFrame::GetDirectoryFromWindowType( int nWindowType )
{
	switch ( nWindowType )
	{
		case UI_BUTTON:
			return "Buttons\\";
		case UI_STATIC:
			return "Statics\\";
		case UI_STATUS_BAR:
			return "Status bars\\";
		case UI_DIALOG:
			return "Dialogs\\";
		case UI_SLIDER:
			return "Sliders\\";
		case UI_SCROLLBAR:
			return "Scrollbars\\";
		case UI_LIST:
			return "Lists\\";
		default:
			NI_ASSERT_T( 0, "Unknown window type" );
			return "";
	}
}

void CGUIFrame::OnTestbutton() 
{
	IObjectFactory *pFactory = GetCommonFactory();

	string szName;
	CPtr<IDataStream> pStream;
	CPtr<IDataTree> pDT;

/*
	IUIStatic *pStatic = ( IUIStatic *) pFactory->CreateObject( UI_STATIC );
	szName = theApp.GetEditorDataDir();
	szName += "editor\\UI\\statics\\default.xml";
	pStream = CreateFileStream( szName.c_str(), STREAM_ACCESS_WRITE );
	pDT = CreateDataTreeSaver( pStream, IDataTree::WRITE );
	pStatic->operator&( *pDT );
	
	IUIButton *pButton = ( IUIButton *) pFactory->CreateObject( UI_BUTTON );
	szName = theApp.GetEditorDataDir();
	szName += "editor\\UI\\buttons\\default.xml";
	pStream = CreateFileStream( szName.c_str(), STREAM_ACCESS_WRITE );
	pDT = CreateDataTreeSaver( pStream, IDataTree::WRITE );
	pButton->operator&( *pDT );

	IUISlider *pSlider = ( IUISlider *) pFactory->CreateObject( UI_SLIDER );
	szName = theApp.GetEditorDataDir();
	szName += "editor\\UI\\sliders\\default.xml";
	pStream = CreateFileStream( szName.c_str(), STREAM_ACCESS_WRITE );
	pDT = CreateDataTreeSaver( pStream, IDataTree::WRITE );
	pSlider->operator&( *pDT );

	IUIScrollBar *pScrollBar = ( IUIScrollBar *) pFactory->CreateObject( UI_SCROLLBAR );
	szName = theApp.GetEditorDataDir();
	szName += "editor\\UI\\scrollbars\\default.xml";
	pStream = CreateFileStream( szName.c_str(), STREAM_ACCESS_WRITE );
	pDT = CreateDataTreeSaver( pStream, IDataTree::WRITE );
	pScrollBar->operator&( *pDT );
	
	IUIStatusBar *pStatusBar = ( IUIStatusBar *) pFactory->CreateObject( UI_STATUS_BAR );
	szName = theApp.GetEditorDataDir();
	szName += "editor\\UI\\statusbars\\default.xml";
	pStream = CreateFileStream( szName.c_str(), STREAM_ACCESS_WRITE );
	pDT = CreateDataTreeSaver( pStream, IDataTree::WRITE );
	pStatusBar->operator&( *pDT );
*/

	IUIListControl *pList = ( IUIListControl *) pFactory->CreateObject( UI_LIST );
	szName = theApp.GetEditorDataDir();
	szName += "editor\\UI\\lists\\default.xml";
	pStream = CreateFileStream( szName.c_str(), STREAM_ACCESS_WRITE );
	pDT = CreateDataTreeSaver( pStream, IDataTree::WRITE );
	pList->operator&( *pDT );
	
/*
	IUIDialog *pDialog = ( IUIDialog *) pFactory->CreateObject( UI_DIALOG );
	szName = theApp.GetEditorDataDir();
	szName += "editor\\UI\\dialogs\\default.xml";
	pStream = CreateFileStream( szName.c_str(), STREAM_ACCESS_WRITE );
	pDT = CreateDataTreeSaver( pStream, IDataTree::WRITE );
	pDialog->operator&( *pDT );


	IUINumberIndicator *pNumIndicator = ( IUINumberIndicator *) pFactory->CreateObject( UI_NUMBER_INDICATOR );
	szName = theApp.GetEditorDataDir();
	szName += "editor\\UI\\NumberIndicators\\default.xml";
	pStream = CreateFileStream( szName.c_str(), STREAM_ACCESS_WRITE );
	pDT = CreateDataTreeSaver( pStream, IDataTree::WRITE );
	pNumIndicator->operator&( *pDT );
*/
}

IUIElement *CGUIFrame::GUICreateElement()
{
	if ( pTemplatePropsItem == 0 )
		return 0;
	
	IUIElement *pWindow = static_cast<IUIElement*>( GetCommonFactory()->CreateObject( pTemplatePropsItem->GetWindowType() ) );
	NI_ASSERT_T( pWindow != 0 , NStr::Format( "Cannot create window by type: %d", pTemplatePropsItem->GetWindowType() ) );
	CPtr<IDataStream> pStream = CreateFileStream( pTemplatePropsItem->GetXMLFileName(), STREAM_ACCESS_READ );
	CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::READ );
	
	pWindow->operator&( *pDT );
	return pWindow;
}

void CGUIFrame::OnUpdateEditCopy(CCmdUI* pCmdUI) 
{
	if ( !m_selectedList.empty() )
		pCmdUI->Enable( TRUE );
	else
		pCmdUI->Enable( FALSE );
}

void CGUIFrame::OnUpdateEditPaste(CCmdUI* pCmdUI) 
{
	if ( !m_copiedList.empty() )
		pCmdUI->Enable( TRUE );
	else
		pCmdUI->Enable( FALSE );
}

void CGUIFrame::OnUpdateEditCut(CCmdUI* pCmdUI) 
{
	if ( !m_selectedList.empty() )
		pCmdUI->Enable( TRUE );
	else
		pCmdUI->Enable( FALSE );
}

void CGUIFrame::OnEditCopy() 
{
	m_copiedList.clear();
	for ( CWindowList::iterator it=m_selectedList.begin(); it!=m_selectedList.end(); ++it )
	{
		m_copiedList.push_back( (*it).GetPtr() );
	}
}

void CGUIFrame::OnEditCut() 
{
	m_undoStack.push_back( new CSaveAllUndo( m_pContainer ) );
	m_copiedList.clear();
	if ( m_selectedList.empty() )
		return;
	
	for ( CWindowList::iterator it=m_selectedList.begin(); it!=m_selectedList.end(); ++it )
	{
		m_copiedList.push_back( (*it).GetPtr() );
	}
	
	for ( CWindowList::iterator it=m_selectedList.begin(); it!=m_selectedList.end(); ++it )
	{
		m_pContainer->RemoveChild( *it );
	}
	m_pHigh = 0;
	m_selectedList.clear();
	GFXDraw();
	pPropertyDockBar->ClearVariables();
}

void CGUIFrame::OnEditPaste() 
{
	if ( m_copiedList.empty() )
		return;

	CTRect<float> minRC = GetElementRect( m_copiedList.front() );
	for ( CCopyWindowList::iterator it=m_copiedList.begin(); it!=m_copiedList.end(); ++it )
	{
		CTRect<float> rc = GetElementRect( *it );
		if ( rc.left < minRC.left )
			minRC.left = rc.left;
		if ( rc.top < minRC.top )
			minRC.top = rc.top;
	}
	minRC.left -= 50;
	minRC.top  -= 50;
	CTRect<float> parentRC = GetElementRect( m_pContainer );
	minRC.left -= parentRC.left;
	minRC.top -= parentRC.top;

	CPtr<IDataStorage> pStorage = OpenStorage( "memory", STREAM_ACCESS_READ | STREAM_ACCESS_WRITE, STORAGE_TYPE_MEM );
	NI_ASSERT( pStorage != 0 );
	
	m_selectedList.clear();
	for ( CCopyWindowList::iterator it=m_copiedList.begin(); it!=m_copiedList.end(); ++it )
	{
		{
			CPtr<IDataStream> pStream = pStorage->CreateStream( "element", STREAM_ACCESS_WRITE );
			CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::WRITE );
			(*it)->operator&( *pDT );
		}
		
		{
			CPtr<IDataStream> pStream = pStorage->OpenStream( "element", STREAM_ACCESS_READ );
			CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::READ );
			CTreeAccessor tree = pDT;
			int nClassTypeID = GetCommonFactory()->GetObjectTypeID( *it );

			if ( nClassTypeID == UI_DIALOG )
			{
				if ( m_pContainer.GetPtr() != m_pScreen.GetPtr() )
					return;		//уже создан диалог на экране
			}

			m_undoStack.push_back( new CSaveAllUndo( m_pContainer ) );
			IUIElement *pWindow = static_cast<IUIElement*>( GetCommonFactory()->CreateObject( nClassTypeID ) );
			NI_ASSERT( pWindow != 0 );
			pWindow->operator&( *pDT );
			
			CTRect<float> rc = GetElementRect( *it );
			rc.left -= minRC.left;
			rc.right -= minRC.left;
			rc.top -= minRC.top;
			rc.bottom -= minRC.top;
			
			m_pContainer->AddChild( pWindow );
			SetElementRect( pWindow, rc );
			
			m_selectedList.push_back( pWindow );
		}
	}
	
	GFXDraw();
}

void CGUIFrame::OnEditUndo() 
{
	if ( m_undoStack.empty() )
		return;
	
	pPropertyDockBar->ClearVariables();
	IGUIUndo *pUndoCommand = m_undoStack.back();
	pUndoCommand->Undo();
	m_undoStack.pop_back();
	m_pHigh = 0;
	m_selectedList.clear();

	CTRect<float> screenRC = GetElementRect( m_pScreen );
	m_pScreen->Reposition( screenRC );

	if ( m_undoStack.empty() )
	{
		if ( pUnchanged == 0 )
			SetChangedFlag( false );
		else
			SetChangedFlag( true );
	}
	else if ( m_undoStack.back() == pUnchanged )
		SetChangedFlag( false );
	else
		SetChangedFlag( true );
	
	GFXDraw();
}
