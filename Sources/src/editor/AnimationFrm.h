#ifndef __ANIMATIONFRM_H__
#define __ANIMATIONFRM_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\\Common\\StingrayCompat.h"
#include "..\Main\rpgstats.h"
#include "..\Scene\scene.h"

#include "ParentFrame.h"
#include "AnimTreeItem.h"
#include "ThumbListDockBar.h"
#include "AnimationView.h"

class CUnitAnimationPropsItem;
class CDirectoryPropsItem;

class CAnimationFrame : public CParentFrame
{
	DECLARE_DYNCREATE(CAnimationFrame)
public:
	CAnimationFrame();
	virtual ~CAnimationFrame();
	
// Attributes
public:

// Operations
public:
	virtual void ShowFrameWindows( int nCommand );
	virtual void GFXDraw();
	
	BOOL Run();																			//���������� �� EditorApp OnIdle()
	bool IsRunning() { return bRunning; }

	void ClearComposedFlag() { bComposed = false; }
	void ViewSizeChanged();
	void UpdateThumbWindows() { m_wndAllDirThumbItems.Invalidate(); m_wndSelectedThumbItems.Invalidate(); }
	void UpdateUnitsCoordinates();

	void SetActiveDirTreeItem( CDirectoryPropsItem *pDirPropsItem );
	void SetActiveAnimItem( CUnitAnimationPropsItem *pAnimation );
	void ActiveDirNameChanged();

	void SelectItemInSelectedThumbList( DWORD dwData );
	void DeleteFrameInSelectedList( DWORD dwData );

	int DisplayAcksMenu();
	
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAnimationFrame)
	protected:
//	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
private:
	// view for the client area of the frame.
	CDirectoryPropsItem *m_pActiveDirTreeItem;
	CUnitAnimationPropsItem *m_pActiveAnimation;

	CThumbList m_wndAllDirThumbItems;
	CThumbList m_wndSelectedThumbItems;
	CScrollBar m_wndScrollBar;

	bool bRunning;								//���� ��� ���������, �������������� � ��������������� ��������
	bool bComposed;
	bool bExportOnlyRPGStats;			//��� �������� ������ rpg stats, �� �������� �������� ��������
	
	//��� ���������� �������� � ������������ ����������
	struct SUnitObject
	{
		CVec3 vPos;
		CPtr<IObjVisObj> pUnit;
	};
	std::vector<SUnitObject> units;

protected:
	bool ComposeAnimations();
	void ClickOnThumbList( int nID );
	void DoubleClickOnThumbList( int nID );
	void DeleteFrameInTree( int nID );
	void InitDirNames();
	
	virtual BOOL SpecificTranslateMessage( MSG *pMsg );
	virtual void SpecificInit();
	virtual void SpecificClearBeforeBatchMode();
	
	virtual void SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName );
	virtual void LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem );
	void FillRPGStats( SInfantryRPGStats &rpgStats, CTreeItem *pRootItem );
	void GetRPGStats( const SInfantryRPGStats &rpgStats, CTreeItem *pRootItem );

	virtual bool ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem );
	virtual FILETIME FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem );
	virtual FILETIME FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem );
	
// Generated message map functions
protected:
	//{{AFX_MSG(CAnimationFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnRunButton();
	afx_msg void OnStopButton();
	afx_msg void OnUpdateRunButton(CCmdUI* pCmdUI);
	afx_msg void OnUpdateStopButton(CCmdUI* pCmdUI);
	afx_msg void OnFileExportOnlyRpgStats();
	afx_msg void OnUpdateFileExportOnlyRpgStats(CCmdUI* pCmdUI);
	afx_msg void OnUpdateImportAckFile(CCmdUI* pCmdUI);
	afx_msg void OnUpdateExportAckFile(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // __ANIMATIONFRM_H__

