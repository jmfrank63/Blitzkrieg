#include "StdAfx.h"

#include "ArtRocketStates.h"
#include "SerializeOwner.h"
int CArtRocketAttackGroundState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pArtillery, &saver );
	saver.Add( 2, &point );
	saver.Add( 3, &bFired );
	saver.Add( 4, &eState );
	saver.Add( 5, &bFinished );

	return 0;
}
