#include "StdAfx.h"

#include "Fence.h"
#include "Updater.h"
#include "Diplomacy.h"
#include "TimeCounter.h"
#include "StaticObjectsIters.h"
BASIC_REGISTER_CLASS( CFence );
extern CDiplomacy theDipl;
extern CStaticObjects theStatObjs;
extern CUpdater updater;

extern CTimeCounter timeCounter;
CFence::CFence( const SFenceRPGStats *_pStats, const CVec2 &center, const int dbID, const float fHP, const int nFrameIndex, const int nDiplomacy, bool IsEditor )
: pStats( _pStats ), CCommonStaticObject( center, dbID, fHP, nFrameIndex, ESOT_FENCE ), eLifeType( ETOL_SAFE )
{
	nCreator = nDiplomacy == -1 ? theDipl.GetNeutralPlayer() : nDiplomacy;
	bSuspendAppear = 
		!IsEditor && !theDipl.IsNetGame() && theDipl.GetDiplStatus( nCreator, theDipl.GetMyNumber() ) == EDI_ENEMY;
}
bool CFence::ShouldSuspendAction( const EActionNotify &eAction ) const
{
	return
		!theDipl.IsEditorMode() &&
		( CCommonStaticObject::ShouldSuspendAction( eAction ) || eAction == ACTION_NOTIFY_NEW_ST_OBJ && bSuspendAppear );
}
void CFence::InitDirectionInfo()
{
	nDir = -1;
	for ( int i = 0; i < pStats->dirs.size(); ++i )
	{
		for ( int j = 0; j < pStats->dirs[i].centers.size(); ++j )
		{
			const int nStatsIndex = pStats->dirs[i].centers[j];
			if ( pStats->stats[nStatsIndex].nIndex == nFrameIndex )
				nDir = i;
		}
	}
	NI_ASSERT_T( nDir != -1, NStr::Format( "Can't find direction of fence (%s)\n", pStats->szKeyName.c_str() ) );

	CTilesSet tiles;
	GetCoveredTiles( &tiles );

	rightTile = leftTile = tiles.front();

	if ( nDir == 0 || nDir == 2 )
	{
		for ( CTilesSet::const_iterator iter = tiles.begin(); iter != tiles.end(); ++iter )
		{
			const SVector &tile = *iter;
			if ( tile.y < leftTile.y )
				leftTile = tile;
			if ( tile.y > rightTile.y )
				rightTile = tile;
		}

		NI_ASSERT_T( leftTile.x == rightTile.x,
			NStr::Format( 
				"Can't recognize fence %s, vertical direction %d, upper tile is (%d,%d), lower tile is (%d,%d)",
				GetStats()->szKeyName, nDir, leftTile.x, leftTile.y, rightTile.x, rightTile.y 
			) 
		);
		NI_ASSERT_T( leftTile != rightTile,
			NStr::Format( 
				"Can't recognize fence %s, horizontal direction %d, left tile is (%d,%d), right tile is (%d,%d)",
				GetStats()->szKeyName.c_str(), nDir, leftTile.x, leftTile.y, rightTile.x, rightTile.y 
			) 
		);
	}
	else
	{
		for ( CTilesSet::const_iterator iter = tiles.begin(); iter != tiles.end(); ++iter )
		{
			const SVector &tile = *iter;
			if ( tile.x < leftTile.x )
				leftTile = tile;
			if ( tile.x > rightTile.x )
				rightTile = tile;
		}

		NI_ASSERT_T( leftTile.y == rightTile.y,
			NStr::Format( 
				"Can't recognize fence %s, horizontal direction %d, left tile is (%d,%d), right tile is (%d,%d)",
				GetStats()->szKeyName, nDir, leftTile.x, leftTile.y, rightTile.x, rightTile.y 
			) 
		);
		NI_ASSERT_T( leftTile != rightTile,
			NStr::Format( 
				"Can't recognize fence %s, horizontal direction %d, left tile is (%d,%d), right tile is (%d,%d)",
				GetStats()->szKeyName.c_str(), nDir, leftTile.x, leftTile.y, rightTile.x, rightTile.y 
			) 
		);
	}
}
void CFence::AnalyzeConnection( CFence *pFence )
{
	if ( rightTile == pFence->leftTile )
		pFence->AnalyzeConnection( this );
	else if ( rightTile == pFence->rightTile || leftTile == pFence->rightTile || leftTile == pFence->leftTile )
	{
		neighFences.push_back( pFence );
		pFence->neighFences.push_back( this );
		
		if ( rightTile == pFence->rightTile )
		{
			if ( pFence->nDir == 0 || pFence->nDir == 3 )
				dirToBreak.push_back( ETOL_RIGHT );
			else
				dirToBreak.push_back( ETOL_LEFT );

			if ( nDir == 0 || nDir == 3 )
				pFence->dirToBreak.push_back( ETOL_RIGHT );
			else
				pFence->dirToBreak.push_back( ETOL_LEFT );
		}
		else if ( leftTile == pFence->rightTile || leftTile == pFence->leftTile )
		{
			if ( nDir == 0 || nDir == 3 )
				pFence->dirToBreak.push_back( ETOL_LEFT );
			else
				pFence->dirToBreak.push_back( ETOL_RIGHT );

			if ( leftTile == pFence->rightTile )
			{
				if ( pFence->nDir == 0 || pFence->nDir == 3 )
					dirToBreak.push_back( ETOL_RIGHT );
				else
					dirToBreak.push_back( ETOL_LEFT );
			}
			else
			{
				if ( pFence->nDir == 0 || pFence->nDir == 3 )
					dirToBreak.push_back( ETOL_LEFT );
				else
					dirToBreak.push_back( ETOL_RIGHT );
			}
		}
	}
}
void CFence::Init()
{
	timeCounter.Count( 0, true );

	CCommonStaticObject::Init();
	InitDirectionInfo();

	SRect boundRect;
	GetBoundRect( &boundRect );
	const float fR = fabs( boundRect.v1 - boundRect.v3 ) * 2.0f;
	for ( CStObjCircleIter<false> iter( GetCenter(), fR ); !iter.IsFinished(); iter.Iterate() )
	{
		CExistingObject *pObj = *iter;
		if ( pObj->GetObjectType() == ESOT_FENCE && pObj != this )
		{
			NI_ASSERT_T( dynamic_cast<CFence*>( pObj ) != 0, "Wrong fence" );
			if ( std::find( neighFences.begin(), neighFences.end(), pObj ) == neighFences.end() )
				AnalyzeConnection( static_cast<CFence*>( pObj ) );
		}
	}

	timeCounter.Count( 0, false );
}
void CFence::Delete()
{
	if ( eLifeType != ETOL_DESTROYED )
	{
		UnlockTiles();
		RemoveTransparencies();
		
		const int nDestroyedSize = pStats->dirs[nDir].cdamages.size();
		if ( nDestroyedSize == 0 )
		{
			if ( GetGlobalVar("report", 0) == 1 )
			{
				GetSingleton<IConsoleBuffer>()->WriteASCII
				(
					CONSOLE_STREAM_CONSOLE,
					NStr::Format("Can't find full damaged stats for %s, direction %d", pStats->szKeyName, nDir ),
					0xffff0000, true 
				);
			}
		}
		else
		{
			nFrameIndex = pStats->dirs[nDir].cdamages[ Random( 0, nDestroyedSize - 1 ) ];
			updater.Update( ACTION_NOTIFY_CHANGE_FRAME_INDEX, this, nFrameIndex );
		}

		eLifeType = ETOL_DESTROYED;
		updater.Update( ACTION_NOTIFY_SILENT_DEATH, this );

		InitTransparenciesPossibility();
		SetTransparencies();
		LockTiles();
	}

	NI_ASSERT_T( neighFences.size() == dirToBreak.size(), "Wrong fence neighbours" );

	std::list< CPtr<CFence> >::iterator fenceIter = neighFences.begin();
	std::list<ETypesOfLife>::iterator dirIter = dirToBreak.begin();

	while ( fenceIter != neighFences.end() )
	{
		(*fenceIter)->DamagePartially( *dirIter );

		++fenceIter;
		++dirIter;
	}
}
void CFence::Die( const float fDamage )
{
	Delete();
}
void CFence::DamagePartially( const ETypesOfLife eType )
{
 	if ( eType != eLifeType && eLifeType != ETOL_DESTROYED )
	{
		UnlockTiles();
		RemoveTransparencies();

		if ( eLifeType == ETOL_SAFE )
		{
			if ( eType == ETOL_LEFT )
			{
				const int nLeftSize = pStats->dirs[nDir].ldamages.size();

				if ( nLeftSize == 0 )
				{
					if ( GetGlobalVar("report", 0) == 1 )
					{
						GetSingleton<IConsoleBuffer>()->WriteASCII
						(
							CONSOLE_STREAM_CONSOLE, 
							NStr::Format("Can't find left damaged stats for %s, direction %d", pStats->szParentName.c_str(), nDir ),
							0xffff0000, true 
						);
					}
				}
				else
					nFrameIndex = pStats->dirs[nDir].ldamages[ Random( 0, nLeftSize - 1 ) ];
				eLifeType = ETOL_LEFT;
			}
			else
			{
				const int nRightSize = pStats->dirs[nDir].rdamages.size();
				if ( nRightSize == 0 )
				{
					if ( GetGlobalVar("report", 0) == 1 )
					{
						GetSingleton<IConsoleBuffer>()->WriteASCII
						(
							CONSOLE_STREAM_CONSOLE,
							NStr::Format("Can't find right damaged stats for %s, direction %d", pStats->szParentName.c_str(), nDir ),
							0xffff0000, true 
						);
					}
				}
				else
					nFrameIndex = pStats->dirs[nDir].rdamages[ Random( 0, nRightSize - 1 ) ];

				eLifeType = ETOL_RIGHT;
			}
		}
		else
		{
			const int nDestroyedSize = pStats->dirs[nDir].cdamages.size();

			if ( nDestroyedSize == 0 )
			{
				if ( GetGlobalVar("report", 0) == 1 )
				{
					GetSingleton<IConsoleBuffer>()->WriteASCII
					(
						CONSOLE_STREAM_CONSOLE,
						NStr::Format("Can't find full damaged stats for %s, direction %d", pStats->szParentName.c_str(), nDir ),
						0xffff0000, true 
					);
				}
			}
			else
				nFrameIndex = pStats->dirs[nDir].cdamages[ Random( 0, nDestroyedSize - 1 ) ];

			updater.Update( ACTION_NOTIFY_SILENT_DEATH, this );
			eLifeType = ETOL_DESTROYED;
		}

		InitTransparenciesPossibility();
		SetTransparencies();
		LockTiles();

		updater.Update( ACTION_NOTIFY_CHANGE_FRAME_INDEX, this, nFrameIndex );
	}
}
bool CFence::CanUnitGoThrough( const EAIClass &eClass ) const
{
	return ( pStats->dwAIClasses & eClass ) == 0;
}
