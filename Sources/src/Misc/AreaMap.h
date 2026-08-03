#ifndef __AREA_MAP_H__
#define __AREA_MAP_H__
#pragma ONCE
template <class TYPE, class TStorageType = CPtr<TYPE>, class TPosition = CVec3, class TCoeffType = float>
class CAreaMap : public CArray2D< std::list<TStorageType> >
{
public:
	typedef std::list<TStorageType> CDataList;
	typedef CArray2D<CDataList> CBaseArea;
private:
	const TCoeffType tCellSize;								// area map cell size
	class CObjEqualFunctional
	{
		TYPE *pObj;
	public:
		explicit CObjEqualFunctional( TYPE *_pObj ) : pObj( _pObj ) {  }
		bool operator()( const TStorageType &ptr ) const { return ptr == pObj; }
	};
	void AddTo( int nX, int nY, TYPE *pObj )
	{
		CDataList &data = (*this)[nY][nX];
		typename CDataList::const_iterator pos = std::find_if( data.begin(), data.end(), CObjEqualFunctional(pObj) );
		if ( pos == data.end() )
			data.push_back( pObj );
	}
	void RemoveFrom( int nX, int nY, TYPE *pObj )
	{
		if ( (nX < 0) || (nX >= this->GetSizeX()) || (nY < 0) || (nY >= this->GetSizeY()) )
			return;
		CDataList &data = (*this)[nY][nX];
		typename CDataList::iterator pos = std::find_if( data.begin(), data.end(), CObjEqualFunctional(pObj) );
		if ( pos != data.end() )
			data.erase( pos );
	}
	CAreaMap( const CAreaMap &a ) {  }
	CAreaMap& operator=( const CAreaMap &a ) { return *this; }
public:
	explicit CAreaMap( TCoeffType _tCellSize ) : tCellSize( _tCellSize ) {  }
	void MoveTo( TYPE *pObj, const TPosition &vNewPos )
	{
		const TPosition vOldPos = pObj->GetPosition();
		const int nOldIndexX = int( vOldPos.x / tCellSize );
		const int nOldIndexY = int( vOldPos.y / tCellSize );
		const int nNewIndexX = int( vNewPos.x / tCellSize );
		const int nNewIndexY = int( vNewPos.y / tCellSize );
		if ( (nOldIndexX != nNewIndexX) || (nOldIndexY != nNewIndexY) )
		{
			AddTo( nNewIndexX, nNewIndexY, pObj );
			RemoveFrom( nOldIndexX, nOldIndexY, pObj );
		}
		pObj->SetPosition( vNewPos );
	}
	void Add( TYPE *pObj )
	{
		const TPosition vPos = pObj->GetPosition();
		const int nX = int( vPos.x / tCellSize );
		const int nY = int( vPos.y / tCellSize );
		AddTo( nX, nY, pObj );
	}
	void Remove( TYPE *pObj )
	{
		const TPosition vPos = pObj->GetPosition();
		const int nX = int( vPos.x / tCellSize );
		const int nY = int( vPos.y / tCellSize );
		RemoveFrom( nX, nY, pObj );
	}

	void AddToPosition( TYPE *pObj, const TPosition &vPos )
	{
		const int nX = int( vPos.x / tCellSize );
		const int nY = int( vPos.y / tCellSize );
		AddTo( nX, nY, pObj );
	}

	void RemoveFromPosition( TYPE *pObj, const TPosition &vPos )
	{
		const int nX = int( vPos.x / tCellSize );
		const int nY = int( vPos.y / tCellSize );
		RemoveFrom( nX, nY, pObj );
	}
	const bool IsInArea( const TPosition &vPos ) const
	{
		return ( vPos.x >= 0 ) && (vPos.y >= 0) && (vPos.x < GetSizeX()*tCellSize) && (vPos.y < GetSizeY()*tCellSize);
	}
};
template <class TYPE>
class CStructAreaMapComparator
{
	const TYPE &obj;
public:
	CStructAreaMapComparator( const TYPE &_obj ) : obj( _obj ) {  }
	const bool operator()( const TYPE &obj2 ) const { return obj == obj2; }
};
template <class TYPE, class TPosition = CVec3, class TComparator = CStructAreaMapComparator<TYPE>, class TCoeffType = float>
class CStructAreaMap : public CArray2D< std::list<TYPE> >
{
public:
	typedef std::list<TYPE> CDataList;
	typedef CArray2D<CDataList> CBaseArea;
private:
	const TCoeffType tCellSize;								// area map cell size
	void AddTo( int nX, int nY, const TYPE &obj )
	{
		CDataList &data = (*this)[nY][nX];
		typename CDataList::const_iterator pos = std::find_if( data.begin(), data.end(), TComparator(obj) );
		if ( pos == data.end() )
			data.push_back( obj );
	}
	void RemoveFrom( int nX, int nY, const TYPE &obj )
	{
		if ( (nX < 0) || (nX >= this->GetSizeX()) || (nY < 0) || (nY >= this->GetSizeY()) )
			return;
		CDataList &data = (*this)[nY][nX];
		typename CDataList::iterator pos = std::find_if( data.begin(), data.end(), TComparator(obj) );
		if ( pos != data.end() )
			data.erase( pos );
	}
	CStructAreaMap( const CStructAreaMap &a ) {  }
	CStructAreaMap& operator=( const CStructAreaMap &a ) { return *this; }
public:
	explicit CStructAreaMap( TCoeffType _tCellSize ) : tCellSize( _tCellSize ) {  }
	void MoveTo( TYPE &obj, const TPosition &vNewPos )
	{
		const TPosition vOldPos = obj.GetPosition();
		const int nOldIndexX = int( vOldPos.x / tCellSize );
		const int nOldIndexY = int( vOldPos.y / tCellSize );
		const int nNewIndexX = int( vNewPos.x / tCellSize );
		const int nNewIndexY = int( vNewPos.y / tCellSize );
		if ( (nOldIndexX != nNewIndexX) || (nOldIndexY != nNewIndexY) )
		{
			AddTo( nNewIndexX, nNewIndexY, obj );
			RemoveFrom( nOldIndexX, nOldIndexY, obj );
		}
		obj.SetPosition( vNewPos );
	}
	void Add( const TYPE &obj )
	{
		const TPosition vPos = obj.GetPosition();
		const int nX = int( vPos.x / tCellSize );
		const int nY = int( vPos.y / tCellSize );
		AddTo( nX, nY, obj );
	}
	void Remove( const TYPE &obj )
	{
		const TPosition vPos = obj.GetPosition();
		const int nX = int( vPos.x / tCellSize );
		const int nY = int( vPos.y / tCellSize );
		RemoveFrom( nX, nY, obj );
	}

	void AddToPosition( const TYPE &obj, const TPosition &vPos )
	{
		const int nX = int( vPos.x / tCellSize );
		const int nY = int( vPos.y / tCellSize );
		AddTo( nX, nY, obj );
	}

	void RemoveFromPosition( const TYPE &obj, const TPosition &vPos )
	{
		const int nX = int( vPos.x / tCellSize );
		const int nY = int( vPos.y / tCellSize );
		RemoveFrom( nX, nY, obj );
	}
	const bool IsInArea( const TPosition &vPos ) const
	{
		return ( vPos.x >= 0 ) && (vPos.y >= 0) && (vPos.x < GetSizeX()*tCellSize) && (vPos.y < GetSizeY()*tCellSize);
	}
};
#endif // __AREA_MAP_H__
