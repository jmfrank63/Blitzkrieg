#ifndef __SQUADVIEW_H__
#define __SQUADVIEW_H__



class CSquadView : public CWnd
{
public:
	CSquadView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	
public:
	virtual ~CSquadView();
	
private:
	
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};


#endif		//__SQUADVIEW_H__
