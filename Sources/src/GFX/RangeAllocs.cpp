#include "StdAfx.h"

#include "RangeAllocs.h"
/*
  1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987, 1597,
  2584, 4181, 6765, 10946, 17711, 28657, 46368, 75025, 121393,
  196418, 317811, 514229, 832040, 1346269, 2178309, 3524578,
  5702887, 9227465, 14930352, 24157817, 39088169, 63245986,
  102334155, 165580141, 267914296, 433494437, 701408733,
  1134903170, 1836311903, 2971215073, 4807526976, 7778742049,
  12586269025, 20365011074, 32951280099, 53316291173,
  86267571272, 139583862445, 225851433717, 365435296162,
  591286729879, 956722026041, 1548008755920, 2504730781961,
  4052739537881, 6557470319842, 10610209857723

*/
inline DWORD GetPow2( int nSize )
{
	return GetMSB( GetNextPow2( nSize ) );
}
inline int GetChunkSize( int nChunk )
{
	return 1UL << nChunk;
}
inline int GetOptimalChunk( int nSize )
{
	return GetMSB( GetNextPow2( nSize ) );
}
inline int GetOptimalChunkSize( int nSize )
{
	return GetChunkSize( GetOptimalChunk( nSize ) );
}
inline void SplitChunk( int nChunk, int nStart, SRangeLimits *pR1, SRangeLimits *pR2 )
{
	pR1->first = nStart;
	pR1->second = nChunk - 1;
	pR2->first = pR1->first + GetChunkSize( pR1->second );
	pR2->second = nChunk - 1;
}
void CPow2Allocator::SetSize( int nAmount )
{
	for ( std::vector< std::list<SRangeLimits> >::iterator pos = ranges.begin(); pos != ranges.end(); ++pos )
		pos->clear();
	DWORD dwPow2 = GetMSB( nAmount );
	ranges.resize( dwPow2 + 1 );
	ranges[dwPow2].push_back( SRangeLimits(0, nAmount) );
}
bool CPow2Allocator::GetRangeFromChunk( int nChunk, SRangeLimits *pRange )
{
	if ( ranges[nChunk].empty() )
		return false;
	*pRange = ranges[nChunk].front();
	ranges[nChunk].pop_front();
	return true;
}
bool CPow2Allocator::AllocateChunk( int nOptimalChunk, int nChunk, const SRangeLimits &range, SRangeLimits *pResult )
{
	if ( nChunk == nOptimalChunk )
	{
		*pResult = range;
		return true;
	}
	else
	{
		SRangeLimits r1, r2;
		SplitChunk( nChunk, range.first, &r1, &r2 );
		if ( r1.second >= nOptimalChunk )
		{
			FreeLocal( r2.first, r2.second );
			return AllocateChunk( nOptimalChunk, r1.second, r1, pResult );
		}
		else if ( r2.second >= nOptimalChunk )
		{
			FreeLocal( r1.first, r1.second );
			return AllocateChunk( nOptimalChunk, r2.second, r2, pResult );
		}
		return false;
	}
}
void CPow2Allocator::FreeLocal( int nStart, int nChunk )
{
	std::list<SRangeLimits> &subranges = ranges[nChunk];
	if ( subranges.empty() )	// nothing to collapse with - just add
		subranges.push_back( SRangeLimits(nStart, nChunk) );
	else
	{
		const int nChunkSize = GetChunkSize( nChunk );
		for ( std::list<SRangeLimits>::iterator it = subranges.begin(); it != subranges.end(); ++it )
		{
			if ( it->first + nChunkSize == nStart )				// can we collapse our block with 'it' as previous ?
			{
				nStart = it->first;
				subranges.erase( it );
				FreeLocal( nStart, nChunk + 1 );
				return;
			}
			else if ( nStart + nChunkSize == it->first )	// can we collapse our block with 'it' as next ? 
			{
				subranges.erase( it );
				FreeLocal( nStart, nChunk + 1 );
				return;
			}
		}
		if ( nChunk - 1 >= 0 )
		{
			const int nStart1 = nStart - GetChunkSize( nChunk - 1 );
			const int nStart2 = nStart + GetChunkSize( nChunk );
			std::list<SRangeLimits> &subranges2 = ranges[nChunk - 1];
			std::list<SRangeLimits>::iterator its[2] = { subranges2.end(), subranges2.end() };
			for ( std::list<SRangeLimits>::iterator it = subranges2.begin(); it != subranges2.end(); ++it )
			{
				if ( it->first == nStart1 )
					its[0] = it;
				else if ( it->first == nStart2 )
					its[1] = it;
			}
			if ( (its[0] != subranges2.end()) && (its[1] != subranges2.end()) )
			{
				subranges2.erase( its[0] );
				subranges2.erase( its[1] );
				FreeLocal( nStart1, nChunk + 1 );
				return;
			}
		}
		if ( nChunk < ranges.size() - 1 )
		{
			const int nStart1 = nStart + GetChunkSize( nChunk ) + GetChunkSize( nChunk + 1 );
			for ( std::list<SRangeLimits>::iterator it = subranges.begin(); it != subranges.end(); ++it )
			{
				if ( it->first == nStart1 )
				{
					const int nHiRangeStart = nStart + GetChunkSize( nChunk );
					std::list<SRangeLimits> &subranges2 = ranges[nChunk + 1];
					for ( std::list<SRangeLimits>::iterator range = subranges2.begin(); range != subranges2.end(); ++range )
					{
						if ( range->first == nHiRangeStart )
						{
							subranges.erase( it );
							subranges2.erase( range );
							FreeLocal( nStart, nChunk + 1 );
							return;
						}
					}
					break;
				}
			}
			const int nStart2 = nStart - GetChunkSize( nChunk ) - GetChunkSize( nChunk + 1 );
			for ( std::list<SRangeLimits>::iterator it = subranges.begin(); it != subranges.end(); ++it )
			{
				if ( it->first == nStart2 )
				{
					const int nHiRangeStart = nStart - GetChunkSize( nChunk + 1 );
					std::list<SRangeLimits> &subranges2 = ranges[nChunk + 1];
					for ( std::list<SRangeLimits>::iterator range = subranges2.begin(); range != subranges2.end(); ++range )
					{
						if ( range->first == nHiRangeStart )
						{
							subranges.erase( it );
							subranges2.erase( range );
							FreeLocal( nStart2, nChunk + 1 );
							return;
						}
					}
					break;
				}
			}

		}
		subranges.push_back( SRangeLimits(nStart, nChunk) );
	}
}
EAllocVals CPow2Allocator::Allocate( int nAmount, SRangeLimits *pRange )
{
	if ( nAmount > GetTotalSize() )
		return EAV_NOSIZE;
	int nOptimalChunk = GetOptimalChunk( nAmount );
	int nTopChunk = nOptimalChunk;
	for ( ; nTopChunk != ranges.size(); ++nTopChunk )
	{
		if ( ranges[nTopChunk].empty() == false )
		{
			SRangeLimits range;
			if ( GetRangeFromChunk( nTopChunk, &range ) == false )
				continue;
			if ( AllocateChunk( nOptimalChunk, nTopChunk, range, pRange ) == true )
			{
				pRange->second = nAmount;
				return EAV_SUCCESS;
			}
			else
				return EAV_NOFREE;
		}
	}
	return EAV_NOFREE;
}
void CPow2Allocator::Free( const SRangeLimits &range )
{
	int nChunk = GetOptimalChunk( range.second );
	FreeLocal( range.first, nChunk );
}
EAllocVals CPow2Allocator::HasBlock( int nAmount ) const
{
	if ( nAmount > GetTotalSize() )
		return EAV_NOSIZE;
	DWORD dwPow2 = GetPow2( nAmount );
	std::vector< std::list<SRangeLimits> >::const_iterator pos = ranges.begin();
	std::advance( pos, dwPow2 );
	for ( ; pos != ranges.end(); ++pos )
	{
		if ( !pos->empty() )
			return EAV_SUCCESS;
	}
	return EAV_NOFREE;
}
int CPow2Allocator::GetNumBlocks() const
{
	int nNumBlocks = 0;
	for ( std::vector< std::list<SRangeLimits> >::const_iterator it = ranges.begin(); it != ranges.end(); ++it )
		nNumBlocks += it->size();
	return nNumBlocks;
}
int CPow2Allocator::GetFree() const
{
	int nAllocated = 0;
	int i = 0;
	for ( std::vector< std::list<SRangeLimits> >::const_iterator it = ranges.begin(); it != ranges.end(); ++it, ++i )
		nAllocated += it->size() * GetChunkSize( i );
	return nAllocated;
}
void CPow2Allocator::TestRanges() const
{
	int i = 0;
	for ( std::vector< std::list<SRangeLimits> >::const_iterator it = ranges.begin(); it != ranges.end(); ++it, ++i )
	{
		if ( !it->empty() )
		{
			NStr::DebugTrace( "Range %d has a %d elements:\n", i, it->size() );
			for ( std::list<SRangeLimits>::const_iterator range = it->begin(); range != it->end(); ++range )
			{
				NStr::DebugTrace( "\t%d : %d : %d\n", range->first, GetChunkSize(range->second), range->first + GetChunkSize(range->second) );
			}
		}
	}
	NStr::DebugTrace( "\n\n" );
}
