#include "StdAfx.h"

#include "Transition.h"
#include <mmsystem.h>

#include "../SFX/SFX.h"
#define ALPHA_MAX 255.0f
#define ALPHA_MIN 0.0f
#define DURATION 500
bool CTransition::Update( const NTimer::STime &time, bool bForced )
{
	NTimer::STime currTime = timeGetTime();
	if ( timeStart == 0 ) 
		timeStart = currTime;
	fAlpha = Clamp( fAlphaStart + ( fAlphaEnd - fAlphaStart ) * float( currTime - timeStart ) / float( DURATION ), ALPHA_MIN, ALPHA_MAX );
	return bInfinite || ( timeStart + DURATION > currTime );
}
bool CTransition::Draw( interface IGFX *pGFX )
{
	SGFXRect2 rect;
	rect.rect = pGFX->GetScreenRect();
	rect.maps.SetEmpty();
	rect.fZ = 0.001f;
	rect.color = ( DWORD(fAlpha) << 24 ) & 0xff000000;
	pGFX->SetShadingEffect( 3 );
	pGFX->SetTexture( 0, 0 );
	return pGFX->DrawRects( &rect, 1 );
}
void CTransition::Visit( interface ISceneVisitor *pVisitor, int nType )
{
	pVisitor->VisitSceneObject( this );
}
int CTransition::Start( const char *pszVideoName, const DWORD dwAddFlags, const NTimer::STime &_currTime, const bool bFadeIn )
{
	timeStart = 0;
	if ( bFadeIn ) 
	{
		fAlphaStart = ALPHA_MIN;
		fAlphaEnd = ALPHA_MAX;
	}
	else
	{
		fAlphaStart = ALPHA_MAX;
		fAlphaEnd = ALPHA_MIN;
	}
	bInfinite = ( dwAddFlags & IVideoPlayer::PLAY_INFINITE ) != 0;
	return DURATION + 150;
}
int CTransition::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 3, &fAlphaStart );
	saver.Add( 4, &fAlphaEnd );
	saver.Add( 5, &fAlpha );
	saver.Add( 6, &bInfinite );
	if ( saver.IsReading() ) 
	{
		timeStart = timeGetTime();
	}
	return 0;
}
