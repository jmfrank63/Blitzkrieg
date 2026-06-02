#if !defined(AFX_TREEITEMCOMBOBOX_H__06310D0F_1286_49F6_8EE2_F65ED36E3FC1__INCLUDED_)
#define AFX_TREEITEMCOMBOBOX_H__06310D0F_1286_49F6_8EE2_F65ED36E3FC1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


class CTreeItemComboBox : public CComboBox
{
public:
	CTreeItemComboBox();

public:

public:

		virtual BOOL PreTranslateMessage(MSG* pMsg);

public:
	virtual ~CTreeItemComboBox();

protected:
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	

	DECLARE_MESSAGE_MAP()
};



#endif // !defined(AFX_TREEITEMCOMBOBOX_H__06310D0F_1286_49F6_8EE2_F65ED36E3FC1__INCLUDED_)
