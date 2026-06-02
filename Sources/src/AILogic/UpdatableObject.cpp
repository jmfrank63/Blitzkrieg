#include "stdafx.h"

#include "UpdatableObject.h"
#include "AIStaticMap.h"
#include "Diplomacy.h"
#include "Cheats.h"
extern CStaticMap theStaticMap;
extern CDiplomacy theDipl;
extern SCheats theCheats;
BASIC_REGISTER_CLASS( IUpdatableObj );
float IUpdatableObj::GetTerrainHeight( const float x, const float y, const NTimer::STime timeDiff ) const
{
	return theStaticMap.GetVisZ( x, y );
}
const bool IUpdatableObj::IsVisibleByPlayer()
{
	return theCheats.IsHistoryPlaying() || IsVisible( theDipl.GetMyParty() );
}
