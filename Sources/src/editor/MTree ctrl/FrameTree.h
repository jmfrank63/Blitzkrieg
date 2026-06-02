#if !defined(AFX_FRAMETREE_H__C3164C06_6784_4F26_A565_BCA2A98A0600__INCLUDED_)
#define AFX_FRAMETREE_H__C3164C06_6784_4F26_A565_BCA2A98A0600__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "MultiTree.h"

class CFrameTree : public CWnd
{
public:
	static const int SCROLL_BAR_SIZE;
	CFrameTree();

public:
	CMultiTree m_tree;
	bool ifInit;
	bool bCreateControls;
public:

	BOOL SetItemData(HTREEITEM hItem, DWORD dwData);
	DWORD GetItemData(HTREEITEM hItem) const;

	HTREEITEM InsertItem( LPCTSTR lpszItem, int nImage, int nSelectedImage, HTREEITEM hParent = TVI_ROOT, HTREEITEM hInsertAfter = TVI_LAST);
	HTREEITEM InsertItem(LPCTSTR lpszItem, HTREEITEM hParent = TVI_ROOT, HTREEITEM hInsertAfter = TVI_LAST );
	HTREEITEM InsertItem(UINT nMask, LPCTSTR lpszItem, int nImage, int nSelectedImage, UINT nState, UINT nStateMask, LPARAM lParam, HTREEITEM hParent, HTREEITEM hInsertAfter );

	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);

public:
	void Init();
	virtual ~CFrameTree();

protected:

	CBrush* m_pEditBkBrush;

	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg HBRUSH OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor );

	DECLARE_MESSAGE_MAP()
};



#endif // !defined(AFX_FRAMETREE_H__C3164C06_6784_4F26_A565_BCA2A98A0600__INCLUDED_)
