#if !defined(__INPUT_CONTROL_BAR__)
#define __INPUT_CONTROL_BAR__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//все диалоги хранятся внутри этого элемента
#include "InputNotifyShortcutBar.h"
#include "Input3DTabWnd.h"

#include "TemplateEditorFrame1.h"

#include "TabTileEditDialog.h"
#include "TabTerrainAltitudesDialog.h"
#include "TabTerrainFieldsDialog.h"

#include "TabSimpleObjectsDialog.h"
#include "FenceSetupWindow.h"
#include "BridgeSetupDialog.h"

#include "TrenchSetupWindow.h"
#include "TabVOVSODialog.h"

#include "TabToolsDialog.h"
#include "GroupManagerDialog.h"
#include "TabAIGeneralDialog.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CInputControlBar : public SECControlBar
{
	friend class CMainFrame;
	friend class CTemplateEditorFrame;
	CInputNotifyShortcutBar inputShortcutBar;

	bool isCreating;
	//{{AFX_VIRTUAL(CInputControlBar)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CInputControlBar)
protected:
	afx_msg int OnCreate( LPCREATESTRUCT lpCreateStruct );
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnNotifyShortcutChangePage( NMHDR *pNotifyStruct, LRESULT *pResult );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	CWnd* GetInputStateWindow( CTemplateEditorFrame::INPUT_STATES inputStateIndex )
	{
		NI_ASSERT_T( ( inputShortcutBar.GetBarCount() == CTemplateEditorFrame::STATE_COUNT ) &&
								 ( inputShortcutBar.inputTabWindows.size() == CTemplateEditorFrame::STATE_COUNT ),
								 NStr::Format( "Wrong input state window count: %d\n", inputShortcutBar.GetBarCount() ) );
		NI_ASSERT_T( ( inputStateIndex >= 0 ) && ( inputStateIndex < CTemplateEditorFrame::STATE_COUNT ),
								 NStr::Format( "Wrong input state window index: %d\n", inputStateIndex ) );
		
		SECBar& rSECBar = inputShortcutBar.GetBar( inputStateIndex );
		CWnd *pWnd = rSECBar.GetWnd();
		
		NI_ASSERT_T( ( pWnd != 0 ) && ::IsWindow( pWnd->m_hWnd ),
								 NStr::Format( "Bad input state window!\n" ) );
		
		return pWnd;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	CWnd* GetStateObjectsWindowTab( CTemplateEditorFrame::INPUT_STATES_SIMPLE_OBJECTS inputStateVectorObjectIndex )
	{
		CInput3DTabWindow* pInput3DTabWindow = dynamic_cast<CInput3DTabWindow*>( GetInputStateWindow( CTemplateEditorFrame::STATE_SIMPLE_OBJECTS ) );

		NI_ASSERT_T( pInput3DTabWindow != 0,
								 NStr::Format( "Bag cast!\n" ) );
		NI_ASSERT_T( ( pInput3DTabWindow->GetTabCount() == CTemplateEditorFrame::STATE_SO_COUNT ) &&
								 ( pInput3DTabWindow->inputTabWindows.size() == CTemplateEditorFrame::STATE_SO_COUNT ),
								 NStr::Format( "Wrong input state vector objects window count: %d\n", inputShortcutBar.GetBarCount() ) );
		NI_ASSERT_T( ( inputStateVectorObjectIndex >= 0 ) && ( inputStateVectorObjectIndex < CTemplateEditorFrame::STATE_SO_COUNT ),
								 NStr::Format( "Wrong input state vector objects window index: %d\n", inputStateVectorObjectIndex ) );
		
		CWnd *pWnd = pInput3DTabWindow->inputTabWindows[inputStateVectorObjectIndex];
		NI_ASSERT_T( ( pWnd != 0 ) && ::IsWindow( pWnd->m_hWnd ),
								 NStr::Format( "Bad input state window!\n" ) );

		return pWnd;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	CWnd* GetStateVOWindowTab( CTemplateEditorFrame::INPUT_STATES_VECTOR_OBJECTS inputStateVectorObjectIndex )
	{
		CInput3DTabWindow* pInput3DTabWindow = dynamic_cast<CInput3DTabWindow*>( GetInputStateWindow( CTemplateEditorFrame::STATE_VECTOR_OBJECTS ) );

		NI_ASSERT_T( pInput3DTabWindow != 0,
								 NStr::Format( "Bag cast!\n" ) );
		NI_ASSERT_T( ( pInput3DTabWindow->GetTabCount() == CTemplateEditorFrame::STATE_VO_COUNT ) &&
								 ( pInput3DTabWindow->inputTabWindows.size() == CTemplateEditorFrame::STATE_VO_COUNT ),
								 NStr::Format( "Wrong input state vector objects window count: %d\n", inputShortcutBar.GetBarCount() ) );
		NI_ASSERT_T( ( inputStateVectorObjectIndex >= 0 ) && ( inputStateVectorObjectIndex < CTemplateEditorFrame::STATE_VO_COUNT ),
								 NStr::Format( "Wrong input state vector objects window index: %d\n", inputStateVectorObjectIndex ) );
		
		CWnd *pWnd = pInput3DTabWindow->inputTabWindows[inputStateVectorObjectIndex];
		NI_ASSERT_T( ( pWnd != 0 ) && ::IsWindow( pWnd->m_hWnd ),
								 NStr::Format( "Bad input state window!\n" ) );

		return pWnd;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	CWnd* GetStateTerrainWindowTab( CTemplateEditorFrame::INPUT_STATES_TERRAIN inputStateTerrainIndex )
	{
		CInput3DTabWindow* pInput3DTabWindow = dynamic_cast<CInput3DTabWindow*>( GetInputStateWindow( CTemplateEditorFrame::STATE_TERRAIN ) );

		NI_ASSERT_T( pInput3DTabWindow != 0,
								 NStr::Format( "Bag cast!\n" ) );
		NI_ASSERT_T( ( pInput3DTabWindow->GetTabCount() == CTemplateEditorFrame::STATE_TERRAIN_COUNT ) &&
								 ( pInput3DTabWindow->inputTabWindows.size() == CTemplateEditorFrame::STATE_TERRAIN_COUNT ),
								 NStr::Format( "Wrong input state terrain window count: %d\n", inputShortcutBar.GetBarCount() ) );
		NI_ASSERT_T( ( inputStateTerrainIndex >= 0 ) && ( inputStateTerrainIndex < CTemplateEditorFrame::STATE_TERRAIN_COUNT ),
								 NStr::Format( "Wrong input state terrain window index: %d\n", inputStateTerrainIndex ) );
		
		CWnd *pWnd = pInput3DTabWindow->inputTabWindows[inputStateTerrainIndex];
		NI_ASSERT_T( ( pWnd != 0 ) && ::IsWindow( pWnd->m_hWnd ),
								 NStr::Format( "Bad input state window!\n" ) );

		return pWnd;
	}

public:
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	CInputControlBar() : isCreating( true ) {}
	
	int GetRoadEditorState() 
	{
		CInput3DTabWindow* pInput3DTabWindowVO = dynamic_cast<CInput3DTabWindow*>( GetInputStateWindow( CTemplateEditorFrame::STATE_VECTOR_OBJECTS ) );
		CInput3DTabWindow* pInput3DTabWindowO = dynamic_cast<CInput3DTabWindow*>( GetInputStateWindow( CTemplateEditorFrame::STATE_SIMPLE_OBJECTS ) );

		NI_ASSERT_T( ( pInput3DTabWindowVO != 0 ) && ( pInput3DTabWindowO != 0 ),
								 NStr::Format( "Bag cast!\n" ) );

		int nActiveTab = CInputStateParameter::INVALID_STATE;
		if ( inputShortcutBar.GetActiveIndex() == 1 )
		{
			if ( !pInput3DTabWindowO->GetActiveTab( nActiveTab ) )
			{
				nActiveTab = CInputStateParameter::INVALID_STATE;
			}
		}
		else
		{
			if ( !pInput3DTabWindowVO->GetActiveTab( nActiveTab ) )
			{
				nActiveTab = CInputStateParameter::INVALID_STATE;
			}
		}
		NI_ASSERT_T( nActiveTab != CInputStateParameter::INVALID_STATE,
								 NStr::Format( "Bad input state window number!\n" ) );

		return nActiveTab;
	}

	int GetTerrainState() 
	{
		CInput3DTabWindow* pInput3DTabWindow = dynamic_cast<CInput3DTabWindow*>( GetInputStateWindow( CTemplateEditorFrame::STATE_TERRAIN ) );

		NI_ASSERT_T( pInput3DTabWindow != 0,
								 NStr::Format( "Bag cast!\n" ) );

		int nActiveTab = CInputStateParameter::INVALID_STATE;
		if ( !pInput3DTabWindow->GetActiveTab( nActiveTab ) )
		{
			nActiveTab = CInputStateParameter::INVALID_STATE;
		}
		
		NI_ASSERT_T( nActiveTab != CInputStateParameter::INVALID_STATE,
								 NStr::Format( "Bad input state window number!\n" ) );

		return nActiveTab;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	CTabTileEditDialog* GetTabTileEditDialog() 
	{
		CTabTileEditDialog *pTabTileEditDialog = dynamic_cast<CTabTileEditDialog*>( GetStateTerrainWindowTab( CTemplateEditorFrame::STATE_TERRAIN_TILES ) );

		NI_ASSERT_T( pTabTileEditDialog != 0,
								 NStr::Format( "Bag cast!\n" ) );

		return pTabTileEditDialog;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	CShadeEditorWnd* GetShade() 
	{
		CShadeEditorWnd *pShadeEditorWnd = dynamic_cast<CShadeEditorWnd*>( GetStateTerrainWindowTab( CTemplateEditorFrame::STATE_TERRAIN_ALTITUDES ) );

		NI_ASSERT_T( pShadeEditorWnd != 0,
								 NStr::Format( "Bag cast!\n" ) );

		return pShadeEditorWnd;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	CTabTerrainFieldsDialog* GetTerrainFieldsTab() 
	{
		CTabTerrainFieldsDialog *pTabTerrainFieldsDialog = dynamic_cast<CTabTerrainFieldsDialog*>( GetStateTerrainWindowTab( CTemplateEditorFrame::STATE_TERRAIN_FIELDS ) );

		NI_ASSERT_T( pTabTerrainFieldsDialog != 0,
								 NStr::Format( "Bag cast!\n" ) );

		return pTabTerrainFieldsDialog;
	}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	CTabToolsDialog* GetToolsTab() 
	{
		CTabToolsDialog *pTabToolsDialog = dynamic_cast<CTabToolsDialog*>( GetInputStateWindow( CTemplateEditorFrame::STATE_TOOLS ) );

		NI_ASSERT_T( pTabToolsDialog != 0,
								 NStr::Format( "Bag cast!\n" ) );

		return pTabToolsDialog;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	CGroupManagerDialog* GetGroupMngWnd() 
	{
		CGroupManagerDialog *pGroupManagerDialog = dynamic_cast<CGroupManagerDialog*>( GetInputStateWindow( CTemplateEditorFrame::STATE_GROUPS ) );

		NI_ASSERT_T( pGroupManagerDialog != 0,
								 NStr::Format( "Bag cast!\n" ) );

		return pGroupManagerDialog;
	}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	CTabAIGeneralDialog* GetAIGeneralTab() 
	{
		CTabAIGeneralDialog *pTabAIGeneralDialog = dynamic_cast<CTabAIGeneralDialog*>( GetInputStateWindow( CTemplateEditorFrame::STATE_AI_GENERAL ) );

		NI_ASSERT_T( pTabAIGeneralDialog != 0,
								 NStr::Format( "Bag cast!\n" ) );

		return pTabAIGeneralDialog;
	}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	CTabSimpleObjectsDialog* GetObjectWnd() 
	{
		CTabSimpleObjectsDialog *pTabSimpleObjectsDialog = dynamic_cast<CTabSimpleObjectsDialog*>( GetStateObjectsWindowTab( CTemplateEditorFrame::STATE_SO_OBJECTS ) );

		NI_ASSERT_T( pTabSimpleObjectsDialog != 0,
								 NStr::Format( "Bag cast!\n" ) );

		return pTabSimpleObjectsDialog;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	CFenceSetupWindow* GetFenceWnd() 
	{
		CFenceSetupWindow *pFenceSetupWindow = dynamic_cast<CFenceSetupWindow*>( GetStateObjectsWindowTab( CTemplateEditorFrame::STATE_SO_FENCES ) );

		NI_ASSERT_T( pFenceSetupWindow != 0,
								 NStr::Format( "Bag cast!\n" ) );

		return pFenceSetupWindow;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	CBridgeSetupDialog* GetBridgeWnd() 
	{
		CBridgeSetupDialog *pBridgeSetupDialog = dynamic_cast<CBridgeSetupDialog*>( GetStateObjectsWindowTab( CTemplateEditorFrame::STATE_SO_BRIDGES ) );

		NI_ASSERT_T( pBridgeSetupDialog != 0,
								 NStr::Format( "Bag cast!\n" ) );

		return pBridgeSetupDialog;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	CTrenchSetupWindow* GetTrenchSetupWindow() 
	{
		CTrenchSetupWindow *pTrenchSetupWindow = dynamic_cast<CTrenchSetupWindow*>( GetStateVOWindowTab( CTemplateEditorFrame::STATE_VO_ENTRENCHMENTS ) );

		NI_ASSERT_T( pTrenchSetupWindow != 0,
								 NStr::Format( "Bag cast!\n" ) );

		return pTrenchSetupWindow;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	CTabVOVSODialog* GetRiversTab()
	{
		CTabVOVSODialog *pTabVOVSODialog = dynamic_cast<CTabVOVSODialog*>( GetStateVOWindowTab( CTemplateEditorFrame::STATE_VO_RIVERS ) );

		NI_ASSERT_T( pTabVOVSODialog != 0,
								 NStr::Format( "Bag cast!\n" ) );

		return pTabVOVSODialog;
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	CTabVOVSODialog* GetRoads3DTab()
	{
		CTabVOVSODialog *pTabVOVSODialog = dynamic_cast<CTabVOVSODialog*>( GetStateVOWindowTab( CTemplateEditorFrame::STATE_VO_ROADS3D ) );

		NI_ASSERT_T( pTabVOVSODialog != 0,
								 NStr::Format( "Bag cast!\n" ) );

		return pTabVOVSODialog;
	}
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
#endif // !defined(__INPUT_CONTROL_BAR__)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// basement storage  
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
