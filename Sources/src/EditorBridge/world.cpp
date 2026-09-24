#include "StdAfx.h"
#include "world.h"
#include "../Main/GameTimer.h"
#include "../Misc/HPTimer.h"

void CEditorWorld::UpdateNow()
{
	IGameTimer *pGameTimer = GetSingleton<IGameTimer>();
	if ( pGameTimer == 0 )
		return;
	NHPTimer::STime hptime;
	NHPTimer::GetTime( &hptime );
	pGameTimer->Update( DWORD( NHPTimer::GetSeconds( hptime ) * 1000.0f ) );
	Update( pGameTimer->GetGameTime() );
}
