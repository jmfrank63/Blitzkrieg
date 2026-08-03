#include "StdAfx.h"

#include "VectorObject.h"

#include "../Misc/Checker.h"
void STVOLayer::SelectPatches( const std::vector<DWORD> &sels, const int nNumBasePoints )
{
	std::vector<const SPatch*> selPatches;
	selPatches.reserve( patches.size() );
	int nNumPoints = 0;
	for ( std::vector<SPatch>::const_iterator it = patches.begin(); it != patches.end(); ++it )
	{
		if ( std::binary_search(sels.begin(), sels.end(), it->dwPatch) ) 
		{
			selPatches.push_back( &(*it) );
			nNumPoints += it->points.size();
		}
	}
	if ( nNumPoints == 0 ) 
	{
		vertices.clear();
		indices.clear();
		return;
	}
	std::vector<int> points1;
	points1.reserve( nNumPoints + 20 );
	for ( std::vector<const SPatch*>::const_iterator it = selPatches.begin(); it != selPatches.end(); ++it )
		points1.insert( points1.end(), (*it)->points.begin(), (*it)->points.end() );
	std::sort( points1.begin(), points1.end() );
	points1.erase( std::unique( points1.begin(), points1.end() ), points1.end() );

	std::vector<int> points2;
	points2.reserve( points1.size() + 20 );
	{
		if ( points1.front() > 0 ) 
			points2.push_back( points1.front() - 1 );
		std::vector<int>::const_iterator point = points1.begin();
		int nLastPoint = *point;
		++point;
		for ( ; point != points1.end(); ++point )
		{
			if ( nLastPoint + 1 < *point ) 
			{
				points2.push_back( nLastPoint + 1 );
				if ( nLastPoint + 1 != *point - 1 ) 
					points2.push_back( *point - 1 );
			}
			else
				points2.push_back( nLastPoint );
			nLastPoint = *point;
		}
		points2.push_back( nLastPoint );
		if ( points1.back() < nNumBasePoints - 1 ) 
			points2.push_back( points1.back() + 1 );
	}
	const int N = nNumVertsPerLine;
	vertices.clear();
	vertices.reserve( points2.size() * N );
	for ( std::vector<int>::const_iterator it = points2.begin(); it != points2.end(); ++it )
	{
		CheckRange( allvertices, (*it) * N + N - 1 );
		vertices.insert( vertices.end(), 
				             allvertices.begin() + ((*it) * N), 
										 allvertices.begin() + ((*it) * N + N) );			
	}
	indices.clear();
	indices.reserve( vertices.size() * 1.25f * 6 );
	for ( int i = 1; i < points2.size(); ++i )
	{
		if ( points2[i - 1] + 1 == points2[i] )
		{
			for ( int j = 0; j < N - 1; ++j )
			{
				const int nBase = (i - 1) * N + j;
				CheckRange( vertices, nBase + N + 1 );

				indices.push_back( nBase );
				indices.push_back( nBase + 1 );
				indices.push_back( nBase + N );

				indices.push_back( nBase + N + 1 );
				indices.push_back( nBase + N );
				indices.push_back( nBase + 1 );
			}
		}
	}
	std::vector<WORD>( indices ).swap( indices );
}
int STVOLayer::SPatch::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &dwPatch );
	saver.Add( 2, &points );
	return 0;
}
int STVOLayer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &textures );
	saver.Add( 2, &patches );
	saver.Add( 3, &allvertices );
	saver.Add( 4, &nNumVertsPerLine );
	return 0;
}
void CTerrainVectorObject::Init( const SVectorStripeObject &_desc )
{
	desc = _desc;
	int nStart = 0;
	for ( int i = 1; i < desc.points.size(); ++i )
	{
		if ( desc.points[i].bKeyPoint )
		{
			const int nEnd = i;
			const float fOpStart = desc.points[nStart].fOpacity;
			const float fOpEnd = desc.points[nEnd].fOpacity;
			for ( int j = nStart + 1; j < nEnd; ++j )
				desc.points[j].fOpacity = fOpStart + float( j - nStart ) / float( nEnd - nStart ) * ( fOpEnd - fOpStart );
			nStart = nEnd;
		}
	}
}
