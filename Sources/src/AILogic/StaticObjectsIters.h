#ifndef __STATIC_OBJECTS_ITERS__
#define __STATIC_OBJECTS_ITERS__
#pragma ONCE
#include "StaticObjects.h"
#include "StaticObject.h"
extern CStaticObjects theStatObjs;
template<bool bOnlyContainers>
class CStObjIter
{
	int y, x;
	int downX, downY, upX, upY;

	CStaticObjects::StaticObjectsAreaMap::CDataList::iterator iter;

	void NextXY();
	void IterateToNextCell();

	CStObjIter( const CStObjIter &iter );
	CStObjIter& operator=( const CStObjIter &iter );
protected:
	CStaticObjects::StaticObjectsAreaMap& GetAreaMap();
	int GetCellSize() const;
public:
	CStObjIter() : x( 0 ), y( 0 ), downX( 0 ), downY( 0 ), upX( 0 ), upY( 0 ) { }
	CStObjIter( const int downX, const int upX, const int downY, const int upY );
	~CStObjIter() { theStatObjs.SetIterCreated( false ); }

	void Iterate();
	bool IsFinished() const;
	void Reset() 
	{
		theStatObjs.SetIterCreated( false );
		Init( downX, upX, downY, upY );
	}
	void Init( const int _downX, const int _upX, const int _downY, const int _upY );

	class CExistingObject* operator*();
};
template<bool bOnlyContainers>
class CStObjGlobalIter : public CStObjIter<bOnlyContainers>
{
	CStObjGlobalIter( const CStObjGlobalIter &iter );
	CStObjGlobalIter& operator=( const CStObjGlobalIter &iter );
public:
	CStObjGlobalIter();
};
template<bool bOnlyContainers>
class CStObjCircleIter : public CStObjIter<bOnlyContainers>
{
	CStObjCircleIter( const CStObjCircleIter &iter );
	CStObjCircleIter& operator=( const CStObjCircleIter &iter );
public:
	CStObjCircleIter( const CVec2 &vCenter, const float fR );
};
class CMinesIter : protected CStObjCircleIter<false>
{
	int nParty;
	const bool bAllMines;
	
	void IterateToNextMine();
public:
	CMinesIter( const CVec2 &vCenter, float fR, const int nParty, const bool bAllMines = false );
	void Iterate();
	bool IsFinished() const;
	void Reset() ;
	class CMineStaticObject* operator->();
	class CMineStaticObject* operator*();
};
template<bool bOnlyContainers>
inline CStaticObjects::StaticObjectsAreaMap& CStObjIter<bOnlyContainers>::GetAreaMap()
{
	return theStatObjs.GetAreaMap();
}
template<>
inline CStaticObjects::StaticObjectsAreaMap& CStObjIter<true>::GetAreaMap()
{
	return theStatObjs.GetContainersAreaMap();
}
template<bool bOnlyContainers>
inline int CStObjIter<bOnlyContainers>::GetCellSize() const
{
	return SConsts::STATIC_OBJ_CELL;
}
template<>
inline int CStObjIter<true>::GetCellSize() const
{
	return SConsts::STATIC_CONTAINER_OBJ_CELL;
}
template<bool bOnlyContainers>
CStObjIter<bOnlyContainers>::CStObjIter( const int downX, const int upX, const int downY, const int upY ) 
{ 
	Init( downX, upX, downY, upY );
}
template<bool bOnlyContainers>
void CStObjIter<bOnlyContainers>::Init( const int _downX, const int _upX, const int _downY, const int _upY )
{
	downX = Clamp( _downX, 0, this->GetAreaMap().GetSizeX() - 1 );
	upX = Clamp( _upX, 0, this->GetAreaMap().GetSizeX() - 1 );
  downY = Clamp( _downY, 0, this->GetAreaMap().GetSizeY() - 1 );
	upY = Clamp( _upY, 0, this->GetAreaMap().GetSizeY() - 1 );

#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	NI_ASSERT_T( !theStatObjs.IsIterCreated(), "Can't create two iterators concurrently" ); 
	theStatObjs.SetIterCreated( true );
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)

	CExistingObject::UpdateGlobalMark();

	y = downY; x = downX;
	if ( this->GetAreaMap().GetSizeX() > 0 && this->GetAreaMap().GetSizeY() > 0 && !this->GetAreaMap()[y][x].empty() )
		iter = this->GetAreaMap()[y][x].begin();
	else
		IterateToNextCell();

	if ( !IsFinished() )
		(*iter)->SetGlobalUpdated();
}
template<bool bOnlyContainers>
void CStObjIter<bOnlyContainers>::IterateToNextCell()
{
	do
	{
		NextXY();
	} while ( !IsFinished() && this->GetAreaMap()[y][x].empty() );

	if ( !IsFinished() )
		iter = this->GetAreaMap()[y][x].begin();
}
template<bool bOnlyContainers>
void CStObjIter<bOnlyContainers>::NextXY()
{
	if ( ++x > upX )
	{
		++y;
		x = downX;
	}
}
template<bool bOnlyContainers>
bool CStObjIter<bOnlyContainers>::IsFinished() const
{
	return y > upY;
}
template<bool bOnlyContainers>
void CStObjIter<bOnlyContainers>::Iterate()
{ 
	do
	{
		if ( ++iter == this->GetAreaMap()[y][x].end() )
			IterateToNextCell();
	} while ( !IsFinished() && (*iter)->IsGlobalUpdated() );

	if ( !IsFinished() )
		(*iter)->SetGlobalUpdated();
}
template<bool bOnlyContainers>
CExistingObject* CStObjIter<bOnlyContainers>::operator*() 
{ 
	NI_ASSERT_T( !IsFinished(), "Can't perform operator *" ); 
	return *iter;
}
template<bool bOnlyContainers>
CStObjGlobalIter<bOnlyContainers>::CStObjGlobalIter()
: CStObjIter<bOnlyContainers>( 0, this->GetAreaMap().GetSizeX() - 1, 0, this->GetAreaMap().GetSizeY() - 1 )
{
}
template<bool bOnlyContainers>
CStObjCircleIter<bOnlyContainers>::CStObjCircleIter( const CVec2 &vCenter, const float fR )
{
	const int nBigCellSize = SConsts::TILE_SIZE * this->GetCellSize();

	const int nMinX = Max( 0, int( (vCenter.x - fR) / nBigCellSize ) );
	const int nMaxX = Min( this->GetAreaMap().GetSizeX() - 1, int( (vCenter.x + fR) / nBigCellSize ) );
	const int nMinY = Max( 0, int( ( vCenter.y - fR ) / nBigCellSize ) );
	const int nMaxY = Min( this->GetAreaMap().GetSizeY() - 1, int( ( vCenter.y + fR ) / nBigCellSize ) );

	CStObjIter<bOnlyContainers>::Init( nMinX, nMaxX, nMinY, nMaxY );
}
#endif // __STATIC_OBJECTS_ITERS__
