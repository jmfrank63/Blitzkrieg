#if !defined(AFX_DIRECTIONBUTTON_H__3B7C3659_438D_48C9_9175_85ADA378ECA9__INCLUDED_)
#define AFX_DIRECTIONBUTTON_H__3B7C3659_438D_48C9_9175_85ADA378ECA9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

const UINT WM_ANGLE_CHANGED = WM_USER + 20;
#define IDC_DIRECTION_BUTTON 1020


class CDirectionButton : public CWnd
{ 
public:
	CDirectionButton();

public:

public:
	float GetAngle() { return fAngle; }
	void SetAngle( float fVal ) { fAngle = fVal; Invalidate(); }
	int GetQuadrant();			//возвращает один из восьми квадрантов в котором находится текущий угол


public:
	virtual ~CDirectionButton();

private:
	float fAngle;

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnPaint();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnDestroy();

	DECLARE_MESSAGE_MAP()
};



#endif // !defined(AFX_DIRECTIONBUTTON_H__3B7C3659_438D_48C9_9175_85ADA378ECA9__INCLUDED_)
