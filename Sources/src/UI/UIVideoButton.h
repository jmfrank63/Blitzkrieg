#ifndef __UIVIDEO_BUTTON_H__
#define __UIVIDEO_BUTTON_H__
#include "UIBasic.h"
#include "..\Scene\Scene.h"

class CUIVideoButton : public CSimpleWindow
{
	DECLARE_SERIALIZE;
	std::string szBinkFile;
	CPtr<IVideoPlayer> pVideoPlayer;

	void InitVideoPlayer();
	void Play();
public:
	CUIVideoButton() {  }

	virtual int STDCALL operator&( IDataTree &ss );

	virtual void STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );

	virtual void STDCALL Reposition( const CTRect<float> &rcParent );
	virtual bool STDCALL Update( const NTimer::STime &currTime );

	virtual int STDCALL GetCurrentFrame();
	virtual bool STDCALL SetCurrentFrame( int nFrame );
};
class CUIVideoButtonBridge : public IUIVideoButton, public CUIVideoButton
{
	OBJECT_NORMAL_METHODS( CUIVideoButtonBridge );
	DECLARE_SUPER( CUIVideoButton );
	DEFINE_UIELEMENT_BRIDGE;

	virtual int STDCALL GetCurrentFrame() { return CSuper::GetCurrentFrame(); }
	virtual bool STDCALL SetCurrentFrame( int nFrame ) { return CSuper::SetCurrentFrame( nFrame ); }
};

#endif // __UIVIDEO_BUTTON_H__
