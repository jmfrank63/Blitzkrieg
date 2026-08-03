#include "StdAfx.h"

#include "AnimUnitSoldier.h"
#include "AnimUnitMech.h"
#include "SerializeOwner.h"
int CAnimUnitMech::SMovingState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &state );
	saver.Add( 2, &timeOfIntentionStart );

	return 0;
}
int CAnimUnitMech::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pOwner, &saver );
	saver.Add( 5, &movingState );

	return 0;
}
int CAnimUnitSoldier::SMovingState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &state );
	saver.Add( 2, &timeOfIntentionStart );

	return 0;
}
int CAnimUnitSoldier::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	SerializeOwner( 1, &pOwner, &saver );
	saver.Add( 2, &pOwnerStats );
	saver.Add( 3, &bComplexAttack );

	saver.Add( 4, &nCurAnimation );
	saver.Add( 5, &timeOfFinishAnimation );
	saver.Add( 6, &bMustFinishCurAnimation );
	saver.Add( 7, &movingState );

	return 0;
}
