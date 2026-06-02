#ifndef __UINSTATEBUTTON_H__
#define __UINSTATEBUTTON_H__
#include "UIBasic.h"

class CUIStatic : public CSimpleWindow
{
public:
	CUIStatic() { SetShowBackgroundFlag( 0 ); }
	friend class CUIObjective;
};
class CUIButton : public CSimpleWindow
{
public:
	CUIButton() {  }
};
class CUIStaticBridge : public IUIStatic, public CUIStatic
{
	OBJECT_NORMAL_METHODS( CUIStaticBridge );
	DECLARE_SUPER( CUIStatic );
public:
	DEFINE_UIELEMENT_BRIDGE;
	
	virtual IUIElement * STDCALL Duplicate()
	{ 
		CUIStaticBridge * pWnd = new CUIStaticBridge;
		CopyInternals( pWnd );
		return pWnd;
	}
};
class CUIButtonBridge : public IUIButton, public CUIButton
{
	OBJECT_NORMAL_METHODS( CUIButtonBridge );
	DECLARE_SUPER( CUIButton );
	DEFINE_UIELEMENT_BRIDGE;
};
#endif // __UINSTATEBUTTON_H__
