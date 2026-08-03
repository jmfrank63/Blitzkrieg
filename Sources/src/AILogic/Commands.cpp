#include "StdAfx.h"

#include "Commands.h"
#include "CommonUnit.h"
#include "PathFinder.h"
extern NTimer::STime curTime;
CQueuesSet<SGroupPathInfo> CAICommand::paths;
CFreeIds CAICommand::cmdIds;
BASIC_REGISTER_CLASS( CAICommand );
CAICommand::CAICommand( const SAIUnitCmd &_unitCmd )
 : unitCmd( _unitCmd ), nFlag( -1 )
{ 
	InitCmdId(); 
}
CAICommand::CAICommand( const CAICommand &cmd )
: unitCmd( cmd.unitCmd ), nFlag( cmd.nFlag )
{
	InitCmdId();
}
void CAICommand::InitCmdId()
{
	id = cmdIds.GetFreeId();
	if ( id >= paths.GetQueuesNum() )
		paths.IncreaseQueuesNum( id * 1.5 );
}
IStaticPath* CAICommand::CreateStaticPath( CCommonUnit *pUnit )
{
	if ( !pUnit->CanMove() )
		return 0;
	else if ( pUnit->GetSubGroup() < 0 || pUnit->IsTrain() )
		return CreateStaticPathToPoint( unitCmd.vPos, pUnit->GetGroupShift(), pUnit );
	else
	{
		for ( int i = paths.begin( id ); i != paths.end(); i = paths.GetNext( i ) )
		{
			SGroupPathInfo& pathInfo = paths.GetEl( i );

			if ( pathInfo.nSubGroup == pUnit->GetSubGroup() &&
					 pathInfo.pPathFinder.GetPtr() == pUnit->GetPathFinder() && 
					 pathInfo.cTileSize == pUnit->GetBoundTileRadius() &&
					 pathInfo.aiClass == pUnit->GetAIClass() )
				return pathInfo.pPath;
		}

		SGroupPathInfo pathInfo;
		pathInfo.nSubGroup = pUnit->GetSubGroup();
		pathInfo.cTileSize = pUnit->GetBoundTileRadius();
		pathInfo.aiClass = pUnit->GetAIClass();
		pathInfo.pPathFinder = pUnit->GetPathFinder();
		pathInfo.pPath = CreateStaticPathToPoint( unitCmd.vPos, -pUnit->GetGroupShift(), pUnit );

		paths.Push( id, pathInfo );
		return pathInfo.pPath;
	}
}
