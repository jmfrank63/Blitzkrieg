#ifndef __CELLSCONGLOMERATECONTAINER_H__
#define __CELLSCONGLOMERATECONTAINER_H__
#pragma ONCE

struct SIntPair
{
	int x;
	int y;
	SIntPair() : x( 0 ), y( 0 ) {  }
	SIntPair( const int x,const int y) : x( x ), y( y ) {  }
	bool operator==(const SIntPair &p)const
	{
		return x==p.x &&y==p.y;
	}
	bool operator!=( const SIntPair &p ) const
	{
		return !operator==(p);
	}
};
typedef std::list<SIntPair/*hearable cell coordinate*/> CHearableCells;
typedef CArray2D<CHearableCells> CConglomerate;
typedef std::vector<CConglomerate> CConglomerates;

class CCellsConglomerateContainer
{
	DECLARE_SERIALIZE;
	struct SAddCellEnumerator
	{
		const SIntPair vCenter;
		CConglomerates &conglomerates;
		SAddCellEnumerator( const SIntPair &_vCenter, CConglomerates &_conglomerates ) : vCenter( _vCenter ), conglomerates( _conglomerates ) {  }
		void operator()( const int nCurRank, const int nX, const int nY ) 
		{ 
			conglomerates[nCurRank]( nX, nY ).push_back( vCenter ); 
		}
		int GetSizeX() const { return conglomerates[0].GetSizeX(); }
		int GetSizeY() const { return conglomerates[0].GetSizeY(); }
	};
	
	struct SRemoveCellEnumerator
	{
		const SIntPair vCenter;
		CConglomerates &conglomerates;
		SRemoveCellEnumerator( const SIntPair &_vCenter, CConglomerates &_conglomerates ) : vCenter( _vCenter ), conglomerates( _conglomerates ) {  }
		void operator()( const int nCurRank, const int nX, const int nY ) 
		{ 
			conglomerates[nCurRank]( nX, nY ).remove( vCenter ); 
		}
		int GetSizeX() const { return conglomerates[0].GetSizeX(); }
		int GetSizeY() const { return conglomerates[0].GetSizeY(); }
	};

	static const int MAX_RANK;					// maximum allowed rank
	int nMaxRank;												// current rank
	CConglomerates conglomerates;
	bool bInitted;
public:
	CCellsConglomerateContainer() : bInitted( false ) {  }
	void Init( const int nMaxX, const int nMaxY );
	
	void AddHearCell( const SIntPair &vSourceCell, const int nRadius );
	void RemoveHearCell( const SIntPair &vSourceCell, const int nRadius );
	bool IsInitted() const { return bInitted; }

	void Clear();

	template <class TEnumFunc> 
		void EnumHearableCells( TEnumFunc func, const SIntPair &center )
	{
		for ( int nRank = 0; nRank <= nMaxRank; ++nRank )
		{
			const int nConglomerateSize = 1<<nRank;

			const int nX = center.x / nConglomerateSize;
			const int nY = center.y / nConglomerateSize;
			const CHearableCells & cells = conglomerates[nRank]( nX, nY );
			for ( CHearableCells::const_iterator it = cells.begin(); it != cells.end(); ++it )
				func( abs(center.x - it->x ) + abs( center.y-it->y ), *it );
		}
	}

};
#endif // __CELLSCONGLOMERATECONTAINER_H__
