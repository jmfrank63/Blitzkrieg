#ifndef __3DRIVER_VIEW_H__
#define __3DRIVER_VIEW_H__


class C3DRiverView : public CWnd
{
public:
	C3DRiverView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	
public:
	virtual ~C3DRiverView();
	
private:
	
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};


#endif		//__3DRIVER_VIEW_H__
