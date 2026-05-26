#if !defined(AFX_ETREECTRL_H__4C474B32_E849_401D_BCF3_3163E8CF920E__INCLUDED_)
#define AFX_ETREECTRL_H__4C474B32_E849_401D_BCF3_3163E8CF920E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\\Common\\StingrayCompat.h"
class CTreeItem;
//#include "TreeItem.h"

// ETreeCtrl.h : header file
//

/*
// ���������, ���������� ������������� ����
// wParam - tree ctrl ID
// lParam - ID �����
const UINT WM_ME_TREESEL  = WM_USER + 1;       // ���������� ������ ������� (������� ������)
const UINT WM_ME_DROPITEM = WM_USER + 2;       // 

// ����� �������� ����� ��������� ������ ����� �� ������ 
// �� ��������� � �������� ���� lParam
// ���������� ����������� ��� �������� ID �����
const LPARAM FOLD_MODIFIER = 0x80000000;
*/

#define IDC_TREE_CONTROL 1005

// ��� ��������� �� ������
const UINT WM_USERTREESEL				= WM_USER + 1;				// ���������� ������ �������
const UINT WM_USERDROPITEM			= WM_USER + 2;				// Drop ��� �������� ������
const UINT WM_USERKEYDOWN				= WM_USER + 3;				// ������ �������
const UINT WM_USERRBUTTONCLICK	= WM_USER + 4;				// ���� ������ �����


/////////////////////////////////////////////////////////////////////////////
// CETreeCtrl window

class CETreeCtrl : public CWnd
{
// Construction
public:
	CETreeCtrl();

// Attributes
private:
  SECTreeCtrl m_treeCtrl;
	CImageList *m_pDragImageList;
	CImageList m_imlNormal;
	HTREEITEM m_hitemDrop;
	HTREEITEM m_hitemDrag;
	bool m_bDragging;
	
	CPtr<CTreeItem> pRootItem;
//	CDefItemsVector defItems;
// Operations
public:
/*
	void InitImageLists();
	void AddSomeItems();
*/
	SECTreeCtrl* GetTreeCtrl() { return &m_treeCtrl; }
	void LoadImageList( UINT nID );
	CTreeItem *CreateRootItem( int nRootItemId );
	CTreeItem *GetRootItem() { return pRootItem; }

	void SaveTree( IStructureSaver *pSS );
	void LoadTree( IStructureSaver *pSS );
	void SaveTree( IDataTree *pDT );
	void LoadTree( IDataTree *pDT );

protected:
	void DestroySiblingItems(HTREEITEM _handle);
	CTreeItem* GetTreeItem( HTREEITEM hti );


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CETreeCtrl)
public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
protected:
		//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CETreeCtrl();

	// Generated message map functions
protected:
	//{{AFX_MSG(CETreeCtrl)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnBegindrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnItemExpanding(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDestroy();
	afx_msg void OnSelect(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRButtonClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeyDown(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ETREECTRL_H__4C474B32_E849_401D_BCF3_3163E8CF920E__INCLUDED_)

