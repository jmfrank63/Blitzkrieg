
#include "stdafx.h"
#include "WindowSlider.h"
#include "WindowMultiBkg.h"
#include "SInterfaceConsts.h"
#include "UIScreen.h"

IMPLEMENT_CLONABLE(CWindowSlider);
int CWindowSlider::operator&( IStructureSaver &ss )
{
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
}
int CWindowSlider::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CWindow*>( this ) );
	saver.Add( "Horisontal", &bHorisontal );
	return 0;
}
void CWindowSlider::UpdatePos()
{
	fCur = VerifyPos( fCur );
	
	int nX, nY, nW, nH;
	GetPlacement( &nX, &nY, &nW, &nH );

	int nLeverX, nLeverY, nLeverW, nLeverH;

	if ( bHorisontal )
	{
		nLeverW = fPageSize * nW / ( fMax - fMin );
		nLeverX = ( nW - nLeverW ) * fCur / ( fMax - fMin );
		pLever->SetPlacement( nLeverX, 0, nLeverW, 0,  EWPF_POS_X|EWPF_SIZE_X );
	}
	else
	{
		nLeverH = fPageSize * nH / ( fMax - fMin );
		nLeverY = ( nH - nLeverH ) * fCur / ( fMax - fMin );
		pLever->SetPlacement( 0, nLeverY, 0, nLeverH,  EWPF_POS_Y|EWPF_SIZE_Y );
	}
	NI_ASSERT_T( pNotifySink != 0, "slider witout notify sink" );
	pNotifySink->SliderPosition( fCur / (fMax - fMin) );
}
void CWindowSlider::SetRange( const float _fMin, const float _fMax, const float _fPageSize  )
{
	NI_ASSERT_T( _fMin < _fMax, NStr::Format( "minimum must be strict lesser than maximum" ) );
	fMin = _fMin;
	fMax = _fMax;
	fPageSize = _fPageSize;
	UpdatePos();
}
void CWindowSlider::GetRange( int *pMax, int *pMin ) const
{
	*pMin = fMin;
	*pMax = fMax;
}
void CWindowSlider::SetPos( const int _fCur )
{
	fCur = _fCur;
	UpdatePos();
}
int CWindowSlider::GetPos() const
{
	return fCur;
}
void CWindowSlider::Reposition( const CTRect<float> &parentRect )
{
	if ( !pLever )
		pLever = dynamic_cast<CWindowMSButton*>( GetChild( "Lever" ) );
	NI_ASSERT_T( pLever != 0, "slider without lever" );
	
	SetPos( fCur );
	CWindow::Reposition( parentRect );
}
void CWindowSlider::OnKeyUp( const struct SGameMessage &msg )
{
	if ( !bHorisontal )
	{
		--fCur;
		UpdatePos();
	}
}
void CWindowSlider::OnKeyDown( const struct SGameMessage &msg )
{
	if ( !bHorisontal )
	{
		++fCur;
		UpdatePos();
	}
}
void CWindowSlider::OnKeyPgDn( const struct SGameMessage &msg )
{
	fCur += fPageSize;
	UpdatePos();
}
void CWindowSlider::OnKeyPgUp( const struct SGameMessage &msg )
{
	fCur -= fPageSize;
	UpdatePos();
}
void CWindowSlider::OnKeyHome( const struct SGameMessage &msg )
{
	fCur = fMin;
	UpdatePos();
}
void CWindowSlider::OnKeyEnd( const struct SGameMessage &msg )
{
	fCur = fMax - 1;
	UpdatePos();
}
void CWindowSlider::OnKeyRight( const struct SGameMessage &msg )
{
	if ( bHorisontal )
	{
		++fCur;
		UpdatePos();
	}
}
void CWindowSlider::OnKeyLeft( const struct SGameMessage &msg )
{
	if ( bHorisontal )
	{
		--fCur;
		UpdatePos();
	}
}
void CWindowSlider::OnMouseMove( const CVec2 &vPos, const int nButton )
{
	if ( nButton & MSTATE_BUTTON1 )
	{
		const char *pszPressed = GetPressedName( MSTATE_BUTTON1 );
		if (  pszPressed && pLever->GetName() == pszPressed )
		{
			SetPos( CalcPressedPos( vPos ) );
		}
		else if ( Pick( vPos ) == pLever )			// lever is under cursor
		{
			bFastScrolling = false;
		}
		else if ( IsInside( vPos ) )
		{
			vPressedPos = vPos;
			bFastScrolling = true;
		}
	}
}
float CWindowSlider::VerifyPos( const float _fCur ) const
{
	if ( _fCur < fMin ) return fMin;
	if ( _fCur > fMax ) return fMax;
	return _fCur;
}
float CWindowSlider::CalcPressedPos( const CVec2 &vPos ) const
{
	CTRect<float> rect;
	FillWindowRect( &rect );
	
	int nLeverX, nLeverY, nLeverW, nLeverH;
	pLever->GetPlacement( &nLeverX, &nLeverY, &nLeverW, &nLeverH );
	
	const float fOffset = bHorisontal ? vPos.x - rect.left - nLeverW/2.0f: vPos.y - rect.top - nLeverH/2.0f;
	const float fSize = bHorisontal ? rect.Width() - nLeverW : rect.Height() - nLeverH;
	return fOffset / fSize * ( fMax - fMin );
}
void CWindowSlider::OnButtonDown( const CVec2 &vPos, const int nButton )
{
	CWindow::OnButtonDown( vPos, nButton );
	
	const char *pszPressed = GetPressedName( MSTATE_BUTTON1 );
	if ( !pszPressed || pLever->GetName() != pszPressed )
	{
		bFirstTime = true;

		bPressed = true;
		animTime = 0;
		vPressedPos = vPos;
		bFastScrollForward = VerifyPos( CalcPressedPos( vPos ) ) > fCur;
		bFastScrolling = true;
		ScrollFast();
		GetScreen()->RegisterToSegment( this, true );
	}
}
void CWindowSlider::ScrollFast()
{
	if ( VerifyPos( CalcPressedPos( vPressedPos ) ) > fCur == bFastScrollForward )
	{
		fCur += bFastScrollForward ? fPageSize : -fPageSize;
		UpdatePos();
	}
}
void CWindowSlider::Segment( const NTimer::STime timeDiff )
{
	if ( bPressed && bFastScrolling )
	{
		animTime += timeDiff;
		
		if ( Pick( vPressedPos ) == pLever )
		{
			animTime = 0;
		}
		else if ( animTime > (bFirstTime ? SInterfaceConsts::SCROLLER_ANIM_TIME_1() : SInterfaceConsts::SCROLLER_ANIM_TIME()) )
		{
			ScrollFast();	
			animTime = 0;
			bFirstTime = false;
		}
	}
}
void CWindowSlider::OnButtonUp( const CVec2 &vPos, const int nButton )
{
	CWindow::OnButtonUp( vPos, nButton );
	GetScreen()->RegisterToSegment( this, false );
}
