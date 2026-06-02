#ifndef __UICREDITSSCROLLER_H__
#define __UICREDITSSCROLLER_H__
#pragma once
#include "UIBasic.h"

class CUICreditsScroller : public CSimpleWindow
{
	NTimer::STime nLastUpdate;
	bool bWorking;
	int nCurrOffset;
	int nMaxOffset;
public:
	CUICreditsScroller();
	virtual void STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );
	virtual bool STDCALL Update( const NTimer::STime &currTime );
	virtual void STDCALL ShowWindow( int _nCmdShow );
};
class CUICreditsScrollerBridge : public IUICreditsScroller, public CUICreditsScroller
{
	OBJECT_NORMAL_METHODS( CUICreditsScrollerBridge );
	DECLARE_SUPER( CUICreditsScroller );
	DEFINE_UIELEMENT_BRIDGE;
};
#endif // __UICREDITSSCROLLER_H__
