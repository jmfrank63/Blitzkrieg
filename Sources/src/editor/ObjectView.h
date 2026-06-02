#ifndef __OBJECTVIEW_H__
#define __OBJECTVIEW_H__



class CObjectView : public CWnd
{
public:
	CObjectView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	
public:
	virtual ~CObjectView();
	
private:
	
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};


#endif		//__OBJECTVIEW_H__
