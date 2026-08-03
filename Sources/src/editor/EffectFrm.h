
#ifndef __EFFECTFRM_H__
#define __EFFECTFRM_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..//Common//LegacyUiCompat.h"
#include "../GFX/GFX.H"
#include "../Scene/Scene.h"

#include "ParentFrame.h"
#include "TreeDockWnd.h"

class CDirectionButtonDockBar;
class CEffectFrame : public CParentFrame
{
	DECLARE_DYNCREATE(CEffectFrame)
public:
	CEffectFrame();
	virtual ~CEffectFrame();
	
public:
	
public:
	virtual void GFXDraw();
	virtual void ShowFrameWindows( int nCommand );
	
	BOOL Run();										//���������� �� EditorApp OnIdle()
	bool IsRunning() { return bRunning; }
	
	void SetDirectionButtonDockBar( CDirectionButtonDockBar *pDock ) { pDirectionButtonDockBar = pDock; }
	int GetLastParticleEffectLifeTime();
	
protected:
	
private:
	bool bRunning;								//���� ��� ���������, �������������� � ��������������� ��������
	CDirectionButtonDockBar *pDirectionButtonDockBar;
	CPtr<IEffectVisObj> pRunningEffect;
	bool bHorizontalCamera;
	
protected:
	void UpdateEffectAngle();
	void UpdateCamera();
	
	virtual BOOL SpecificTranslateMessage( MSG *pMsg );
	
	virtual void SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName );

	virtual bool ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem );
	virtual FILETIME FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem );
	
	virtual void SpecificInit();
	virtual void SpecificClearBeforeBatchMode();
	
protected:
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
	DECLARE_MESSAGE_MAP()
};



#endif		//__EFFECTFRM_H__

