#include "stdafx.h"
#include "frames.h"
#include "editor.h"

#include "MapEditorBarWnd.h"

//#include "tabwnd3.h"
//#include "tabwnd.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP( CInputControlBar, SECControlBar )
	//{{AFX_MSG_MAP(CInputControlBar)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_NOTIFY( CInputNotifyShortcutBar::NM_CHANGE_PAGE, IDC_INPUT_NOTIFY_SHOTRCUT_BAR_00, OnNotifyShortcutChangePage )
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CInputControlBar::OnCreate( LPCREATESTRUCT lpCreateStruct ) 
{
	if ( SECControlBar::OnCreate( lpCreateStruct ) == -1 )
	{
		return -1;
	}
	
	isCreating = true;

	inputShortcutBar.Create( this, WS_CHILD | WS_VISIBLE | SEC_OBS_VERT | SEC_OBS_ANIMATESCROLL, IDC_INPUT_NOTIFY_SHOTRCUT_BAR_00 );
	//inputShortcutBar.ModifyStyleEx( 0, WS_EX_CLIENTEDGE );

	//CTabTileEditDialog
	//CShadeEditorWnd
	if ( CInput3DTabWindow *pInput3DTabWindow = inputShortcutBar.AddInputTabWindow( static_cast<CInput3DTabWindow*>( 0 ) ) )
	{
		pInput3DTabWindow->Create( &inputShortcutBar, WS_CHILD | WS_VISIBLE | TWS_TABS_ON_BOTTOM | TWS_DRAW_3D_NORMAL );

		//CTabTileEditDialog
		if ( CTabTileEditDialog *pTabTileEditDialog = pInput3DTabWindow->AddInputTabWindow( static_cast<CTabTileEditDialog*>( 0 ) ) )
		{
			pTabTileEditDialog->Create( CTabTileEditDialog::IDD, pInput3DTabWindow );
			pInput3DTabWindow->AddTab( pTabTileEditDialog, CTemplateEditorFrame::STATE_TERRAIN_TAB_LABELS[CTemplateEditorFrame::STATE_TERRAIN_TILES] );
		}

		//CShadeEditorWnd
		if ( CShadeEditorWnd *pShadeEditorWnd = pInput3DTabWindow->AddInputTabWindow( static_cast<CShadeEditorWnd*>( 0 ) ) )
		{
			pShadeEditorWnd->Create( CShadeEditorWnd::IDD, pInput3DTabWindow );
			pInput3DTabWindow->AddTab( pShadeEditorWnd, CTemplateEditorFrame::STATE_TERRAIN_TAB_LABELS[CTemplateEditorFrame::STATE_TERRAIN_ALTITUDES] );
		}

		//CTabTerrainFieldsDialog
		if ( CTabTerrainFieldsDialog *pTabTerrainFieldsDialog = pInput3DTabWindow->AddInputTabWindow( static_cast<CTabTerrainFieldsDialog*>( 0 ) ) )
		{
			pTabTerrainFieldsDialog->Create( CTabTerrainFieldsDialog::IDD, pInput3DTabWindow );
			pInput3DTabWindow->AddTab( pTabTerrainFieldsDialog, CTemplateEditorFrame::STATE_TERRAIN_TAB_LABELS[CTemplateEditorFrame::STATE_TERRAIN_FIELDS] );
		}

		//проверить на количество созданных табов
		NI_ASSERT_T( ( pInput3DTabWindow->GetTabCount() == CTemplateEditorFrame::STATE_TERRAIN_COUNT ) &&
								 ( pInput3DTabWindow->inputTabWindows.size() == CTemplateEditorFrame::STATE_TERRAIN_COUNT ),
								 NStr::Format( "Wrong tab number: %d\n", pInput3DTabWindow->GetTabCount() ) );

		pInput3DTabWindow->SetTabIcon( CTemplateEditorFrame::STATE_TERRAIN_TILES,			IDI_TERRAIN_TAB );
		pInput3DTabWindow->SetTabIcon( CTemplateEditorFrame::STATE_TERRAIN_ALTITUDES,	IDI_HEIGHTS_TAB );
		pInput3DTabWindow->SetTabIcon( CTemplateEditorFrame::STATE_TERRAIN_FIELDS,		IDI_FIELDS_TAB );

		pInput3DTabWindow->ActivateTab( CTemplateEditorFrame::STATE_TERRAIN_TILES );
		inputShortcutBar.AddBar( pInput3DTabWindow, CTemplateEditorFrame::STATE_TAB_LABELS[CTemplateEditorFrame::STATE_TERRAIN], true );
	}
	
	//CTabSimpleObjectsDialog
	//CFenceSetupWindow
	//CBridgeSetupDialog
	if ( CInput3DTabWindow *pInput3DTabWindow = inputShortcutBar.AddInputTabWindow( static_cast<CInput3DTabWindow*>( 0 ) ) )
	{
		pInput3DTabWindow->Create( &inputShortcutBar, WS_CHILD | WS_VISIBLE | TWS_TABS_ON_BOTTOM | TWS_DRAW_3D_NORMAL );

		//CTabSimpleObjectsDialog
		if ( CTabSimpleObjectsDialog *pTabSimpleObjectsDialog = pInput3DTabWindow->AddInputTabWindow( static_cast<CTabSimpleObjectsDialog*>( 0 ) ) )
		{
			pTabSimpleObjectsDialog->Create( CTabSimpleObjectsDialog::IDD, pInput3DTabWindow );
			pInput3DTabWindow->AddTab( pTabSimpleObjectsDialog, CTemplateEditorFrame::STATE_SO_TAB_LABELS[CTemplateEditorFrame::STATE_SO_OBJECTS] );
		}

		//CFenceSetupWindow
		if ( CFenceSetupWindow *pFenceSetupWindow = pInput3DTabWindow->AddInputTabWindow( static_cast<CFenceSetupWindow*>( 0 ) ) )
		{
			pFenceSetupWindow->Create( CFenceSetupWindow::IDD, pInput3DTabWindow );
			pInput3DTabWindow->AddTab( pFenceSetupWindow, CTemplateEditorFrame::STATE_SO_TAB_LABELS[CTemplateEditorFrame::STATE_SO_FENCES] );
		}

		//CBridgeSetupDialog
		if ( CBridgeSetupDialog *pBridgeSetupDialog = pInput3DTabWindow->AddInputTabWindow( static_cast<CBridgeSetupDialog*>( 0 ) ) )
		{
			pBridgeSetupDialog->Create( CBridgeSetupDialog::IDD, pInput3DTabWindow );
			pInput3DTabWindow->AddTab( pBridgeSetupDialog, CTemplateEditorFrame::STATE_SO_TAB_LABELS[CTemplateEditorFrame::STATE_SO_BRIDGES] );
		}

		//проверить на количество созданных табов
		NI_ASSERT_T( ( pInput3DTabWindow->GetTabCount() == CTemplateEditorFrame::STATE_SO_COUNT ) &&
								 ( pInput3DTabWindow->inputTabWindows.size() == CTemplateEditorFrame::STATE_SO_COUNT ),
								 NStr::Format( "Wrong tab number: %d\n", pInput3DTabWindow->GetTabCount() ) );

		pInput3DTabWindow->SetTabIcon( CTemplateEditorFrame::STATE_SO_OBJECTS,	IDI_OBJECTS_TAB );
		pInput3DTabWindow->SetTabIcon( CTemplateEditorFrame::STATE_SO_FENCES,	IDI_FENCES_TAB );
		pInput3DTabWindow->SetTabIcon( CTemplateEditorFrame::STATE_SO_BRIDGES,	IDI_BRIDGES_TAB );

		pInput3DTabWindow->ActivateTab( CTemplateEditorFrame::STATE_SO_OBJECTS );
		inputShortcutBar.AddBar( pInput3DTabWindow, CTemplateEditorFrame::STATE_TAB_LABELS[CTemplateEditorFrame::STATE_SIMPLE_OBJECTS], true );
	}
	
	//CTrenchSetupWindow
	//CRoad3DSetupWindow
	//CRiverSetupWindow
	if ( CInput3DTabWindow *pInput3DTabWindow = inputShortcutBar.AddInputTabWindow( static_cast<CInput3DTabWindow*>( 0 ) ) )
	{
		//pInput3DTabWindow->Create( &inputShortcutBar, WS_CHILD | WS_VISIBLE | TWS_TABS_ON_LEFT );
		pInput3DTabWindow->Create( &inputShortcutBar, WS_CHILD | WS_VISIBLE | TWS_TABS_ON_BOTTOM | TWS_DRAW_3D_NORMAL );// | TWS_DRAW_3D_NORMAL | TWS_NOACTIVE_TAB_ENLARGED

		//CTrenchSetupWindow
		if ( CTrenchSetupWindow *pTrenchSetupWindow = pInput3DTabWindow->AddInputTabWindow( static_cast<CTrenchSetupWindow*>( 0 ) ) )
		{
			pTrenchSetupWindow->Create( CTrenchSetupWindow::IDD, pInput3DTabWindow );
			pInput3DTabWindow->AddTab( pTrenchSetupWindow, CTemplateEditorFrame::STATE_VO_TAB_LABELS[CTemplateEditorFrame::STATE_VO_ENTRENCHMENTS] );
		}

		//CRoadSetupWindow
		if ( CTabVOVSODialog *pTabVOVSODialog = pInput3DTabWindow->AddInputTabWindow( static_cast<CTabVOVSODialog*>( 0 ) ) )
		{
			pTabVOVSODialog->SetDialogName( "CRoadSetupWindow" );
			pTabVOVSODialog->Create( CTabVOVSODialog::IDD, pInput3DTabWindow );
			pTabVOVSODialog->SetListLabel( "Available roads:" );
			pInput3DTabWindow->AddTab( pTabVOVSODialog, CTemplateEditorFrame::STATE_VO_TAB_LABELS[CTemplateEditorFrame::STATE_VO_ROADS3D] );
		}

		//CRiverSetupWindow
		if ( CTabVOVSODialog *pTabVOVSODialog = pInput3DTabWindow->AddInputTabWindow( static_cast<CTabVOVSODialog*>( 0 ) ) )
		{
			pTabVOVSODialog->SetDialogName( "CRiverSetupWindow" );
			pTabVOVSODialog->Create( CTabVOVSODialog::IDD, pInput3DTabWindow );
			pTabVOVSODialog->SetListLabel( "Available rivers:" );
			pInput3DTabWindow->AddTab( pTabVOVSODialog, CTemplateEditorFrame::STATE_VO_TAB_LABELS[CTemplateEditorFrame::STATE_VO_RIVERS] );
		}

		//проверить на количество созданных табов
		NI_ASSERT_T( ( pInput3DTabWindow->GetTabCount() == CTemplateEditorFrame::STATE_VO_COUNT ) &&
								 ( pInput3DTabWindow->inputTabWindows.size() == CTemplateEditorFrame::STATE_VO_COUNT ),
								 NStr::Format( "Wrong tab number: %d\n", pInput3DTabWindow->GetTabCount() ) );

		pInput3DTabWindow->SetTabIcon( CTemplateEditorFrame::STATE_VO_ENTRENCHMENTS,	IDI_ENTRENCHMENTS_TAB );
		pInput3DTabWindow->SetTabIcon( CTemplateEditorFrame::STATE_VO_ROADS3D,				IDI_ROADS_TAB );
		pInput3DTabWindow->SetTabIcon( CTemplateEditorFrame::STATE_VO_RIVERS,					IDI_RIVERS_TAB );

		pInput3DTabWindow->ActivateTab( CTemplateEditorFrame::STATE_VO_ENTRENCHMENTS );
		inputShortcutBar.AddBar( pInput3DTabWindow, CTemplateEditorFrame::STATE_TAB_LABELS[CTemplateEditorFrame::STATE_VECTOR_OBJECTS], true );
	}

	//CTabToolsDialog
	if ( CTabToolsDialog *pTabToolsDialog = inputShortcutBar.AddInputTabWindow( static_cast<CTabToolsDialog*>( 0 ) ) )
	{
		pTabToolsDialog->Create( CTabToolsDialog::IDD, &inputShortcutBar );
		inputShortcutBar.AddBar( pTabToolsDialog, CTemplateEditorFrame::STATE_TAB_LABELS[CTemplateEditorFrame::STATE_TOOLS], true );
	}

	//CGroupManagerDialog
	if ( CGroupManagerDialog *pGroupManagerDialog = inputShortcutBar.AddInputTabWindow( static_cast<CGroupManagerDialog*>( 0 ) ) )
	{
		pGroupManagerDialog->Create( CGroupManagerDialog::IDD, &inputShortcutBar );
		inputShortcutBar.AddBar( pGroupManagerDialog, CTemplateEditorFrame::STATE_TAB_LABELS[CTemplateEditorFrame::STATE_GROUPS], true );
	}

	//CTabAIGeneralDialog
	if ( CTabAIGeneralDialog *pTabAIGeneralDialog = inputShortcutBar.AddInputTabWindow( static_cast<CTabAIGeneralDialog*>( 0 ) ) )
	{
		pTabAIGeneralDialog->Create( CTabAIGeneralDialog::IDD, &inputShortcutBar );
		inputShortcutBar.AddBar( pTabAIGeneralDialog, CTemplateEditorFrame::STATE_TAB_LABELS[CTemplateEditorFrame::STATE_AI_GENERAL], true );
	}

	//проверить на количество созданных шорткатов
	NI_ASSERT_T( ( inputShortcutBar.GetBarCount() == CTemplateEditorFrame::STATE_COUNT ) &&
							 ( inputShortcutBar.inputTabWindows.size() == CTemplateEditorFrame::STATE_COUNT ),
							 NStr::Format( "Wrong shortcut number: %d\n", inputShortcutBar.GetBarCount() ) );
	
	inputShortcutBar.SelectPane( CTemplateEditorFrame::STATE_TERRAIN );
	isCreating = false;
	return 0; 
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInputControlBar::OnSize(UINT nType, int cx, int cy ) 
{
	SECControlBar::OnSize(nType, cx, cy );
	
	if( inputShortcutBar.GetSafeHwnd() != NULL )
	{
		CRect insideRect;
		GetInsideRect( insideRect );
		inputShortcutBar.SetWindowPos( NULL, insideRect.left, insideRect.top, insideRect.Width(), insideRect.Height(), SWP_SHOWWINDOW );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInputControlBar::OnNotifyShortcutChangePage( NMHDR *pNotifyStruct, LRESULT *pResult )
{
	if ( !isCreating )
	{
		if ( CInputNotifyShortcutBar::SNotifyStruct* pStruct = static_cast<CInputNotifyShortcutBar::SNotifyStruct*>( pNotifyStruct ) )
		{
			if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
			{
				pFrame->SendMessage( CTemplateEditorFrame::UM_CHANGE_SHORTCUT_BAR_PAGE,
														 static_cast<WPARAM>( pStruct->nShortcutIndex ),
														 static_cast<LPARAM>( pStruct->nTabIndex ) );
			}
		}
	}
	if ( pResult )
		*pResult = 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
