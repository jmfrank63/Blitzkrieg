#ifndef __ANTI_ARTILLERY_H__
#define __ANTI_ARTILLERY_H__

#pragma ONCE
#include "UpdatableObject.h"
#include "LinkObject.h"
class CRevealCircle : public CLinkObject
{
	OBJECT_COMPLETE_METHODS( CRevealCircle );
	DECLARE_SERIALIZE;

	CCircle circle;
public:
	CRevealCircle() { }
	CRevealCircle( const CVec2 &center, const float fR ) : circle( center, fR ) { SetUniqueId(); }
	CRevealCircle( const CCircle &_circle ) : circle( _circle ) { SetUniqueId(); }

	virtual void GetRevealCircle( CCircle *pCircle ) const { *pCircle = circle; }
	
	virtual const bool IsVisible( const BYTE party ) const { return true; }
	virtual void GetTilesForVisibility( CTilesSet *pTiles ) const { pTiles->clear(); }
	virtual bool ShouldSuspendAction( const EActionNotify &eAction ) const { return false; }
};
class CAntiArtillery : public CLinkObject
{
	OBJECT_COMPLETE_METHODS( CAntiArtillery );
	DECLARE_SERIALIZE;

	float fMaxRadius;
	int nParty;

	NTimer::STime lastScan;
	std::vector<NTimer::STime> lastShotTime;
	std::vector<NTimer::STime> lastRevealCircleTime;

	std::vector<float> closestEnemyDist2;
	std::vector<CVec2> lastHeardPos;
	std::vector<BYTE> nHeardShots;
	std::vector<CVec2> lastRevealCenter;

	void Scan( const CVec2 &center );
public:
	CAntiArtillery() { }
	explicit CAntiArtillery( class CAIUnit *pOwner );
	
	void SetParty( const int _nParty ) { nParty = _nParty; }

	void Init( const float fMaxRadius, const int nParty );
	void Fired( const float fGunRadius, const CVec2 &center );

	void Segment( bool bOwnerVisible );

	const CCircle GetRevealCircle( const int nParty ) const;
	const NTimer::STime GetLastHeardTime( const int nParty ) const;

	virtual const bool IsVisible( const BYTE party ) const { return true; }
	virtual void GetTilesForVisibility( CTilesSet *pTiles ) const { pTiles->clear(); }
	virtual bool ShouldSuspendAction( const EActionNotify &eAction ) const { return false; }
	
	friend struct SAntiArtillerySort;
	friend class CAntiArtilleryManager;
};
#endif // __ANTI_ARTILLERY_H__
