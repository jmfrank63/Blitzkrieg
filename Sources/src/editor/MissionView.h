#ifndef __MISSIONVIEW_H__
#define __MISSIONVIEW_H__


class CMissionView : public CWnd
{
public:
	CMissionView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	
public:
	virtual ~CMissionView();
	
private:
	
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};


#endif		//__MISSIONVIEW_H__
