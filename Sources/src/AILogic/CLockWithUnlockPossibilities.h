#ifndef __CLockWithUnlockPossibilities_h__
#define __CLockWithUnlockPossibilities_h__

#pragma ONCE
#include "RectTiles.h"

class CAIUnit;
class CLockWithUnlockPossibilities
{
	DECLARE_SERIALIZE;
	
	void Unlock();
	void Lock();

	std::vector<BYTE> formerTilesType;
	CTilesSet pathTiles;

	bool bLocked;
	BYTE bAIClass; //
protected:
	SRect bigRect; // весь этот Rect будет пройден танком.
	bool TryLockAlongTheWay( const bool bLock, const BYTE _bAIClass ) ;

public:
	CLockWithUnlockPossibilities()
		: bLocked(false){}

	bool LockRect( const SRect &rect, const BYTE _bAIClass )
	{
		if ( bLocked )	
		{
			TryLockAlongTheWay(false, bAIClass );
		}
		bigRect = rect;
		bLocked = TryLockAlongTheWay( true, _bAIClass );
		return bLocked ;
	}
	void UnlockIfLocked()
	{
		if ( bLocked )
		{
			bLocked = false;
			TryLockAlongTheWay(false, bAIClass);
		}
	}
};
#endif // __CLockWithUnlockPossibilities_h__
