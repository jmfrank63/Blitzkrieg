#ifndef __BUILDVIEW_H__
#define __BUILDVIEW_H__



class CBuildingView : public CWnd
{
public:
	CBuildingView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	
public:
	virtual ~CBuildingView();
	
private:
	
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};


#endif		//__BUILDVIEW_H__
