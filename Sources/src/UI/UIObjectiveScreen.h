#ifndef __UI_OBJECTIVE_SCREEN_H__
#define __UI_OBJECTIVE_SCREEN_H__
#include "UIBasic.h"
#include "UIDialog.h"
class CUIObjectiveScreen : public CMultipleWindow
{
	DECLARE_SERIALIZE;
	IUIShortcutBar *pSB;				//инициализируется во время загрузки и используется для ускорения доступа к компонентам
	bool bShowAllObjectives;

public:
	CUIObjectiveScreen() : pSB( 0 ), bShowAllObjectives( false ) {}
	~CUIObjectiveScreen() {}

	virtual bool STDCALL OnMouseWheel( const CVec2 &vPos, EMouseState mouseState, float fDelta )
	{
		return pSB->OnMouseWheel( vPos, mouseState, fDelta );
	}
	virtual int STDCALL operator&( IDataTree &ss );
	virtual void STDCALL ShowWindow( int _nCmdShow );
};
class CUIObjectiveScreenBridge : public IUIContainer, public CUIObjectiveScreen
{
	OBJECT_NORMAL_METHODS( CUIObjectiveScreenBridge );
	DECLARE_SUPER( CUIObjectiveScreen );
	DEFINE_UICONTAINER_BRIDGE;
};
#endif		//__UI_OBJECTIVE_SCREEN_H__
