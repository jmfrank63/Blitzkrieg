#if !defined(__ELK_MAIN_FRAME__)
#define __ELK_MAIN_FRAME__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ELK_TreeWindow.h"
#include "InputViewWindow.h"
#include "ELK_Types.h"
#include "Messages.h"
#include "SpellChecker.h"

class CMainFrame : public SECFrameWnd
{
protected: 
	DECLARE_DYNAMIC(CMainFrame)

	UINT*	pDefButtonGroup;	// toolbar default button group
	UINT	nDefButtonCount;	// the number of elements in m_pDefaultButtons
	
	HICON hIcon;

	SECStatusBar wndStatusBar;
	CELKTreeWindow wndBaseTree;
	CInputViewWindow wndInputView;
	CFindReplaceDialog *pwndFindReplaceDialog;

	bool bShortApperence;
	bool bGameExists;
	SMainFrameParams params;
	CELK elk;
	CSpellChecker spellChecker;

	CComboBox *pwndFiltersComboBox;
	void FillFiltersComboBox();


	afx_msg int OnCreate( LPCREATESTRUCT lpCreateStruct );
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnToolsCustomize();
	afx_msg void OnClose();
	afx_msg void OnViewStatistic();
	afx_msg void OnUpdateViewStatistic(CCmdUI* pCmdUI);
	afx_msg void OnViewTree();
	afx_msg void OnUpdateViewTree(CCmdUI* pCmdUI);
	afx_msg void OnSetFocus(CWnd *pOldWnd);
	afx_msg void OnImportFromGame();
	afx_msg void OnUpdateImportFromGame(CCmdUI* pCmdUI);
	afx_msg void OnImportFromPak();
	afx_msg void OnUpdateImportFromPak(CCmdUI* pCmdUI);
	afx_msg void OnUpdateRecentElk(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFileOpen(CCmdUI* pCmdUI);
	afx_msg void OnFileOpen();
	afx_msg void OnUpdateFileClose(CCmdUI* pCmdUI);
	afx_msg void OnFileClose();
	afx_msg void OnExportToExcel();
	afx_msg void OnUpdateExportToExcel(CCmdUI* pCmdUI);
	afx_msg void OnUpdateBackPageButton(CCmdUI* pCmdUI);
	afx_msg void OnBackPageButton();
	afx_msg void OnNextPageButton();
	afx_msg void OnUpdateNextPageButton(CCmdUI* pCmdUI);
	afx_msg void OnUpdateNextButton(CCmdUI* pCmdUI);
	afx_msg void OnNextButton();
	afx_msg void OnBackButton();
	afx_msg void OnUpdateBackButton(CCmdUI* pCmdUI);
	afx_msg void OnImportFromExcel();
	afx_msg void OnUpdateImportFromExcel(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFileSave(CCmdUI* pCmdUI);
	afx_msg void OnFileSave();
	afx_msg void OnUpdateExportToPack(CCmdUI* pCmdUI);
	afx_msg void OnExportToPack();
	afx_msg void OnUpdateExportToPackByFilter(CCmdUI* pCmdUI);
	afx_msg void OnExportToPackByFilter();
	afx_msg void OnBrowseCollapseItem();
	afx_msg void OnUpdateBrowseCollapseItem(CCmdUI* pCmdUI);
	afx_msg void OnBrowseSkipOptions();
	afx_msg void OnUpdateBrowseSkipOptions(CCmdUI* pCmdUI);
	afx_msg void OnUpdateEditPaste(CCmdUI* pCmdUI);
	afx_msg void OnEditPaste();
	afx_msg void OnUpdateEditUndo(CCmdUI* pCmdUI);
	afx_msg void OnEditUndo();
	afx_msg void OnUpdateEditSelectAll(CCmdUI* pCmdUI);
	afx_msg void OnEditSelectAll();
	afx_msg void OnUpdateEditClear(CCmdUI* pCmdUI);
	afx_msg void OnEditClear();
	afx_msg void OnUpdateEditCopy(CCmdUI* pCmdUI);
	afx_msg void OnEditCopy();
	afx_msg void OnUpdateEditCut(CCmdUI* pCmdUI);
	afx_msg void OnEditCut();
	afx_msg void OnEditFind();
	afx_msg void OnUpdateEditFind(CCmdUI* pCmdUI);
	afx_msg void OnToolsRunGame();
	afx_msg void OnUpdateToolsRunGame(CCmdUI* pCmdUI);
	afx_msg void OnCloseButton();
	afx_msg void OnUpdateClose(CCmdUI* pCmdUI);
	afx_msg void OnUpdateToolsSpelling(CCmdUI* pCmdUI);
	afx_msg void OnToolsSpelling();
	afx_msg void OnUpdateHelpContents(CCmdUI* pCmdUI);
	afx_msg void OnHelpContents();
	afx_msg void OnFileDelete();
	afx_msg void OnUpdateFileDelete(CCmdUI* pCmdUI);
	afx_msg void OnToolsChooseFons();
	afx_msg void OnUpdateToolsChooseFons(CCmdUI* pCmdUI);
	afx_msg void OnRecentElk( UINT nID );
	afx_msg void OnUpdateRecentElkRange( CCmdUI* pCmdUI );
  afx_msg LONG OnFindReplace( WPARAM wParam, LPARAM lParam );

	afx_msg LRESULT OnCreateCombo( WPARAM wParam, LPARAM lParam );
	afx_msg void OnChangeFilter();
	DECLARE_MESSAGE_MAP()


	int OnETNTextSelected( int nState );
	int OnETNFolderSelected( int nState );
	int OnIFNStateChanged( int nState );
	int OnTENKeyDown( UINT nChar );

	void OnExportToPAKInternal( bool bUseFilter = false );
	void LayoutClientWindows();
public:
	CMainFrame();
	virtual ~CMainFrame();

	bool CloseELK();
	bool OpenELK( const std::string &rszELKFileName );
	bool OpenLastELK();


	bool CheckGameApp( LPCSTR pszMainClass, LPCSTR pszMainTitle );
	void AddToRecentList( const std::string &rszELKFileName );
	void RemoveFromRecentList( const std::string &rszELKFileName );
	void UpdateRecentList();

	void RunExternalHelpFile( const std::string &rszHelpFilePath );

	public:
	virtual BOOL PreCreateWindow( CREATESTRUCT& cs );
	protected:
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

/**
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump( CDumpContext& dc ) const;
#endif
/**/
};
#endif // !defined(__ELK_MAIN_FRAME__)


