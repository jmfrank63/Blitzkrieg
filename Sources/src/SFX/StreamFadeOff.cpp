#include "stdAfx.h"
#include "Winbase.h"
#include "Mmsystem.h"

#include "StreamFadeOff.h"
DWORD WINAPI TheThreadProc( LPVOID lpParameter )
{
	CStreamFadeOff * pFade = reinterpret_cast<CStreamFadeOff*>(lpParameter);
	pFade->Start();

	while( pFade->HaveToRun() )
	{
		if ( pFade->Segment( 100 ) )
			Sleep(100);
		else
			break;
	}

	pFade->Stop();
	return 0;
}
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

		if ( bRun )
		{
			Fade( int(fVolume/fVolumeSpeed) );
		}
	}
	else
	{
		if ( hThread && WAIT_OBJECT_0 != WaitForSingleObject( hFinishReport,0 ) ) // ���� ���� �� �����������
		{
			bool bRun = true;
			saver.Add( 4, &bRun );
		}
	}

	return 0;
}
void CStreamFadeOff::InitConsts()
{
	pSFX = GetSingleton<ISFX>();

	hThread = 0;
	hFinishReport = CreateEvent( 0, true, false, 0 );
	hStopCommand = CreateEvent( 0, true, false, 0 );
	bStopping = false;
}
CStreamFadeOff::~CStreamFadeOff() 
{ 
	Clear(); 
	CloseHandle( hFinishReport );
	CloseHandle( hStopCommand );
}
bool CStreamFadeOff::Segment( const int nTimeDelta )
{
	if ( !pSFX ) 
		return false;
	const float fDVolume = ( timeAccumulator + nTimeDelta )* fVolumeSpeed;
	if ( fVolume != 0.0f && fDVolume > 0.01 )
	{
		fVolume -= fDVolume;
		fVolume = Max( 0.0f, fVolume );
		if ( fVolume >= 0 )
			pSFX->SetStreamVolume( fVolume );
		timeAccumulator = 0;		
	}
	else
		timeAccumulator += nTimeDelta;
	return fVolume > 0 || timeAccumulator < 500;
}
void CStreamFadeOff::Clear()
{
	if ( bStopping )
		return;

	if ( hThread )
	{
		if ( WAIT_OBJECT_0 != WaitForSingleObject( hFinishReport,0 ) ) // ���� ���� �� �����������
		{
			SetEvent( hStopCommand );
			WaitForSingleObject( hFinishReport, INFINITE );
		}

		CloseHandle( hThread );
		hThread = 0;
	}

	ResetEvent( hStopCommand );
	ResetEvent( hFinishReport );
}
void CStreamFadeOff::Start()
{
	timeAccumulator = 0;
	ResetEvent( hFinishReport );
}
void CStreamFadeOff::Fade( const unsigned int nTimeToFade )
{
	if ( !pSFX ) return;

	NI_ASSERT_T( nTimeToFade != 0, "cannot fade with zero or negative time" );
	Clear();
	fVolume = pSFX->GetStreamVolume();
	fVolumeSpeed = fVolume / nTimeToFade;
	DWORD dwThreadId;
	hThread = CreateThread( 0, 1024*1024, TheThreadProc, reinterpret_cast<LPVOID>(this), 0, &dwThreadId );
}
bool CStreamFadeOff::IsFading() const
{
	return hThread && WAIT_OBJECT_0 != WaitForSingleObject( hFinishReport,0 );
}
bool CStreamFadeOff::HaveToRun()
{
	return WAIT_OBJECT_0 != WaitForSingleObject( hStopCommand,0 );
}
void CStreamFadeOff::Stop()
{
	if ( !pSFX ) return;
	bStopping = true;
	pSFX->StopStream( 0 );
	bStopping = false;
	SetEvent( hFinishReport );
}