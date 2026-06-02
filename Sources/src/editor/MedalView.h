#ifndef __MEDALVIEW_H__
#define __MEDALVIEW_H__


class CMedalView : public CWnd
{
public:
	CMedalView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	
public:
	virtual ~CMedalView();
	
private:
	
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};


#endif		//__MEDALVIEW_H__
