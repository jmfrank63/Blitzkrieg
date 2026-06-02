
#if !defined(AFX_WINDOWSCROLLBAR_H__46D0E093_95AF_4D78_9A5B_BB754D40FC3A__INCLUDED_)
#define AFX_WINDOWSCROLLBAR_H__46D0E093_95AF_4D78_9A5B_BB754D40FC3A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Window.h"
#include "Interface.h"
#include "WindowSlider.h"
#include "WindowMultiBkg.h"

class CWindowScrollBar : 	public CWindow, public ISlider
, public ISliderNotify
{
	OBJECT_COMPLETE_METHODS(CWindowScrollBar)
	DECLARE_SERIALIZE
	DECLARE_CLONABLE_CLASS 

	CNCPtr<CWindowMSButton> pButtonUp;
	CNCPtr<CWindowMSButton> pButtonDown;

	CNCPtr<CWindowSlider> pSlider;
	
public:

	virtual int STDCALL operator&( IDataTree &ss );
	virtual void STDCALL Reposition( const CTRect<float> &parentRect );

	virtual void STDCALL SliderPosition( const float fPosition ) 
	{
		
	}
	virtual void STDCALL SetRange( const float fMin, const float fMax, const float fPageSize ) { pSlider->SetRange( fMin, fMax, fPageSize ); }
	virtual void STDCALL GetRange( int *pMax, int *pMin ) const { pSlider->GetRange( pMax, pMin ); }
	virtual void STDCALL SetPos( const int nCur ) { pSlider->SetPos( nCur ); }
	virtual int STDCALL GetPos() const { return pSlider->GetPos(); }

	virtual void STDCALL SetNotifySink( interface ISliderNotify *_pNotifySink );
};
#endif // !defined(AFX_WINDOWSCROLLBAR_H__46D0E093_95AF_4D78_9A5B_BB754D40FC3A__INCLUDED_)
