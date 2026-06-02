#ifndef __RANGEALLOCS_H__
#define __RANGEALLOCS_H__
#pragma ONCE
enum EAllocVals
{
	EAV_SUCCESS,                          // successive allocation
	EAV_NOFREE,                           // not enough free space
	EAV_NOSIZE,                           // total size is not big enough
	EAV_BUSY                              // can't allocate range before another is not freed
};
enum EAllocReset
{
	EAR_HAVEDATA,                         // allocator still contain allocated ranges
	EAR_NODATA,                           // no ranges allocated. everything is freed
};
class CStaticAllocator
{
	int nSize;                            // total size
	int nCurrPos;                         // current allocation posision
public:
	CStaticAllocator( int _nSize )
		: nSize( _nSize ), nCurrPos( 0 ) {  }
	static int GetOptimalSize( int nDesiredSize, int nElementSize ) { return nDesiredSize; }
	EAllocVals Allocate( int nAmount, SRangeLimits *pRange )
	{
		if ( nAmount > nSize )
			return EAV_NOSIZE;
		else if ( nCurrPos + nAmount > nSize )
			return EAV_NOFREE;
		else
		{
			pRange->first = nCurrPos;
			pRange->second = nAmount;
			nCurrPos += nAmount;
			return EAV_SUCCESS;
		}
	}
	void Free( const SRangeLimits &range ) {  }
	EAllocReset FrameReset() const { return nCurrPos != 0 ? EAR_HAVEDATA : EAR_NODATA; }
	EAllocReset OverflowReset() const { return nCurrPos != 0 ? EAR_HAVEDATA : EAR_NODATA; }
	EAllocVals HasBlock( int nAmount ) const
	{
		if ( nAmount > nSize )
			return EAV_NOSIZE;
		else if ( nCurrPos + nAmount > nSize )
			return EAV_NOFREE;
		else
			return EAV_SUCCESS;
	}
};
class CTempAllocator
{
	int nSize;                            // total size
	int nCurrPos;                         // current allocation posision
	bool bRangeAllocated;
public:
	CTempAllocator( int _nSize )
		: nSize( _nSize ), nCurrPos( 0 ) {  }
	static int GetOptimalSize( int nDesiredSize, int nElementSize ) { return Max( nDesiredSize, 65536/nElementSize ); }
	EAllocVals Allocate( int nAmount, SRangeLimits *pRange )
	{
		if ( bRangeAllocated )
			return EAV_BUSY;
		if ( nAmount > nSize )
			return EAV_NOSIZE;
		else if ( nCurrPos + nAmount > nSize )
			return EAV_NOFREE;
		else
		{
			pRange->first = nCurrPos;
			pRange->second = nAmount;
			nCurrPos += nAmount;
			bRangeAllocated = true;
			return EAV_SUCCESS;
		}
	}
	void Free( const SRangeLimits &range ) { bRangeAllocated = false; }
	EAllocReset FrameReset() const { return bRangeAllocated ? EAR_HAVEDATA : EAR_NODATA; }
	EAllocReset OverflowReset()
	{
		if ( bRangeAllocated )
			return EAR_HAVEDATA;
		else
		{
			nCurrPos = 0;
			return EAR_NODATA;
		}
	}
	EAllocVals HasBlock( int nAmount ) const
	{
		if ( bRangeAllocated )
			return EAV_BUSY;
		if ( nAmount > nSize )
			return EAV_NOSIZE;
		else if ( nCurrPos + nAmount > nSize )
			return EAV_NOFREE;
		else
			return EAV_SUCCESS;
	}
};
class CPow2Allocator
{
	std::vector< std::list<SRangeLimits> > ranges;
	int GetTotalSize() const { return int( 1UL << ranges.size() ); }
	void SetSize( int nSize );
	bool Clear();
	bool GetRangeFromChunk( int nChunk, SRangeLimits *pRange );
	bool AllocateChunk( int nOptimalChunk, int nChunk, const SRangeLimits &range, SRangeLimits *pResult );
	void FreeLocal( int nStart, int nChunk );
public:
	CPow2Allocator( int nSize ) { SetSize( nSize ); }
	static int GetOptimalSize( int nDesiredSize, int nElementSize )
	{
		DWORD dwDesiredPow2 = GetNextPow2( nDesiredSize );
		DWORD dwOptimalPow2 = GetNextPow2( 65536/nElementSize );
		return int( Max( dwDesiredPow2, dwOptimalPow2 ) );
	}
	EAllocVals Allocate( int nAmount, SRangeLimits *pRange );
	void Free( const SRangeLimits &range );
	EAllocVals HasBlock( int nAmount ) const;
	int GetNumBlocks() const;
	int GetFree() const;
	void TestRanges() const;
};
#endif // __RANGEALLOCS_H__
