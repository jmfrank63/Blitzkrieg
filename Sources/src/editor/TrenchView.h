#ifndef __TRENCHVIEW_H__
#define __TRENCHVIEW_H__


class CTrenchView : public CWnd
{
public:
	CTrenchView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	
public:
	virtual ~CTrenchView();
	
private:
	
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};


#endif		//__TRENCHVIEW_H__
