#include "StdAfx.h"
#include "StreamFadeOff.h"

int CStreamFadeOff::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &fVolume );
	saver.Add( 3, &fVolumeSpeed );
	if ( saver.IsReading() )
	{
		InitConsts();
		bool bRun = true;
		saver.Add( 4, &bRun );
		if ( bRun ) Fade( int(fVolume/fVolumeSpeed) );
	}
	else if ( IsFading() )
	{
		bool bRun = true;
		saver.Add( 4, &bRun );
	}
	return 0;
}

void CStreamFadeOff::InitConsts()
{
	pSFX = GetSingleton<ISFX>();
	bStopping = false;
	bThreadStarted = false;
	bWorkerFinished = false;
	nFinishedPending.store( 0 );
}

CStreamFadeOff::~CStreamFadeOff() { Clear(); }

bool CStreamFadeOff::Segment( const int nTimeDelta )
{
	if ( !pSFX ) return false;
	const float fDVolume = ( timeAccumulator + nTimeDelta ) * fVolumeSpeed;
	if ( fVolume != 0.0f && fDVolume > 0.01 )
	{
		fVolume = Max( 0.0f, fVolume - fDVolume );
		pSFX->SetStreamVolume( fVolume );
		timeAccumulator = 0;
	}
	else timeAccumulator += nTimeDelta;
	return fVolume > 0 || timeAccumulator < 500;
}

void CStreamFadeOff::Step()
{
	if ( !Segment( 100 ) )
	{
		bWorkerFinished = true;
		nFinishedPending.store( 1 );
		FinishThread();
	}
}

void CStreamFadeOff::Clear()
{
	if ( bStopping ) return;
	bStopping = true;
	if ( bThreadStarted ) StopThread();
	bThreadStarted = false;
	bWorkerFinished = false;
	bStopping = false;
}

void CStreamFadeOff::Fade( const unsigned int nTimeToFade )
{
	if ( !pSFX ) return;
	NI_ASSERT_T( nTimeToFade != 0, "cannot fade with zero or negative time" );
	Clear();
	fVolume = pSFX->GetStreamVolume();
	fVolumeSpeed = fVolume / nTimeToFade;
	timeAccumulator = 0;
	bWorkerFinished = false;
	bThreadStarted = true;
	RunThread();
}

bool CStreamFadeOff::IsFading() const
{
	return bThreadStarted && !bWorkerFinished;
}
