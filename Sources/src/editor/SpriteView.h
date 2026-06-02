#ifndef __SPRITEVIEW_H__
#define __SPRITEVIEW_H__



class CSpriteView : public CWnd
{
public:
	CSpriteView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	
public:
	virtual ~CSpriteView();
	
private:
	
protected:
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	DECLARE_MESSAGE_MAP()
};


#endif		//__SPRITEVIEW_H__
