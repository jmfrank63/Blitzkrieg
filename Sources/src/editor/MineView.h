#ifndef __MINEVIEW_H__
#define __MINEVIEW_H__


class CMineView : public CWnd
{
public:
	CMineView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	
public:
	virtual ~CMineView();
	
private:
	
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};


#endif		//__MINEVIEW_H__
