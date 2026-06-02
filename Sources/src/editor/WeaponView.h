#ifndef __WEAPONVIEW_H__
#define __WEAPONVIEW_H__



class CWeaponView : public CWnd
{
public:
	CWeaponView();
	
public:
	
public:
	
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	
public:
	virtual ~CWeaponView();
	
private:
	
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};


#endif		//__WEAPONVIEW_H__
