#ifndef __TILESETVIEW_H__
#define __TILESETVIEW_H__



class CTileSetView : public CWnd
{
public:
	CTileSetView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	
public:
	virtual ~CTileSetView();
	
private:
	
protected:
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	DECLARE_MESSAGE_MAP()
};


#endif		//__TILESETVIEW_H__
