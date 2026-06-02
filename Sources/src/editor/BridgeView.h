#ifndef __BRIDGEVIEW_H__
#define __BRIDGEVIEW_H__


class CBridgeView : public CWnd
{
public:
	CBridgeView();
	virtual ~CBridgeView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	
private:
	
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};


#endif		//__BRIDGEVIEW_H__
