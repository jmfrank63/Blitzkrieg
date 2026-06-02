#ifndef __PARTICLE_VIEW_H__
#define __PARTICLE_VIEW_H__


class CParticleView : public CWnd
{
public:
	CParticleView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	
public:
	virtual ~CParticleView();
	
private:
	
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};


#endif		//__PARTICLE_VIEW_H__
