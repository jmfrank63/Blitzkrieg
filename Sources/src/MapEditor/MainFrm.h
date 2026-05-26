#if !defined(__EDITOR_MAIN_FRAME__)
#define __EDITOR_MAIN_FRAME__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\\Common\\StingrayCompat.h"
#include "..\GFX\GFX.h"
#include "..\Input\Input.h"
#include "..\Scene\Scene.h"

#include "GameWnd.h"
#include "TabTileEditDialog.h"

#include "MapEditorBarWnd.h"
#include "MiniMapBar.h"


#include "MiniMapDialog.h"
#include "EditorWindowSingleton.h"

extern int GAME_SIZE_X;
extern int GAME_SIZE_Y;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMainFrame : public SECWorkbook
{
	DECLARE_DYNAMIC(CMainFrame)

public:
	CMainFrame();
	~CMainFrame();

	//{{AFX_VIRTUAL(CMainFrame)
	//}}AFX_VIRTUAL


	bool bFixedDimensions;
	std::string szHelpFilePath;
	CGameWnd wndGameWnd;
	CInputControlBar wndInputControlBar;
	CMiniMapBar wndMiniMapBar;
	SECStatusBar m_wndStatusBar;
	int nFireRangeRegisterGroup;
	CComboBox *pwndFireRangeFilterComboBox;
	CComboBox *pwndBrushSizeComboBox;
	CComboBox *pwndPlayerNumberComboBox;
	HACCEL hMDIAccel;
	CEditorWindowSingletonApp editorWindowSingletonApp;
	
	std::list<std::string> recentList;

	afx_msg void OnHelp();

	//void SetMainWindowTitle( const char *pszTitle ) { SetTitle( pszTitle ); }
	//void SetMainWindowText( const char *pszText ) { SetWindowText( pszText ); }

	int InitGameWindow();
	int CreateTemplateEditorFrame();
	void FillRangeFilterComboBox( const std::string &rszCurrentFilter, const TFilterHashMap &rAllFilters, bool bUpdate = true );
	void FillBrushSize();
	void FillPlayerNumbers( const std::string &rszPlayerNumber );
	SECToolBarManager* GetControlBarManager() { return static_cast<SECToolBarManager*>( m_pControlBarManager ); }
	void ShowSECControlBar( SECControlBar *pControlBar, int nCommand );

	//{{AFX_MSG(CMainFrame)
	afx_msg void OnToolsCustomize();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg LRESULT OnCreateCombo( WPARAM wParam, LPARAM lParam );
	afx_msg void OnChangeBrushSize();
	afx_msg void OnChangePlayerNumber();
	afx_msg void OnClose();
	afx_msg void OnUpdateHelp(CCmdUI* pCmdUI);
	afx_msg void OnViewLeftBar();
	afx_msg void OnUpdateViewLeftBar(CCmdUI* pCmdUI);
	afx_msg void OnViewToolbar0();
	afx_msg void OnViewToolbar1();
	afx_msg void OnViewToolbar2();
	afx_msg void OnViewToolbar3();
	afx_msg void OnViewToolbar4();
	afx_msg void OnUpdateViewToolbar0(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewToolbar1(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewToolbar2(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewToolbar3(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewToolbar4(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShowminimap(CCmdUI* pCmdUI);
	afx_msg void OnShowminimap();
	afx_msg void OnTool0();
	afx_msg void OnTool1();
	afx_msg void OnTool2();
	afx_msg void OnTool3();
	afx_msg void OnTool4();
	afx_msg void OnDropFiles( HDROP hDropInfo );
	afx_msg BOOL OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnChangeFireRangeFilter();
	afx_msg void OnRecentMap( UINT nID );
	afx_msg void OnUpdateRecentMap(CCmdUI* pCmdUI);
	afx_msg void OnUpdateRecentMapRange( CCmdUI* pCmdUI );

	void AddToRecentList( const std::string &rszMapFileName );
	void RemoveFromRecentList( const std::string &rszMapFileName );
	void UpdateRecentList();
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
#endif // !defined(__EDITOR_MAIN_FRAME__)

