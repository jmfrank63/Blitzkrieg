
#ifndef __SPRITEFRM_H__
#define __SPRITEFRM_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..//Common//LegacyUiCompat.h"
#include "../GFX/GFX.H"

#include "ParentFrame.h"
#include "SpriteTreeItem.h"
#include "SpriteView.h"
#include "ThumbList.h"


class CSpriteFrame : public CParentFrame
{
	DECLARE_DYNCREATE(CSpriteFrame)
public:
	CSpriteFrame();
	virtual ~CSpriteFrame();

public:

public:
	virtual void GFXDraw();
	virtual void ShowFrameWindows( int nCommand );
	
	BOOL Run();										//���������� �� EditorApp OnIdle()
	bool IsRunning() { return bRunning; }
	void ClearComposedFlag() { bComposed = false; }

	void ViewSizeChanged();
	void UpdateThumbWindows() { m_wndAllDirThumbItems.Invalidate(); m_wndSelectedThumbItems.Invalidate(); }

	void ActiveDirNameChanged();
	void SelectItemInSelectedThumbList( DWORD dwData );
	void DeleteFrameInSelectedList( DWORD dwData );
	
protected:
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

private:
	CSpritesItem *m_pActiveSpritesItem;
	CThumbList m_wndAllDirThumbItems;
	CThumbList m_wndSelectedThumbItems;

	bool bRunning;								//���� ��� ���������, �������������� � ��������������� ��������
	bool bComposed;

protected:
	void ComposeAnimations();
	void ClickOnThumbList( int nID );
	void DoubleClickOnThumbList( int nID );
	void DeleteFrameInTree( int nID );
	void SetActiveSpritesItem( CSpritesItem *pSpritesItem );

	virtual BOOL SpecificTranslateMessage( MSG *pMsg );
	virtual void SpecificInit();													//��� ������������� ���������� ������ ����� �������� ������� ��� �������� ������
	virtual void SpecificClearBeforeBatchMode();
	
	virtual bool ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem );
	virtual FILETIME FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem );
	
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnRunButton();
	afx_msg void OnStopButton();
	afx_msg void OnUpdateStopButton(CCmdUI* pCmdUI);
	afx_msg void OnUpdateRunButton(CCmdUI* pCmdUI);
	DECLARE_MESSAGE_MAP()
};



#endif		//__SPRITEFRM_H__

