#ifndef __EFFECTVIEW_H__
#define __EFFECTVIEW_H__



class CEffectView : public CWnd
{
public:
	CEffectView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	
public:
	virtual ~CEffectView();
	
private:
	
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};


#endif		//__EFFECTVIEW_H__
