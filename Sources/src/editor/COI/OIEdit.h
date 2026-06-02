#if !defined(AFX_OIEDIT_H__257BD5B4_F0D5_4982_AB55_368BE38F3314__INCLUDED_)
#define AFX_OIEDIT_H__257BD5B4_F0D5_4982_AB55_368BE38F3314__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

const UINT WM_USER_LOST_FOCUS = WM_USER + 2;

class COIEdit : public CEdit
{
public:
	COIEdit();

public:

public:


public:
	virtual ~COIEdit();

protected:
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

	DECLARE_MESSAGE_MAP()
};



#endif // !defined(AFX_OIEDIT_H__257BD5B4_F0D5_4982_AB55_368BE38F3314__INCLUDED_)
