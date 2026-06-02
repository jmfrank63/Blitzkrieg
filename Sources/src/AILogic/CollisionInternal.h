#ifndef __COLLISION_INTERNAL_H__
#define __COLLISION_INTERNAL_H__

#pragma ONCE
#include "Collision.h"
class CPathUnit;
struct SUnitsPair
{
	DECLARE_SERIALIZE;

public:
	CPtr<CPathUnit> pUnit1;
	CPtr<CPathUnit> pUnit2;
	int nCollideType;

	SUnitsPair() { }
	SUnitsPair( class CPathUnit *_pUnit1, class CPathUnit *_pUnit2, const int _nCollideType ) 
		: pUnit1( _pUnit1 ), pUnit2( _pUnit2 ), nCollideType( _nCollideType ) { }

	friend bool operator < ( const SUnitsPair &pair1, const SUnitsPair &pair2 );
};
bool operator < ( const SUnitsPair &pair1, const SUnitsPair &pair2 );
class CCollisionsCollector : public IRefCount
{
	OBJECT_NORMAL_METHODS( CCollisionsCollector );
	DECLARE_SERIALIZE;
	
	std::priority_queue<SUnitsPair> collisions;
public:
	void AddCollision( class CPathUnit *pUnit1, class CPathUnit *pUnit2, const int nCollideType );
	void HandOutCollisions();
};
class CCollision : public ICollision
{
	DECLARE_SERIALIZE;
protected:	
	class CPathUnit *pUnit;
	CPtr<CAIUnit> pPushUnit;
	int nPriority;
public:	
	CCollision() : pUnit( 0 ) { }
	CCollision( class CPathUnit *pUnit, class CAIUnit *pPushUnit, const int nPriority );

	virtual bool IsSolved();
	
	virtual int FindCandidates();

	virtual int GetPriority() const { return nPriority; }

	class CAIUnit *GetPushUnit() const { return pPushUnit; }
};
class CFreeOfCollisions : public CCollision
{
	OBJECT_COMPLETE_METHODS( CFreeOfCollisions );
	DECLARE_SERIALIZE;
public:
	CFreeOfCollisions() { }
	explicit CFreeOfCollisions( class CPathUnit *pUnit, class CAIUnit *pPushUnit );

	virtual bool IsSolved();
	virtual interface IPath* GetPath() const { return 0; }

	virtual void Segment() { }

	virtual ECollisionName GetName() const { return ICollision::ECN_FREE; }
};
class CGivingPlaceCollision : public CCollision
{
	OBJECT_COMPLETE_METHODS( CGivingPlaceCollision );
	DECLARE_SERIALIZE;

	CVec2 vDir;
	CVec2 finishPoint;
	NTimer::STime timeToFinish;

	bool IsPathSolved();
public:
	CGivingPlaceCollision() { }
	CGivingPlaceCollision( class CPathUnit *pUnit, class CAIUnit *pPushUnit, const CVec2 &vDir, const float fDist, const int nPriority );

	virtual bool IsSolved();
	virtual interface IPath* GetPath() const;

	virtual void Segment();

	virtual ECollisionName GetName() const { return ICollision::ECN_GIVE_PLACE; }
};
class CGivingPlaceRotateCollision : public CCollision
{
	OBJECT_COMPLETE_METHODS( CGivingPlaceRotateCollision );
	DECLARE_SERIALIZE;

	WORD wDir;
	bool bTurned;
public:
	CGivingPlaceRotateCollision() { }
	CGivingPlaceRotateCollision( class CPathUnit *pUnit, class CAIUnit *pPushUnit, const CVec2 &vDir, const int nPriority );

	virtual bool IsSolved() { return bTurned || CCollision::IsSolved(); }
	virtual interface IPath* GetPath() const { return 0; }

	virtual void Segment();

	virtual ECollisionName GetName() const { return ICollision::ECN_GIVE_PLACE_ROTATE; }
};
class CWaitingCollision : public CCollision
{
	OBJECT_NORMAL_METHODS( CWaitingCollision );
	DECLARE_SERIALIZE;

	NTimer::STime finishTime;
	bool bLock;
public:
	CWaitingCollision() { }
	CWaitingCollision( class CPathUnit *pUnit, class CAIUnit *pPushUnit, bool bLock );
	virtual ~CWaitingCollision();

	virtual bool IsSolved();
	virtual interface IPath* GetPath() const { return 0; }

	virtual void Segment() { }

	virtual ECollisionName GetName() const { return ICollision::ECN_WAIT; }

	void Finish();
};
class CStopCollision : public CCollision
{
	OBJECT_NORMAL_METHODS( CStopCollision );
	DECLARE_SERIALIZE;

	NTimer::STime finishTime;
public:
	CStopCollision() { }
	CStopCollision( class CPathUnit *pUnit );

	virtual bool IsSolved();
	virtual interface IPath* GetPath() const { return 0; }

	virtual void Segment() { }
	virtual ECollisionName GetName() const { return ICollision::ECN_STOP; }
};
class CPlaneCollision : public ICollision
{
	OBJECT_COMPLETE_METHODS( CPlaneCollision );
	DECLARE_SERIALIZE;
public:	
	virtual int FindCandidates() { return 0; }

	virtual bool IsSolved() { return false; }
	virtual int GetPriority() const { return 0; }

	virtual interface IPath* GetPath() const { return 0; }
	virtual class CAIUnit* GetPushUnit() const { return 0; }

	virtual void Segment() { }

	virtual ECollisionName GetName() const { return ICollision::ECN_FREE; }
};
#endif // __COLLISION_INTERNAL_H__
