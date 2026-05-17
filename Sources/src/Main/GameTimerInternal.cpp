#include "StdAfx.h"

#include <mmsystem.h>

#include "iMain.h"
#include "GameTimerInternal.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** time slider
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CTimeSlider::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pTimer );
	saver.Add( 2, &timeLastTime );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** segment timer
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CSegmentTimer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &prevTime );
	saver.Add( 2, &currTime );
	saver.Add( 3, &tSegmentTime );
	saver.Add( 4, &nSegment );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** single timer
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSingleTimer::SetGuarantieFPS( const float fFPS )
{
	fGuarantieFPS = fFPS; 
	nGuarantieTimeStep = (fGuarantieFPS > 0) && (fGuarantieFPS < 1000.0f) ? int( 1000.0f / fGuarantieFPS ) : 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSingleTimer::Update( const NTimer::STime &time )
{
	NTimer::STime dT = prevTime == 0 ? 0 : time - prevTime;
	prevTime = time;
	if ( !bPaused )
	{
		float fdt = float( dT*fTimeScale ) + fTimeError;
		dT = NTimer::STime( fdt );
		fTimeError = fdt - float( dT );
		currTime += nGuarantieTimeStep > 0 ? int( nGuarantieTimeStep * fTimeScale ) : dT;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CSingleTimer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &currTime );
	saver.Add( 2, &bPaused );
	saver.Add( 3, &fTimeScale );
	saver.Add( 4, &fTimeError );
	saver.Add( 5, &fGuarantieFPS );
	saver.Add( 6, &nGuarantieTimeStep );
	if ( !saver.IsReading() )
		prevTime = 0;
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** game timer
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameTimer::Init()
{
	pGameTimer = CreateObject<ISingleTimer>( MAIN_SINGLE_TIMER );
	pSyncTimer = CreateObject<ISingleTimer>( MAIN_SINGLE_TIMER );
	pAbsTimer = CreateObject<ISingleTimer>( MAIN_SINGLE_TIMER );
	pGameSegmentTimer = CreateObject<ISegmentTimer>( MAIN_SEGMENT_TIMER );
	pSyncSegmentTimer = CreateObject<ISegmentTimer>( MAIN_SEGMENT_TIMER );
	nTimeCoeff = 0;
	nPauseReason = -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameTimer::DoPause( bool bPause, int nType, ISingleTimer *pTimer, CPausesMap &pauses )
{
	if ( bPause ) 
	{
		// set pause
		pauses[nType] = 1;
		pTimer->Pause( bPause );
	}
	else
	{
		// remove pause
		std::unordered_map<int, int>::iterator pos = pauses.find( nType );
		if ( pos != pauses.end() )
			pauses.erase( pos );
		if ( pauses.empty() ) 
			pTimer->Pause( bPause );
	}
	// re-calc pause reason
	nPauseReason = -1;
	for ( CPausesMap::const_iterator it = pauses.begin(); it != pauses.end(); ++it )
		nPauseReason = Max( nPauseReason, it->first );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameTimer::PauseGame( bool bPause, int nType )
{
	DoPause( bPause, nType, pGameTimer, gamepauses );
}
void CGameTimer::PauseSync( bool bPause, int nType )
{
	DoPause( bPause, nType, pSyncTimer, syncpauses );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGameTimer::HasPause( const int nReason ) const
{
	return gamepauses.find(nReason) != gamepauses.end();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameTimer::SetGuarantieFPS( const float fFPS ) 
{ 
	pGameTimer->SetGuarantieFPS( fFPS );
	pSyncTimer->SetGuarantieFPS( fFPS );
	pAbsTimer->SetGuarantieFPS( fFPS );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameTimer::Update( const NTimer::STime &time )
{
	pGameTimer->Update( time );
	pSyncTimer->Update( time );
	pAbsTimer->Update( time );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameTimer::SetupTimeScaleForTimers()
{
	const float fCoeff = NTimer::GetCoeffFromSpeed( nTimeCoeff );
	pGameTimer->SetTimeScale( fCoeff );
	pSyncTimer->SetTimeScale( fCoeff );
}
int CGameTimer::SetSpeed( const int _nSpeed )
{
	const int nMaxSpeed = +GetGlobalVar( "maxspeed", 10 );
	const int nMinSpeed = -GetGlobalVar( "minspeed", 10 );
	nTimeCoeff = Clamp( _nSpeed, nMinSpeed, nMaxSpeed );
	SetupTimeScaleForTimers();
	return GetSpeed();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CGameTimer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pGameTimer );
	saver.Add( 2, &pSyncTimer );
	saver.Add( 3, &pAbsTimer );
	saver.Add( 4, &pGameSegmentTimer );
	saver.Add( 5, &pSyncSegmentTimer );
	saver.Add( 6, &nTimeCoeff );
	saver.Add( 7, &gamepauses );
	saver.Add( 8, &syncpauses );
	saver.Add( 9, &nPauseReason );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
