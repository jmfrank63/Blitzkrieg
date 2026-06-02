#ifndef __UI_STATUSBAR_H__
#define __UI_STATUSBAR_H__

#include "UIButton.h"
const int PROP_SIZE = 2;

class CUIStatusBar : public CMultipleWindow
{
	DECLARE_SERIALIZE;
	typedef std::vector<int> CIDVector;
	CIDVector idVectors[PROP_SIZE];
	int nVisibleWindowPropIDs[PROP_SIZE];
	DWORD dwActiveIcons;
	CVec2 vInitialIconPos;

public:
	CUIStatusBar();
	virtual void STDCALL OutputString( int nControl, const WORD *pszString );
	virtual void STDCALL OutputValue( int nControl, float fVal );
	virtual void STDCALL SetUnitProperty( int nPropType, int nPropValue, const WORD *pszToolText );
	virtual void STDCALL SetUnitIcons( DWORD dwIcons );

	virtual int STDCALL operator&( IDataTree &ss );
};
class CUIStatusBarBridge : public IUIStatusBar, public CUIStatusBar
{
	OBJECT_NORMAL_METHODS( CUIStatusBarBridge );
	DECLARE_SUPER( CUIStatusBar );
	DEFINE_UICONTAINER_BRIDGE;
	virtual void STDCALL OutputString( int nControl, const WORD *pszText ) { CSuper::OutputString( nControl, pszText ); }
	virtual void STDCALL OutputValue( int nControl, float fVal ) { CSuper::OutputValue( nControl, fVal ); }
	virtual void STDCALL SetUnitProperty( int nPropType, int nPropValue, const WORD *pszToolText ) { CSuper::SetUnitProperty( nPropType, nPropValue, pszToolText ); }
	virtual void STDCALL SetUnitIcons( DWORD dwIcons ) { CSuper::SetUnitIcons( dwIcons ); }
};
#endif		//__UI_STATUSBAR_H__
