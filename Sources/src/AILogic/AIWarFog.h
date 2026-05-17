#ifndef __AI_WAR_FOG_H__
#define __AI_WAR_FOG_H__

#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Heap.h"
#include "SuspendedUpdates.h"
#include "Diplomacy.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CExistingObject;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CWarFog
{
	// parameters of a region
	SVector corners[4];
	int minSum, maxSum, minDiff, maxDiff;
	int sizeSum, sizeDiff;

	// parameters of a unit
	SVector center;
	int r;

	// parameters for tracing
	bool isInside;
	SLine plusRay, minusRay;

	// visibilities
	std::vector<BYTE> vis;
	
	// statics
	typedef BYTE& ( CWarFog:: *TVisFunc )( const SVector &point );
	const static TVisFunc visFuncs[2];

	typedef bool ( CWarFog:: *TSegmTypes )( const int n ) const;
	const static TSegmTypes checkSegms[9][9];
	//

	// functions for determination if the unit influences on visibilities
	bool CSf( const int n ) const { return false; }
	bool CSt( const int n ) const { return true; }
	bool CS1( const int n ) const 
	{
		NI_ASSERT_SLOW_T( n == minSum || n == maxSum, "Wrong n" );
		return ( n >= center.x+center.y-2*r ) && ( n <= center.x+center.y+2*r );
	}
	bool CS2( const int n ) const
	{
		NI_ASSERT_SLOW_T( n == minDiff || n == maxDiff, "Wrong n" );
		return ( n >= center.x-center.y-2*r ) && ( n <= center.x-center.y+2*r );
	}

	bool CheckSegm( const SVector &p1, const SVector &p2, const int n ) const;
	bool IsInfluencedUnit( const struct SVector &center ) const;

	// determination of rays for checking if the ray must be traced
	void DetermineRegion();

	// manipulation with vis array
	BYTE& VisFunc0( const SVector &point ) { return vis[0]; }
	BYTE& VisFunc1( const SVector &point ) { return vis[(point.x+point.y-minSum)+(point.x-point.y-minDiff)*sizeSum+1]; }
	BYTE& Vis( const SVector &point ) { return (this->*visFuncs[IsPointInside( point )])( point ); }

	BYTE GetTileVis( const SVector &tile, const int nParty ) { return Vis( tile ); }
public:
	// initialization for war of fog determination
	void Init( const struct SVector &upLeft, const struct SVector &downLeft, 
						 const struct SVector &downRight, const struct SVector &upRight );

	// initialization for units visibilities determination
	void InitVisCheck( const struct SVector &upLeft, const struct SVector &downLeft, 
								  	 const struct SVector &downRight, const struct SVector &upRight );

	// if the point is inside of region
	bool IsPointInside( const SVector &point ) const 
	{ 
		return point.x+point.y >= minSum && point.x+point.y <= maxSum && point.x-point.y >= minDiff && point.x-point.y <= maxDiff;
	}

	void AddUnit( const struct SFogInfo &fogInfo );
	
	void GetVisibilities( struct SAIVisInfo **pVisBuffer, int *pnLen );

	// if the ray must be traced?
	bool CanTraceRay( const SVector &point ) const
	{ 
		return isInside || plusRay.GetHPLineSign( point ) >= 0 && minusRay.GetHPLineSign( point ) <= 0; 
	}
	
	bool VisitPoint( const SVector &point, const int vis, const float fLen2, const float fR2, const float fSightPower2 )
	{
		if ( vis > Vis( point ) )
			Vis( point ) = vis;

		return true;
	}

	static int TraceToPointForScan( const SVector &center, const SVector &finishPoint );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFogInfo
{
	DECLARE_SERIALIZE;
public:
	SVector center;
	WORD wUnitDir;
	bool bAngleLimited;
	CPtr<CExistingObject> pObject;
	bool bPlane;
	float fSightPower;

	union
	{
		struct
		{
			WORD r;
			WORD wVisionAngle, wMinAngle, wMaxAngle;
		};
		long values;
	};

	SFogInfo() : fSightPower( 1.0f ) { }
	SFogInfo( const SVector &_center, const WORD _r, const WORD _wUnitDir, const WORD _wVisionAngle, const bool _bPlane, const float _fSightPower ) 
	: center( _center ), r( _r ), wUnitDir( _wUnitDir ), wVisionAngle( _wVisionAngle ), bAngleLimited( false ), wMinAngle( 0 ), wMaxAngle( 0 ), bPlane( _bPlane ), fSightPower( _fSightPower ) { }
	SFogInfo( const SVector &_center, const WORD _r, const WORD _wUnitDir, const WORD _wVisionAngle, bool _bAngleLimited, const WORD _wMinAngle, const WORD _wMaxAngle, const bool _bPlane, const float _fSightPower )
	: center( _center ), r( _r ), wUnitDir( _wUnitDir ), wVisionAngle( _wVisionAngle ), bAngleLimited( _bAngleLimited ), wMinAngle( _wMinAngle ), wMaxAngle( _wMaxAngle ), bPlane( _bPlane ), fSightPower( _fSightPower ) { }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern class CGlobalWarFog theWarFog;
extern CSuspendedUpdates theSuspendedUpdates;
extern CDiplomacy theDipl;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CGlobalWarFog
{
	DECLARE_SERIALIZE;

	struct SDelWarFog
	{
		const int nParty;

//#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
//		std::list<SVector> *pTiles;
//#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		//
		explicit SDelWarFog( const int _nParty ) : nParty( _nParty ) { }

//#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
//		SDelWarFog( const int _nParty, std::list<SVector> *_pTiles ) : pTiles( _pTiles ), nParty( _nParty ) { }
//#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)

		bool CanTraceRay( const SVector &point ) const { return true; }
		bool VisitPoint( const SVector &point, const int vis, const float fLen2, const float fR2, const float fSightPower2 )
		{

//#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
//			pTiles->push_back( point );
//#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)

			if ( theWarFog.fogCnts[nParty][point.y][point.x] > 0 )
			{
				--theWarFog.fogCnts[nParty][point.y][point.x];
				if ( theWarFog.fogCnts[nParty][point.y][point.x] == 0 )
				{
					theWarFog.maxVis[nParty].SetData( point.x, point.y, 0 );
					theWarFog.minCoeff2[nParty][point.y][point.x] = floatToByte( 1.0f );
				}
			}

			return true;
		}
	};

	struct SAddWarFog
	{
		const int nParty;

//#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
//		std::list<SVector> *pTiles;
//#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)

		//
		explicit SAddWarFog( const int _nParty ) : nParty( _nParty ) { }

//#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
//		SAddWarFog( const int _nParty, std::list<SVector> *_pTiles ) : pTiles( _pTiles ), nParty( _nParty ) { }
//#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)

		bool CanTraceRay( const SVector &point ) const { return true; }
		bool VisitPoint( const SVector &point, const int vis, const float fLen2, const float fR2, const float fSightPower2 )
		{

//#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
//			pTiles->push_back( point );
//#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)

			// увидели тайл, который был до этого невидим
			if ( theWarFog.fogCnts[nParty][point.y][point.x] == 0 )
				theSuspendedUpdates.TileBecameVisible( point, nParty );
			
			++theWarFog.fogCnts[nParty][point.y][point.x];

			if ( theWarFog.maxVis[nParty].GetData( point.x, point.y ) < vis )
				theWarFog.maxVis[nParty].SetData( point.x, point.y, vis );

			const float fRatio = fSightPower2 * fLen2 / fR2;
			if ( byteToFloat( theWarFog.minCoeff2[nParty][point.y][point.x] ) > fRatio )
				theWarFog.minCoeff2[nParty][point.y][point.x] = floatToByte( fRatio );

			return true;
		}
	};

	// зависит от клиента!
	struct SScriptAreaFog
	{
		bool bOpen;

		//
		SScriptAreaFog( bool _bOpen ) : bOpen( _bOpen ) { }

		bool CanTraceRay( const SVector &point ) const { return true; }
		bool VisitPoint( const SVector &point, const int vis, const float fLen2, const float fR2, const float fSightPower2 )
		{
			if ( bOpen )
			{
				theWarFog.areasOpenTiles.SetData( point.x, point.y );
				theSuspendedUpdates.TileBecameVisible( point, theDipl.GetMyParty() );
			}
			else
				theWarFog.areasOpenTiles.RemoveData( point.x, point.y );

			return true;
		}
	};

	struct SUnitInfo
	{
		DECLARE_SERIALIZE;

		public:		

//#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
//		std::list<SVector> addedTiles;
//		std::list<SVector> deletedTiles;
//#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)

			SFogInfo fogInfo;
			int nHeapPos;
			int nParty;

			SUnitInfo() { }
	};

	struct SWeightInfo
	{
		int nWeight;
		int id;

		SWeightInfo() { }
		SWeightInfo( const int _nWeight, const int _id ) : nWeight( _nWeight ), id( _id ) { }
	};

	struct SCommonSort
	{
		bool operator()( const SWeightInfo &a, const SWeightInfo &b ) { return a.nWeight < b.nWeight; }
	};

	struct SWillSwap
	{
		void operator()( const SWeightInfo &a, const SWeightInfo &b, const int nAIndex, const int nBIndex );
	};

	CHeap<SWeightInfo, SCommonSort, SWillSwap> weights;


	// сколько юнитов данной дипл. стороны видят тайл
	std::vector< CArray2D<WORD> > fogCnts;
	// максимальная видимость тайла для дипл. стороны
	std::vector<CArray2D4Bit> maxVis;
	
	// для камуфлирования - мин. из квадратов отношений < расст. от центра юнита / радиус видимости юнита >
	std::vector< CArray2D<BYTE> > minCoeff2;

	std::vector<SUnitInfo> unitsInfo;
	std::vector<SFogInfo> newUnitsInfo;

	typedef std::list< CObj<CExistingObject> > CObjectsList;
	// объекты, удалённые между перерасчётами тумана для юнита
	std::vector<CObjectsList> removedObjects4Units;
	// объекты, добавленные между перерасчётами тумана для юнита
	std::vector<CObjectsList> addedObjects4Units;

	struct SDeletedUnitInfo
	{
		SUnitInfo unitInfo;
		CObjectsList removedObjects;
		CObjectsList addedObjects;

		virtual int STDCALL operator&( interface IStructureSaver &ss )
		{
			CSaverAccessor saver = &ss;

			saver.Add( 1, &unitInfo );
			saver.Add( 2, &removedObjects );
			saver.Add( 3, &addedObjects );

			return 0;
		}
	};
	std::list<SDeletedUnitInfo> deletedUnits;
	std::list<int> newUnits;

	int nMiniMapY;
	std::vector<BYTE> miniMapSums;

	// open script areas
	std::unordered_set<std::string> areas;
	CArray2D1Bit areasOpenTiles;

	//
	void UpdateUnit( const int id, SFogInfo &newFogInfo, const int weight );

	void RemoveUnitWarfog( SUnitInfo &unitInfo, const CObjectsList &removedObjects, const CObjectsList &addedObjects );
	void AddUnitWarfog( SUnitInfo &unitInfo );
	void ProcessNewUnits( bool bAllUnits = false );
	void ProcessDeletedUnits( bool bAllUnits = false );

	static const int GetWeight( const SFogInfo &oldFog, const SFogInfo &newFog );
public:
	void Init();
	void Clear();

	void AddUnit( const int id, int nParty, const SFogInfo &fogInfo );
	void DeleteUnit( const int id );

	void ChangeUnitState( const int id, SFogInfo &newFogInfo );
	void ChangeUnitCoord( const int id, SFogInfo &newFogInfo );

	// пересчитать весь необходимый туман при добавлении/удалении юнита
	void ReclaculateFogAfterRemoveObject( class CExistingObject *pObj );
	void ReclaculateFogAfterAddObject( class CExistingObject *pObj );

	void ChangeUnitParty( const int id, const int nParty );
	void ProcessAllNewUnits() { ProcessNewUnits( true ); }

	void Segment( bool bAllUnits );

	bool IsTileVisible( const SVector &tile, const int nParty ) const;
	BYTE GetTileVis( const SVector &tile, const int nParty ) const { return GetTileVis( tile.x, tile.y, nParty ); }
	BYTE GetTileVis( const int tileX, const int tileY, const int nParty ) const;
	// для клиента, учитывая, играется hisotry или нет
	BYTE GetClientTileVis( const SVector &tile, const int nParty ) const { return GetClientTileVis( tile.x, tile.y, nParty ); }
	BYTE GetClientTileVis( const int tileX, const int tileY, const int nParty ) const;
	bool IsUnitVisible( const int nParty, const SVector &tile, bool bCamouflated, const float fCamouflage ) const;

	float GetMinCoeff2( const SVector &tile, const int nParty ) const { return byteToFloat( minCoeff2[nParty][tile.y][tile.x] ); }

	void GetMiniMapInfo( BYTE **pVisBuffer, int *pnLen, const int nParty, bool bFirstTime );
	
	void ToggleOpen4ScriptAreaTiles( const struct SScriptArea &scriptArea, bool bOpen );
	bool IsOpenBySriptArea( const int x, const int y ) const;
	bool IsOpenBySriptArea( const SVector &tile ) const { return IsOpenBySriptArea( tile.x, tile.y ); }

	// пересчитать туман для юнита id, когда удалили объект pObject
	void RecalculateForRemovedObject( const int id, const float fDist, class CExistingObject *pObject );
	// пересчитать туман для юнита id, когда добавили объект pObject
	void RecalculateForAddedObject( const int id, const float fDist, class CExistingObject *pObject );

	//
	friend struct SDelWarFog;
	friend struct SAddWarFog;
	friend struct SWillSwap;
	friend struct SScriptAreaFog;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern CGlobalWarFog theWarFog;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CGlobalWarFog::SWillSwap::operator()( const CGlobalWarFog::SWeightInfo &a, const CGlobalWarFog::SWeightInfo &b, const int nAIndex, const int nBIndex )
{
	std::swap( theWarFog.unitsInfo[a.id].nHeapPos, theWarFog.unitsInfo[b.id].nHeapPos );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __AI_WAR_FOG_H__
