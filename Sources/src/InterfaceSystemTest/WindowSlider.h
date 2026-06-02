
#if !defined(AFX_WINDOWSLIDER_H__FF45A97C_D276_4BE2_BF2C_061AFFE51E2F__INCLUDED_)
#define AFX_WINDOWSLIDER_H__FF45A97C_D276_4BE2_BF2C_061AFFE51E2F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Interface.h"
#include "Window.h"

class CWindowMSButton;
class CWindowSlider : public CWindow, public ISlider  
{
	OBJECT_COMPLETE_METHODS(CWindowSlider)
	DECLARE_SERIALIZE
	DECLARE_CLONABLE_CLASS 

	CPtr<CWindowMSButton> pLever;
	bool bHorisontal;												// if false - then vertical
	
	float fMin;
	float fMax;
	float fCur;
	float fPageSize;														// 
	
	bool bPressed;													// remember pressed state, scrolls by timer
	bool bFirstTime;												// first scroll should will wait for a longer time
	NTimer::STime animTime;
	CVec2 vPressedPos;
	bool bFastScrollForward;
	bool bFastScrolling;

	ISliderNotify * pNotifySink;						// parent
	
	void UpdatePos();
	float CalcPressedPos( const CVec2 &vPos ) const;
	float VerifyPos( const float _fCur ) const;
	void ScrollFast();

public:

	CWindowSlider() : fMin( 0.0f ), fMax( 100.0f ), fCur( 0.0f ), fPageSize( 5.0f ),
		bPressed( false ), bFastScrolling( false ), bHorisontal( false ), pNotifySink( 0 ) {  }

	bool IsHorisontal() const { return bHorisontal; }

	void OnKeyUp( const struct SGameMessage &msg );
	void OnKeyDown( const struct SGameMessage &msg );

	void OnKeyRight( const struct SGameMessage &msg );
	void OnKeyLeft( const struct SGameMessage &msg );

	void OnKeyPgDn( const struct SGameMessage &msg );
	void OnKeyPgUp( const struct SGameMessage &msg );

	void OnKeyHome( const struct SGameMessage &msg );
	void OnKeyEnd( const struct SGameMessage &msg );

	virtual void STDCALL SetRange( const float _fMin, const float _fMax, const float _fPageSize );
	virtual void STDCALL GetRange( int *pMax, int *pMin ) const;
	virtual void STDCALL SetPos( const int _nCur );
	virtual int STDCALL GetPos() const;
	virtual void STDCALL SetNotifySink( interface ISliderNotify *_pNotifySink ) { pNotifySink = _pNotifySink; }

	virtual int STDCALL operator&( IDataTree &ss );
	virtual void STDCALL Reposition( const CTRect<float> &parentRect );

	virtual void STDCALL Segment( const NTimer::STime timeDiff );
	
	virtual void STDCALL OnMouseMove( const CVec2 &vPos, const int nButton );
	virtual void STDCALL OnButtonDown( const CVec2 &vPos, const int nButton );
	virtual void STDCALL OnButtonUp( const CVec2 &vPos, const int nButton );

};
#endif // !defined(AFX_WINDOWSLIDER_H__FF45A97C_D276_4BE2_BF2C_061AFFE51E2F__INCLUDED_)
