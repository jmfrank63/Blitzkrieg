#ifndef __GUIVIEW_H__
#define __GUIVIEW_H__

class CGUIView : public CWnd
{
public:
	CGUIView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	
public:
	virtual ~CGUIView();
	
private:
	
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};


#endif		//__GUIVIEW_H__
