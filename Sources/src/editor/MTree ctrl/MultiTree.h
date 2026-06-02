#if !defined(AFX_MULTITREE_H__C215D318_F9F4_40AD_B30F_81CADE81DAA5__INCLUDED_)
#define AFX_MULTITREE_H__C215D318_F9F4_40AD_B30F_81CADE81DAA5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#include "CTreeItem.h"
#include "MultiTreeEditBox.h"
enum TREEITEMTYPES
{
	simpleTreeItem,
	truefalseTreeItem,
	procentTreeItem,
	numComboBoxItem,
	emptyItem,
	propertieItem,
	propertieItemCombo,
	propertieItemDir,
	propertieItemFile,
	propertieItemUnits,
};
class CMultiTree : public CTreeCtrl
{
public:
	CMultiTree();
	CHeaderCtrl m_wndHeader;
public:
	HTREEITEM InsertItem( LPCTSTR lpszItem, int nImage, int nSelectedImage, HTREEITEM hParent = TVI_ROOT, HTREEITEM hInsertAfter = TVI_LAST);
	HTREEITEM InsertItem(LPCTSTR lpszItem, HTREEITEM hParent = TVI_ROOT, HTREEITEM hInsertAfter = TVI_LAST );
	HTREEITEM InsertItem(UINT nMask, LPCTSTR lpszItem, int nImage, int nSelectedImage, UINT nState, UINT nStateMask, LPARAM lParam, HTREEITEM hParent, HTREEITEM hInsertAfter );
	
	BOOL SetItemData(HTREEITEM hItem, DWORD dwData);
	DWORD GetItemData(HTREEITEM hItem) const;

	ITreeItem* GetTreeItemPtr(HTREEITEM hItem);

	int GetColumnWidth(int nCol);

	HTREEITEM InsertItemEx(LPCTSTR lpszItem, HTREEITEM hParent = TVI_ROOT, HTREEITEM hInsertAfter = TVI_LAST ,TREEITEMTYPES type = truefalseTreeItem );

	void DrawItemText (CDC* pDC, CString &text, CRect &rect, int nWidth, int nFormat);

public:

	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual LRESULT DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam);

public:
	int GetFullWidth();
	virtual ~CMultiTree();
	void SafeDeleteItem( HTREEITEM item );
	void SafeDeleteAllItems();
	
private:
	CMultiTreeEditBox *m_editCtl;
	HTREEITEM m_editedItem;

protected:
	afx_msg void OnPaint();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
		afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);

	DECLARE_MESSAGE_MAP()
};



#endif // !defined(AFX_MULTITREE_H__C215D318_F9F4_40AD_B30F_81CADE81DAA5__INCLUDED_)
