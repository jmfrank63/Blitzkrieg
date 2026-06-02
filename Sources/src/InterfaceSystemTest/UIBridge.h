#ifndef __USER_INTERFACE_BRIDGE_H__
#define __USER_INTERFACE_BRIDGE_H__
#define DEFINE_UIELEMENT_BRIDGE \
	virtual int STDCALL operator&( IDataTree &ss ) { return CSuper::operator&( ss ); } \
	virtual int STDCALL operator&( IStructureSaver &ss ) { return CSuper::operator&( ss ); } \
	virtual bool STDCALL OnMouseMove( const CVec2 &vPos, EMouseState mouseState ) { return CSuper::OnMouseMove( vPos, mouseState ); } \
	virtual bool STDCALL OnLButtonDown( const CVec2 &vPos, EMouseState mouseState ) { return CSuper::OnLButtonDown( vPos, mouseState ); } \
	virtual bool STDCALL ProcessMessage( const SUIMessage &msg ) { return CSuper::ProcessMessage( msg ); }

#define DEFINE_UICONTAINER_BRIDGE \
	DEFINE_UIELEMENT_BRIDGE \
	virtual void STDCALL AddChild( IUIElement *pWnd ) { CSuper::AddChild( pWnd ); } \
	virtual void STDCALL RemoveChild( IUIElement *pWnd ) { CSuper::RemoveChild( pWnd ); }

#define DEFINE_UISCREEN_BRIDGE \
	DEFINE_UICONTAINER_BRIDGE \
	virtual int STDCALL Load( const char *pszResourceName, bool bRelative ) { return CSuper::Load( pszResourceName, bRelative ); } \
	virtual void STDCALL ProcessGameMessage( const SGameMessage &msg ) { CSuper::ProcessGameMessage( msg ); }

#endif // __USER_INTERFACE_BRIDGE_H__
