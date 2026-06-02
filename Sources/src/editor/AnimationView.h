#if !defined(AFX_ANIMATIONVIEW_H__FD350F35_C8FF_499B_8B91_C590E4BDEEE0__INCLUDED_)
#define AFX_ANIMATIONVIEW_H__FD350F35_C8FF_499B_8B91_C590E4BDEEE0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000



class CAnimationView : public CWnd
{
public:
	CAnimationView();

public:

public:

	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

public:
	virtual ~CAnimationView();

private:

protected:
	afx_msg void OnPaint();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	DECLARE_MESSAGE_MAP()
};



#endif // !defined(AFX_ANIMATIONVIEW_H__FD350F35_C8FF_499B_8B91_C590E4BDEEE0__INCLUDED_)
