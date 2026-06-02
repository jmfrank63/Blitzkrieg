#ifndef __FENCEVIEW_H__
#define __FENCEVIEW_H__


class CFenceView : public CWnd
{
public:
	CFenceView();
	virtual ~CFenceView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	
private:
	
protected:
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	DECLARE_MESSAGE_MAP()
};


#endif		//__FENCEVIEW_H__
