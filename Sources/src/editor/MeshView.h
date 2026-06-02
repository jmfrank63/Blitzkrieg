#ifndef __MESHVIEW_H__
#define __MESHVIEW_H__



class CMeshView : public CWnd
{
public:
	CMeshView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	
public:
	virtual ~CMeshView();
	
private:
	
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};


#endif		//__MESHVIEW_H__
