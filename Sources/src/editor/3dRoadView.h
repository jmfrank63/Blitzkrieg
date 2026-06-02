#ifndef __3DROAD_VIEW_H__
#define __3DROAD_VIEW_H__


class C3DRoadView : public CWnd
{
public:
	C3DRoadView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	
public:
	virtual ~C3DRoadView();
	
private:
	
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};


#endif		//__3DROAD_VIEW_H__
