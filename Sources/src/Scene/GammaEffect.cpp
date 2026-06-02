#include "StdAfx.h"

#include "GammaEffect.h"
CGammaEffect::~CGammaEffect()
{
	if ( timeDuration != 0 ) 
		SetGammaCorrectionBounded( vOriginal.x, vOriginal.y, vOriginal.z, GetSingleton<IGFX>(), true, false );
}
void CGammaEffect::Init( const float fGammaR, const float fGammaG, const float fGammaB,
 									       const NTimer::STime &_timeStart, const NTimer::STime &_timeDuration )
{
	float fBrightness, fContrast, fGamma;
	GetSingleton<IGFX>()->GetGammaCorrectionValues( &fBrightness, &fContrast, &fGamma );
	vOriginal.Set( fBrightness, fContrast, fGamma );
	vGamma[0].Set( fGamma, fGamma, fGamma );
	vGamma[1].Set( fGammaR, fGammaG, fGammaB );
	vGamma[2].Set( fGamma, fGamma, fGamma );
	timeStart = _timeStart;
	timeDuration = _timeDuration;
}
bool CGammaEffect::Update( const NTimer::STime &time, bool bForced )
{
	if ( time > timeStart + timeDuration ) 
	{
		SetGammaCorrectionBounded( vOriginal.x, vOriginal.y, vOriginal.z, GetSingleton<IGFX>(), true, false );
		return false;
	}
	if ( time > timeStart + timeDuration/2 ) 
	{
		const float fCoeff = float( time - (timeStart + timeDuration/2) ) / float( timeDuration / 2 );
		vCurrGamma.Interpolate( vGamma[1], vGamma[2], fCoeff );
	}
	else
	{
		const float fCoeff = float( time - timeStart ) / float( timeDuration / 2 );
		vCurrGamma.Interpolate( vGamma[0], vGamma[1], fCoeff );
	}
	return true;
}
bool CGammaEffect::Draw( IGFX *pGFX )
{
	return SetGammaCorrectionBounded( vCurrGamma.r, vOriginal.y, vOriginal.z, pGFX, false, false );
}
void CGammaEffect::Visit( interface ISceneVisitor *pVisitor, int nType )
{
	pVisitor->VisitSceneObject( this );
}
int CGammaEffect::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddRawData( 1, &(vGamma[0]), sizeof(vGamma) );
	saver.Add( 2, &vCurrGamma );
	saver.Add( 3, &vOriginal );
	saver.Add( 4, &timeStart );
	saver.Add( 5, &timeDuration );
	return 0;
}
CGammaFader::~CGammaFader()
{
	if ( timeDuration != 0 ) 
		SetGammaCorrectionBounded( vOriginal.x, vOriginal.y, vOriginal.z, GetSingleton<IGFX>(), true, false );
}
bool CGammaFader::Update( const NTimer::STime &time, bool bForced )
{
	if ( timeStart == 0 )
	{
		float fBrightness, fContrast, fGamma;
		GetSingleton<IGFX>()->GetGammaCorrectionValues( &fBrightness, &fContrast, &fGamma );
		vOriginal.Set( fBrightness, fContrast, fGamma );
		timeStart = time;
		timeDuration = GetGlobalVar( "Scene.FadeOut.Time", 1000 );
		fCurrValue = -1;
	}
	else if ( time > timeStart + timeDuration ) 
	{
		SetGammaCorrectionBounded( vOriginal.x, vOriginal.y, vOriginal.z, GetSingleton<IGFX>(), true, false );
		return false;
	}
	else
	{
		const float fCoeff = Clamp( float( timeStart + timeDuration - time ) / float( timeDuration ), 0.0f, 1.0f );
		fCurrValue = vOriginal.x + fCoeff * ( -1.0f - vOriginal.x );
	}
	return true;
}
bool CGammaFader::Draw( interface IGFX *pGFX )
{
	return SetGammaCorrectionBounded( fCurrValue, vOriginal.y, vOriginal.z, pGFX, false, false );
}
void CGammaFader::Visit( interface ISceneVisitor *pVisitor, int nType )
{
	pVisitor->VisitSceneObject( this );
}
int CGammaFader::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &vOriginal );
	saver.Add( 2, &timeStart );
	saver.Add( 4, &fCurrValue );
	if ( saver.IsReading() ) 
	{
		timeStart = 1;
		timeDuration = 0;
	}
	return 0;
}
