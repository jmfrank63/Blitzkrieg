#include "StdAfx.h"

#include "Passages.h"
#include "AIUnit.h"
#include "AIStaticMap.h"
#include "StandartPath.h"
#include "ValidObjectCheck.h"
extern CStaticMap theStaticMap;
extern NTimer::STime curTime;
CPassages thePassages;
static bool PassageTrace()
{
	static const bool bTrace = getenv( "BK_PASSAGE_TRACE" ) != 0;
	return bTrace;
}
// How far, in tiles either way, a tile is from a box of tiles (0 inside it).
static int DistanceToBox( const SVector &tile, const SVector &vMin, const SVector &vMax )
{
	const int dx = Max( 0, Max( vMin.x - tile.x, tile.x - vMax.x ) );
	const int dy = Max( 0, Max( vMin.y - tile.y, tile.y - vMax.y ) );
	return Max( dx, dy );
}
// Free tiles beside a path tile across the way it goes, up to nCap each side.
static int FreeTilesAside( const SVector &tile, const SVector &vAcross, const int nCap, const BYTE aiClass )
{
	int n = 0;
	while ( n < nCap && theStaticMap.CanUnitGo( 0, SVector( tile.x + vAcross.x * ( n + 1 ), tile.y + vAcross.y * ( n + 1 ) ), aiClass ) )
		++n;
	return n;
}
int SPassageOnPath::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &nPassage );
	saver.Add( 2, &vMin );
	saver.Add( 3, &vMax );
	return 0;
}
int CPassages::SWaiter::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pUnit );
	saver.Add( 2, &arrival );
	return 0;
}
int CPassages::SPassage::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &vMin );
	saver.Add( 2, &vMax );
	saver.Add( 3, &pOccupant );
	saver.Add( 4, &bInside );
	saver.Add( 5, &timeEntered );
	saver.Add( 6, &waiting );
	return 0;
}
int CPassages::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &passages );
	return 0;
}
// A narrow place another path already crossed (their tiles touch) is that
// passage, grown by these tiles; else a new one.
int CPassages::Register( const SVector &vMin, const SVector &vMax )
{
	for ( int i = 0; i < passages.size(); ++i )
	{
		SPassage &passage = passages[i];
		if ( vMin.x <= passage.vMax.x + 1 && vMax.x >= passage.vMin.x - 1 && vMin.y <= passage.vMax.y + 1 && vMax.y >= passage.vMin.y - 1 )
		{
			passage.vMin = SVector( Min( passage.vMin.x, vMin.x ), Min( passage.vMin.y, vMin.y ) );
			passage.vMax = SVector( Max( passage.vMax.x, vMax.x ), Max( passage.vMax.y, vMax.y ) );
			return i;
		}
	}
	passages.push_back( SPassage() );
	SPassage &passage = passages.back();
	passage.vMin = vMin;
	passage.vMax = vMax;
	passage.bInside = false;
	passage.timeEntered = 0;
	if ( PassageTrace() )
		fprintf( stderr, "BK_PASSAGE_TRACE: %u ms passage %d new box %d %d %d %d\n", unsigned( curTime ), int( passages.size() ) - 1, vMin.x, vMin.y, vMax.x, vMax.y );
	return passages.size() - 1;
}
// A path tile is narrow when the way there is not wide enough for two of this
// vehicle side by side: fewer than 2 (2r + 1) free tiles across it, r the
// vehicle's bound radius, counted on the static map (units do not narrow a
// way: the 2003 collisions move them).
void CPassages::FindOnPath( CAIUnit *pUnit, IStaticPath *pPath, std::vector<SPassageOnPath> *pOnPath )
{
	pOnPath->clear();
	CCommonStaticPath *pCommonPath = dynamic_cast<CCommonStaticPath*>( pPath );
	if ( pCommonPath == 0 || pCommonPath->GetLength() < 2 )
		return;
	const int nLane = 2 * pUnit->GetBoundTileRadius() + 1;
	const BYTE aiClass = pUnit->GetAIClass();

	theStaticMap.MemMode();
	theStaticMap.SetMode( ELM_STATIC );
	const int nLength = pCommonPath->GetLength();
	int nRunStart = -1;
	SVector vMin, vMax;
	for ( int i = 0; i <= nLength; ++i )
	{
		bool bNarrow = false;
		SVector tile( 0, 0 );
		if ( i < nLength )
		{
			tile = pCommonPath->GetTile( i );
			const SVector next = pCommonPath->GetTile( i + 1 < nLength ? i + 1 : i - 1 );
			const SVector vStep( Sign( next.x - tile.x ), Sign( next.y - tile.y ) );
			const SVector vAcross( -vStep.y, vStep.x );
			bNarrow = 1 + FreeTilesAside( tile, vAcross, 2 * nLane, aiClass ) + FreeTilesAside( tile, SVector( -vAcross.x, -vAcross.y ), 2 * nLane, aiClass ) < 2 * nLane;
		}
		if ( bNarrow )
		{
			if ( nRunStart < 0 )
			{
				nRunStart = i;
				vMin = vMax = tile;
			}
			vMin = SVector( Min( vMin.x, tile.x ), Min( vMin.y, tile.y ) );
			vMax = SVector( Max( vMax.x, tile.x ), Max( vMax.y, tile.y ) );
		}
		else if ( nRunStart >= 0 )
		{
			// A narrow place where the path begins is one the vehicle is in already.
			if ( nRunStart > 0 )
			{
				SPassageOnPath onPath;
				onPath.nPassage = Register( vMin, vMax );
				onPath.vMin = vMin;
				onPath.vMax = vMax;
				pOnPath->push_back( onPath );
			}
			nRunStart = -1;
		}
	}
	theStaticMap.RestoreMode();
}
void CPassages::RemoveWaiter( SPassage &passage, CAIUnit *pUnit )
{
	for ( std::list<SWaiter>::iterator it = passage.waiting.begin(); it != passage.waiting.end(); )
	{
		if ( it->pUnit == pUnit )
			it = passage.waiting.erase( it );
		else
			++it;
	}
}
void CPassages::Release( const int nPassage, const char *pszWhy )
{
	SPassage &passage = passages[nPassage];
	if ( PassageTrace() && passage.pOccupant != 0 )
		fprintf( stderr, "BK_PASSAGE_TRACE: %u ms passage %d %s unit %d\n", unsigned( curTime ), nPassage, pszWhy, passage.pOccupant->GetUniqueId() );
	passage.pOccupant = 0;
	passage.bInside = false;
}
bool CPassages::MustWait( CAIUnit *pUnit, std::vector<SPassageOnPath> *pOnPath )
{
	const int nRadius = pUnit->GetBoundTileRadius();
	const SVector tile = AICellsTiles::GetTile( pUnit->GetCenter() );
	while ( !pOnPath->empty() )
	{
		const SPassageOnPath &onPath = pOnPath->front();
		SPassage &passage = passages[onPath.nPassage];
		// Inside: its body (radius r) overlaps the passage's tiles.
		const int nDistance = DistanceToBox( tile, passage.vMin, passage.vMax );
		if ( passage.pOccupant == pUnit )
		{
			if ( nDistance <= nRadius )
			{
				passage.bInside = true;
				return false;
			}
			if ( !passage.bInside )
				return false;
			// Through: the next one waits for this one no more.
			Release( onPath.nPassage, "leave" );
			pOnPath->erase( pOnPath->begin() );
			continue;
		}
		// In it without having waited (it stood there when ordered): not this one's to wait for.
		if ( nDistance <= nRadius )
		{
			RemoveWaiter( passage, pUnit );
			pOnPath->erase( pOnPath->begin() );
			continue;
		}
		// Waits two tiles before the passage, its body clear of it.
		if ( nDistance > nRadius + 2 )
			return false;

		bool bQueued = false;
		for ( std::list<SWaiter>::const_iterator it = passage.waiting.begin(); it != passage.waiting.end() && !bQueued; ++it )
			bQueued = it->pUnit == pUnit;
		if ( !bQueued )
		{
			std::list<SWaiter>::iterator it = passage.waiting.begin();
			while ( it != passage.waiting.end() && ( it->arrival < curTime || ( it->arrival == curTime && it->pUnit->GetUniqueId() < pUnit->GetUniqueId() ) ) )
				++it;
			SWaiter waiter;
			waiter.pUnit = pUnit;
			waiter.arrival = curTime;
			passage.waiting.insert( it, waiter );
			if ( PassageTrace() )
				fprintf( stderr, "BK_PASSAGE_TRACE: %u ms passage %d arrive unit %d\n", unsigned( curTime ), onPath.nPassage, pUnit->GetUniqueId() );
		}
		if ( passage.pOccupant == 0 && passage.waiting.front().pUnit == pUnit )
		{
			passage.waiting.pop_front();
			passage.pOccupant = pUnit;
			passage.bInside = false;
			passage.timeEntered = curTime;
			if ( PassageTrace() )
				fprintf( stderr, "BK_PASSAGE_TRACE: %u ms passage %d enter unit %d\n", unsigned( curTime ), onPath.nPassage, pUnit->GetUniqueId() );
			return false;
		}
		return true;
	}
	return false;
}
void CPassages::Leave( CAIUnit *pUnit )
{
	for ( int i = 0; i < passages.size(); ++i )
	{
		RemoveWaiter( passages[i], pUnit );
		if ( passages[i].pOccupant == pUnit )
			Release( i, "leave" );
	}
}
// An occupant that is dead, or has held a passage longer than any vehicle
// takes to cross one, gives it back: no queue waits for ever.
void CPassages::Segment()
{
	for ( int i = 0; i < passages.size(); ++i )
	{
		SPassage &passage = passages[i];
		for ( std::list<SWaiter>::iterator it = passage.waiting.begin(); it != passage.waiting.end(); )
		{
			if ( !IsValidObj( it->pUnit ) )
				it = passage.waiting.erase( it );
			else
				++it;
		}
		if ( passage.pOccupant != 0 && !IsValidObj( passage.pOccupant ) )
			Release( i, "dead" );
		else if ( passage.pOccupant != 0 && curTime - passage.timeEntered > OCCUPY_TIMEOUT )
			Release( i, "timeout" );
	}
}
