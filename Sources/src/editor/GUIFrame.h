#ifndef __GUIFRAME_H__
#define __GUIFRAME_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\\Common\\StingrayCompat.h"
#include "..\UI\UI.h"
#include "..\GFX\GFX.h"
#include "..\Input\Input.h"
#include "..\Input\InputHelper.h"
#include "ParentFrame.h"
#include "GUIView.h"
#include "GUIUndo.h"

class CTemplatePropsTreeItem;
interface IObjVisObj;
class CPropertyDockBar;

class CGUIFrame : public CParentFrame
{
	DECLARE_DYNCREATE(CGUIFrame)
public:
	CGUIFrame();
	virtual ~CGUIFrame();

// Attributes
public:
	typedef vector< CPtr<IGFXVertices> > CVectorOfVertices;

// Operations
public:
	virtual void Init( IGFX *_pGFX );			//��������������
	virtual void ShowFrameWindows( int nCommand );
	virtual void GFXDraw();

	BOOL Run();										//���������� �� EditorApp OnIdle()
	bool IsRunning() { return bRunning; }

	void LoadSprites();
	int DisplayInsertMenu();

	void SetPropertyDockBar( CPropertyDockBar *pWnd ) { pPropertyDockBar = pWnd; }
	void SetActiveTemplatePropsItem( CTemplatePropsTreeItem *pItem ) { pTemplatePropsItem = pItem; }
	IUIElement *GUICreateElement();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGUIFrame)
	protected:
	//}}AFX_VIRTUAL

// Implementation
private:
	// view for the client area of the frame.
	CPropertyDockBar *pPropertyDockBar;
	CTemplatePropsTreeItem *pTemplatePropsItem;
	bool bRunning;								//���� ��� ���������, �������������� � ��������������� ��������
	
	CObj<IUIScreen> m_pScreen;
	CPtr<IUIContainer> m_pContainer;
	CPtr<IUIElement> m_pHigh;
	typedef list< CPtr<IUIElement> > CWindowList;
	CWindowList m_selectedList;
	typedef list< CObj<IUIElement> > CCopyWindowList;
	CCopyWindowList m_copiedList;

	enum EMode
	{
		MODE_FREE,
		MODE_SELECT,
		MODE_RESIZE,
		MODE_DRAG,
		MODE_DRAW
	};
	enum EMode m_mode;						//������� ���, ���������� �������� �������� �����, �������� �������������� ������� ��� ��������� ������

	CVec2 m_beginDrag;

	enum EResizeMode
	{
		R_NORESIZE,
		R_LEFT,
		R_TOP,
		R_RIGHT,
		R_BOTTOM,
		R_LEFT_TOP,
		R_RIGHT_TOP,
		R_RIGHT_BOTTOM,
		R_LEFT_BOTTOM
	};
	EResizeMode m_resizeMode;			//��� ����������� ����������� resize

	// undo/redo operations
	typedef list< CPtr<IGUIUndo> > CUndoStack;
	CUndoStack m_undoStack;
	CPtr<IGUIUndo> pUnchanged;

	//��� Run Mode
//	CVec2 vCursorPos;
	int mouseState;
	NInput::CCommandRegistrator standardMsgs;
	
protected:
	virtual BOOL SpecificTranslateMessage( MSG *pMsg );			//����������� ��������� ��������� ��� ������
	virtual void SpecificInit();														//��� ������������� ���������� ������ ����� �������� ������� ��� �������� ������
	virtual void SpecificClearBeforeBatchMode();
	virtual void SpecificSave( IDataTree *pDT );						//���������� ��� ������ �������, ����� ������ � GUI composer

	//������������ ���� ������, ���� ��� ��, ���������� 0, ����� ��� ������
	virtual bool ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem );
//	virtual FILETIME FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem );
//	virtual FILETIME FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem );
	
	//GUI Module
	CTRect<float> GetElementRect( IUIElement *pElement );
	void SetElementRect( IUIElement *pElement, const CTRect<float> &rc );
	void GFXDrawFrame( const CTRect<float> &rc, DWORD color, float width );
	void GFXDrawFilledRect( const CTRect<float> &rc, DWORD color );
	EResizeMode GetResizeMode( IUIElement *pElement, float x, float y );

	const char *GetDirectoryFromWindowType( int nWindowType );
	CTreeItem *GetParentTreeItemForWindowType( int nWindowType );

	// Generated message map functions
protected:
	//{{AFX_MSG(CGUIFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnRunButton();
	afx_msg void OnStopButton();
	afx_msg void OnUpdateStopButton(CCmdUI* pCmdUI);
	afx_msg void OnUpdateRunButton(CCmdUI* pCmdUI);
	afx_msg void OnUpdateInsertTreeItem(CCmdUI* pCmdUI);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnCreatenewtemplate();
	afx_msg void OnTestbutton();
	afx_msg void OnEditCopy();
	afx_msg void OnUpdateEditCopy(CCmdUI* pCmdUI);
	afx_msg void OnEditPaste();
	afx_msg void OnUpdateEditPaste(CCmdUI* pCmdUI);
	afx_msg void OnEditCut();
	afx_msg void OnUpdateEditCut(CCmdUI* pCmdUI);
	afx_msg void OnEditUndo();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif		//__GUIFRAME_H__

