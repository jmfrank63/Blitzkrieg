#ifndef __USER_INTERFACE_BRIDGE_H__
#define __USER_INTERFACE_BRIDGE_H__
#define DEFINE_UIELEMENT_BRIDGE																																								\
	virtual int STDCALL operator&( IDataTree &ss ) { return CSuper::operator&( ss ); }													\
	virtual int STDCALL operator&( IStructureSaver &ss ) { return CSuper::operator&( ss ); }										\
	virtual bool STDCALL Update( const NTimer::STime &currTime ) { return CSuper::Update( currTime ); }					\
	virtual void STDCALL Reposition( const CTRect<float> &rcParent ) { CSuper::Reposition( rcParent ); }				\
	virtual void STDCALL Draw( IGFX *pGFX ) { CSuper::Draw( pGFX ); }																						\
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor ) { CSuper::Visit( pVisitor ); }							\
	virtual bool STDCALL OnLButtonDblClk( const CVec2 &vPos ) { return CSuper::OnLButtonDblClk( vPos ); }				\
	virtual bool STDCALL OnMouseMove( const CVec2 &vPos, EMouseState mouseState ) { return CSuper::OnMouseMove( vPos, mouseState ); }			\
	virtual bool STDCALL OnLButtonDown( const CVec2 &vPos, EMouseState mouseState ) { return CSuper::OnLButtonDown( vPos, mouseState ); }	\
	virtual bool STDCALL OnLButtonUp( const CVec2 &vPos, EMouseState mouseState ) { return CSuper::OnLButtonUp( vPos, mouseState ); }			\
	virtual bool STDCALL OnRButtonDown( const CVec2 &vPos, EMouseState mouseState ) { return CSuper::OnRButtonDown( vPos, mouseState ); }	\
	virtual bool STDCALL OnRButtonUp( const CVec2 &vPos, EMouseState mouseState ) { return CSuper::OnRButtonUp( vPos, mouseState ); }			\
	virtual bool STDCALL OnMouseWheel( const CVec2 &vPos, EMouseState mouseState, float fDelta ) { return CSuper::OnMouseWheel( vPos, mouseState, fDelta ); } \
	virtual bool STDCALL IsInside( const CVec2 &vPos ) { return CSuper::IsInside( vPos ); }											\
	virtual void STDCALL ShowWindow( int nCmdShow ) { CSuper::ShowWindow( nCmdShow ); }													\
	virtual void STDCALL SetWindowTexture( IGFXTexture *pTexture ) { CSuper::SetWindowTexture( pTexture ); }		\
	virtual IGFXTexture* STDCALL GetWindowTexture() { return CSuper::GetWindowTexture(); }											\
	virtual void STDCALL SetWindowMap( const CTRect<float> &maps ) { CSuper::SetWindowMap( maps ); }						\
	virtual void STDCALL SetWindowPlacement( const CVec2 *vPos, const CVec2 *vSize ) { CSuper::SetWindowPlacement( vPos, vSize ); }				\
	virtual void STDCALL SetWindowID( int _nID ) { CSuper::SetWindowID( _nID ); }																\
	virtual bool STDCALL OnChar( int nAsciiCode, int nVirtualKey, bool bPressed, DWORD keyState ) { return CSuper::OnChar( nAsciiCode, nVirtualKey, bPressed, keyState ); }	\
	virtual int STDCALL  GetWindowID() { return CSuper::GetWindowID(); }																				\
	virtual void STDCALL SetWindowText( int nState, const WORD *pszText ) { CSuper::SetWindowText( nState, pszText ); }		\
	virtual const WORD* STDCALL GetWindowText( int nState ) { return CSuper::GetWindowText( nState ); }										\
	virtual void STDCALL SetTextColor( DWORD dwColor ) { CSuper::SetTextColor( dwColor ); }											\
	virtual void STDCALL SetBoundRect( const CTRect<float> &rc ) { CSuper::SetBoundRect( rc ); }								\
	virtual void STDCALL ScaleLayout( const CVec2 &vScale ) { CSuper::ScaleLayout( vScale ); }									\
	virtual CVec2 STDCALL GetLayoutScale() { return CSuper::GetLayoutScale(); }													\
	virtual int STDCALL GetWindowPlacement( CVec2 *pPos, CVec2 *pSize, CTRect<float> *pScreenRect ) { return CSuper::GetWindowPlacement( pPos, pSize, pScreenRect ); } \
	virtual int STDCALL GetPositionFlag() { return CSuper::GetPositionFlag(); }																	\
	virtual IText* STDCALL GetHelpContext( const CVec2 &vPos, CTRect<float> *pRect ) { return CSuper::GetHelpContext( vPos, pRect ); }		\
	virtual void STDCALL SetHelpContext( int nState, const WORD *pszToolTipText ) { CSuper::SetHelpContext( nState, pszToolTipText ); }		\
	virtual void STDCALL SetParent( IUIContainer *pParent ) { CSuper::SetParent( pParent ); }										\
	virtual IUIContainer* STDCALL GetParent() { return CSuper::GetParent(); }																		\
	virtual IManipulator* STDCALL GetManipulator() { return CSuper::GetManipulator(); }													\
	virtual IUIElement* STDCALL PickElement( const CVec2 &vPos, int nRecursion ) { return CSuper::PickElement( vPos, nRecursion ); }			\
	virtual bool STDCALL ProcessMessage( const SUIMessage &msg ) { return CSuper::ProcessMessage( msg ); }			\
	virtual bool STDCALL IsVisible() { return CSuper::IsVisible(); }																						\
	virtual int  STDCALL GetVisibleState() { return CSuper::GetVisibleState(); }																\
	virtual void STDCALL EnableWindow( bool bEnable ) { CSuper::EnableWindow( bEnable ); }											\
	virtual bool STDCALL IsWindowEnabled() { return CSuper::IsWindowEnabled(); }																\
	virtual void STDCALL SetFocus( bool bFocus ) { CSuper::SetFocus( bFocus ); }																\
	virtual void STDCALL SetState( int nState, bool bNotify ) { CSuper::SetState( nState, bNotify ); }	\
	virtual int STDCALL GetState() { return CSuper::GetState(); }																				\
	virtual void STDCALL GetTextSize( const int nState, int *pSizeX, int *pSizeY ) const { CSuper::GetTextSize( nState, pSizeX, pSizeY );} \
	virtual bool STDCALL IsModal() const { return CSuper::IsModal(); }

#define DEFINE_UICONTAINER_BRIDGE																																							\
	DEFINE_UIELEMENT_BRIDGE																																											\
	virtual void STDCALL AddChild( IUIElement *pWnd ) { CSuper::AddChild( pWnd ); }															\
	virtual void STDCALL RemoveChild( IUIElement *pWnd ) { CSuper::RemoveChild( pWnd ); }												\
	virtual void STDCALL RemoveAllChildren() { CSuper::RemoveAllChildren(); }																		\
	virtual IUIElement* STDCALL GetChildByID( int nChildID ) { return CSuper::GetChildByID( nChildID ); }				\
	virtual void STDCALL MoveWindowUp( IUIElement *pWnd ) { CSuper::MoveWindowUp( pWnd ); }											\
	virtual void STDCALL MoveWindowDown( IUIElement *pWnd ) { CSuper::MoveWindowDown( pWnd ); }									\
	virtual void STDCALL SetModalFlag( bool bFlag ) { CSuper::SetModalFlag( bFlag ); }													\
	virtual void STDCALL SetFocusedWindow( IUIElement *pNewFocusWindow ) { CSuper::SetFocusedWindow( pNewFocusWindow ); }

#define DEFINE_UISCREEN_BRIDGE																																								\
	DEFINE_UICONTAINER_BRIDGE																																										\
	virtual int STDCALL Load( const char *pszResourceName, bool bRelative ) { return CSuper::Load( pszResourceName, bRelative ); }				\
	virtual void STDCALL ProcessGameMessage( const SGameMessage &msg ) { CSuper::ProcessGameMessage( msg ); }		\
	virtual bool STDCALL GetMessage( SGameMessage *pMsg ) { return CSuper::GetMessage( pMsg ); }								\
	virtual void STDCALL ClearStrings() { CSuper::ClearStrings(); }

#endif // __USER_INTERFACE_BRIDGE_H__