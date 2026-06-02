#ifndef __FIXEDOBJLIST_H__
#define __FIXEDOBJLIST_H__
template < int TNumObjects, class TObj, class THolder = CPtr<TObj> >
class CFixedObjList
{
	THolder objects[TNumObjects];
public:
	CFixedObjList() {  }
	CFixedObjList( const CFixedObjList &lst ) { *this = lst; }
	CFixedObjList& operator=( const CFixedObjList &lst )
	{
		for ( int i = 0; i < TNumObjects; ++i )
			objects[i] = lst.objects[i];
		return *this;
	}
	void Clear()
	{
		for ( int i = 0; i < TNumObjects; ++i )
			objects[i] = 0;
	}
	static int GetSize() { return TNumObjects; }
	bool IsEmpty() const { return objects[0] == 0; }
	TObj* Get( const int nIndex ) const
	{
		NI_ASSERT_SLOW_T( (nIndex >= 0) && (nIndex < TNumObjects), NStr::Format("Can't access at index %d - valid range [0..%d]", nIndex, TNumObjects) );
		return objects[nIndex];
	}
	TObj* operator[]( const int nIndex ) const
	{
		return Get( nIndex );
	}
	void Add( TObj *pObj )
	{
		for ( int i = TNumObjects - 2; i >= 0; --i )
			objects[i + 1] = objects[i];
		objects[0] = pObj;
	}
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		char cNumObjects = TNumObjects;
		if ( saver.IsReading() ) 
		{
			Clear();
			saver.Add( 1, &cNumObjects );
			cNumObjects = Min( char(TNumObjects), cNumObjects );
		}
		else
		{
			for ( int i = 0; i < TNumObjects; ++i )
			{
				if ( objects[i] == 0 ) 
				{
					cNumObjects = char( i );
					break;
				}
			}
			if ( cNumObjects > 0 ) 
				saver.Add( 1, &cNumObjects );
		}
		for ( char i = 0; i < cNumObjects; ++i )
			saver.Add( 2 + i, &( objects[i] ) );
		return 0;
	}
};
template < int nNumObjects, class TObj, class THolder = CPtr<TObj>, class TCoeffType = float, int nBlockSize = 8 >
class CFixedObjAreaMap
{
public:
	typedef CFixedObjList<nNumObjects, TObj, THolder> CFixedList;
	typedef CArray2D<CFixedList> CBase;
	class CIterator
	{
		CFixedList *pLineCurr;							// current iteration position
		CFixedList *pLineEnd;						  	// line end iteration position
		const int nNextLineAdd;							// value to add to 'pLineEnd' to receive next line start
		int nNumLines;											// number of lines to iterate
		int nElement;												// current iteration element
		void NextStepElement()
		{
			++nElement;
			if ( (nElement >= CFixedList::GetSize()) || (pLineCurr->Get(nElement) == 0) ) 
			{
				nElement = 0;
				NextStepLine();
			}
		}
		void NextStepLine()
		{
			++pLineCurr;
			if ( pLineCurr >= pLineEnd ) 
			{
				--nNumLines;
				pLineCurr = pLineEnd + nNextLineAdd;
				pLineEnd = pLineEnd + nNextLineAdd + nBlockSize;
			}
		}
	public:
		CIterator( const int nX, const int nY, CBase &array )
			: nNextLineAdd( array.GetSizeX() - nBlockSize )
		{
			pLineCurr = &( array[nY][nX] );
			pLineEnd = pLineCurr + nBlockSize;
			nNumLines = nBlockSize;
			nElement = 0;
			while ( !IsEnd() && pLineCurr->IsEmpty() ) 
				NextStepLine();
		}
		CIterator( const CIterator &it )
			: nNextLineAdd( it.nNextLineAdd )
		{
			pLineCurr = it.pLineCurr;
			pLineEnd = it.pLineEnd;
			nNumLines = it.nNumLines;
			nElement = it.nElement;
		}
		void Next()
		{
			NextStepElement();
			while ( !IsEnd() && pLineCurr->IsEmpty() ) 
				NextStepLine();
		}
		bool IsEnd() const { return nNumLines <= 0; }
		operator TObj*() const { return pLineCurr->Get( nElement ); }
		TObj* operator->() const { return pLineCurr->Get( nElement ); }
	};
	typedef CIterator iterator;
private:
	CBase array;
	const TCoeffType tCellSize;
public:
	explicit CFixedObjAreaMap( const TCoeffType &_tCellSize ) : tCellSize( _tCellSize ) {  }
	void SetSizes( const int nSizeX, const int nSizeY )
  {
		array.SetSizes( nSizeX, nSizeY );
	}
	int GetSizeX() const { return array.GetSizeX(); }
	int GetSizeY() const { return array.GetSizeY(); }
	void Clear()
	{
		array.Clear();
	}
	bool Add( TObj *pObj )
	{
		const CVec3 vPos = pObj->GetPosition();
		const int nCellX = vPos.x / tCellSize;
		const int nCellY = vPos.y / tCellSize;
		if ( (nCellX < 0) || (nCellY < 0) || (nCellX >= array.GetSizeX()) || (nCellY >= array.GetSizeY()) )
			return false;
		array[nCellY][nCellX].Add( pObj );
		return true;
	}
	iterator Iterate( const int nPatchX, const int nPatchY )
	{
		return iterator( nPatchX * nBlockSize, nPatchY * nBlockSize, array );
	}
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		if ( saver.IsReading() ) 
		{
			int nSizeX, nSizeY;
			saver.Add( 1, &nSizeX );
			saver.Add( 2, &nSizeY );
			array.Clear();
			array.SetSizes( nSizeX, nSizeY );
			int nCounter = 1;
			saver->SetChunkCounter( nCounter++ );
			int nNextCell = -1;
			saver.Add( 3, &nNextCell );
			if ( nNextCell >= 0 ) 
			{
				CFixedList *pData = array.GetBuffer();
				for ( int i = 0; i < nSizeX*nSizeY; ++i, ++pData )
				{
					if ( i == nNextCell ) 
					{
						saver.Add( 4, pData );
						saver->SetChunkCounter( nCounter++ );
						saver.Add( 3, &nNextCell );
						if ( nNextCell == -1 ) 
							break;
					}
				}
			}
		}
		else
		{
			int nSizeX = array.GetSizeX(), nSizeY = array.GetSizeY();
			saver.Add( 1, &nSizeX );
			saver.Add( 2, &nSizeX );
			CFixedList *pData = array.GetBuffer();
			int nCounter = 1;
			for ( int i = 0; i < nSizeX*nSizeY; ++i, ++pData )
			{
				if ( !pData->IsEmpty() ) 
				{
					saver->SetChunkCounter( nCounter++ );
					saver.Add( 3, &i );
					saver.Add( 4, pData );
				}
			}
			int nCellTerminator = -1;
			saver->SetChunkCounter( nCounter );
			saver.Add( 3, &nCellTerminator );
		}
		return 0;
	}
};
#endif // __FIXEDOBJLIST_H__