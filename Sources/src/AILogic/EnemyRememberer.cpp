#include "stdafx.h"

#include "EnemyRememberer.h"
#include "CommonUnit.h"
BASIC_REGISTER_CLASS( CEnemyRememberer );
extern NTimer::STime curTime;
CEnemyRememberer::CEnemyRememberer( const int timeBeforeForget ) 
: timeBeforeForget( timeBeforeForget )
{
}
void CEnemyRememberer::SetVisible( const class CCommonUnit *pUnit, const bool bVisible )
{
	if ( bVisible )
	{
		timeLastSeen = 0;
	}
	else
	{
		timeLastSeen = curTime;
		vPosition = pUnit->GetCenter();
	}
}
const CVec2 & CEnemyRememberer::GetPos( const class CCommonUnit *pUnit ) const
{
	if ( timeLastSeen == 0  )
		return pUnit->GetCenter();
	else
		return vPosition;
}
const bool CEnemyRememberer::IsTimeToForget() const
{
	return timeLastSeen && curTime - timeLastSeen > timeBeforeForget;
}