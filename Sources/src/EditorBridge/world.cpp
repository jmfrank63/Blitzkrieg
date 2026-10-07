#include "StdAfx.h"
#include "world.h"
#include "../Main/GameTimer.h"
#include "../Platform/Clock.h"

void CEditorWorld::UpdateNow()
{
	IGameTimer *pGameTimer = GetSingleton<IGameTimer>();
	if ( pGameTimer == 0 )
		return;
	pGameTimer->Update( NPlatform::MonotonicMilliseconds() );
	Update( pGameTimer->GetGameTime() );
}

void CEditorWorld::GetObjects( std::vector<SMapObject*> *pObjects )
{
	pObjects->clear();
	for ( iterator it = begin(); it != end(); ++it )
		pObjects->push_back( const_cast<SMapObject*>( static_cast<const SMapObject*>( it ) ) );
}
