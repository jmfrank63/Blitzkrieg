#if !defined(AFX_MULTITREEEDITBOX_H__FC15D22B_F5FA_44C1_8140_39689B14A0B1__INCLUDED_)
#define AFX_MULTITREEEDITBOX_H__FC15D22B_F5FA_44C1_8140_39689B14A0B1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


class CMultiTreeEditBox : public CEdit
{
public:
	CMultiTreeEditBox();

public:

public:

	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);

public:
	virtual ~CMultiTreeEditBox();

protected:
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);

	DECLARE_MESSAGE_MAP()
};



#endif // !defined(AFX_MULTITREEEDITBOX_H__FC15D22B_F5FA_44C1_8140_39689B14A0B1__INCLUDED_)
