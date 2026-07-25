#include "stdafx.h"

#include "StaticMembers.h"
#include "Commands.h"
#include "SoldierStates.h"
#include "InBuildingStates.h"
#include "InTransportStates.h"
#include "InEntrenchmentStates.h"
#include "PlaneStates.h"
#include "TankStates.h"
#include "TransportStates.h"
#include "LinkObject.h"
#include "FormationStates.h"
#include "ArtilleryStates.h"
#include "ArtRocketStates.h"
#include "Entrenchment.h"
#include "Soldier.h"
int CStaticMembers::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &CQueueUnit::cmds );
	saver.Add( 2, &CAICommand::paths );
	saver.Add( 3, &CAICommand::cmdIds );

	saver.Add( 4, &CInBuildingStatesFactory::pFactory );
	saver.Add( 6, &CInEntrenchmentStatesFactory::pFactory );
	saver.Add( 8, &CInTransportStatesFactory::pFactory );
	saver.Add( 10, &CPlaneStatesFactory::pFactory );
	saver.Add( 12, &CSoldierStatesFactory::pFactory );
	saver.Add( 15, &CTankStatesFactory::pFactory );
	saver.Add( 16, &CTransportStatesFactory::pFactory );
	saver.Add( 17, &CFormationStatesFactory::pFactory );

	saver.Add( 18, &CLinkObject::Link2Object() );
	saver.Add( 19, &CLinkObject::DeletedObjects() );
	saver.Add( 20, &CLinkObject::UnitsID2Object() );
	saver.Add( 21, &CLinkObject::nCurUniqueID );
	saver.Add( 22, &CLinkObject::DeletedUniqueObjects() );

	
	saver.Add( 23, &CArtilleryStatesFactory::pFactory );
	saver.Add( 24, &CArtRocketStatesFactory::pFactory	);

	saver.Add( 26, &CExistingObject::globalMark );

	return 0;
}
