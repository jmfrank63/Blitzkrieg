#ifndef __CAMPAIGNVIEW_H__
#define __CAMPAIGNVIEW_H__


class CCampaignView : public CWnd
{
public:
	CCampaignView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	
public:
	virtual ~CCampaignView();
	
private:
	void UpdateScrolls( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar );
	
protected:
	afx_msg void OnPaint();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	DECLARE_MESSAGE_MAP()
};


#endif		//__CAMPAIGNVIEW_H__
