// EffectFrm.h : interface of the CEffectFrame class
//
/////////////////////////////////////////////////////////////////////////////

#ifndef __EFFECTFRM_H__
#define __EFFECTFRM_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\\Common\\StingrayCompat.h"
#include "..\GFX\GFX.h"
#include "..\Scene\Scene.h"

#include "ParentFrame.h"
#include "TreeDockWnd.h"

class CDirectionButtonDockBar;
class CEffectFrame : public CParentFrame
{
	DECLARE_DYNCREATE(CEffectFrame)
public:
	CEffectFrame();
	virtual ~CEffectFrame();
	
	// Attributes
public:
	
	// Operations
public:
	virtual void GFXDraw();
	virtual void ShowFrameWindows( int nCommand );
	
	BOOL Run();										//���������� �� EditorApp OnIdle()
	bool IsRunning() { return bRunning; }
	
	void SetDirectionButtonDockBar( CDirectionButtonDockBar *pDock ) { pDirectionButtonDockBar = pDock; }
	int GetLastParticleEffectLifeTime();
	
	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEffectFrame)
protected:
	//}}AFX_VIRTUAL
	
	// Implementation
private:
	bool bRunning;								//���� ��� ���������, �������������� � ��������������� ��������
	CDirectionButtonDockBar *pDirectionButtonDockBar;
	CPtr<IEffectVisObj> pRunningEffect;
	bool bHorizontalCamera;
	
protected:
	void UpdateEffectAngle();
	void UpdateCamera();
	
	virtual BOOL SpecificTranslateMessage( MSG *pMsg );
//	virtual void SpecificInit();
	
	virtual void SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName );
	//virtual void LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem );

	virtual bool ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem );
//	virtual FILETIME FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem );
	virtual FILETIME FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem );
	
	virtual void SpecificInit();
	virtual void SpecificClearBeforeBatchMode();
	
// Generated message map functions
protected:
	//{{AFX_MSG(CEffectFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnRunButton();
	afx_msg void OnStopButton();
	afx_msg void OnUpdateStopButton(CCmdUI* pCmdUI);
	afx_msg void OnUpdateRunButton(CCmdUI* pCmdUI);
	afx_msg void OnUpdateInterpolateTreeItem(CCmdUI* pCmdUI);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnButtonCamera();
	afx_msg void OnUpdateButtonCamera(CCmdUI* pCmdUI);
	afx_msg void OnShowDirectionButton();	
	afx_msg void OnUpdateShowDirectionButton(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif		//__EFFECTFRM_H__

