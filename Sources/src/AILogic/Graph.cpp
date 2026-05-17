#include "stdafx.h"

#include "Graph.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*															CGraph															*
//*******************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraph::FindUpperComponent( const int v )
{
	if ( graphComponent[v] != -1 )
	{
		int nNextNode = graphComponent[v];

		while ( nNextNode != graphComponent[nNextNode] )
		{
			graphComponent[v] = graphComponent[nNextNode];
			nNextNode = graphComponent[nNextNode];
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraph::AddEdge( const int v1, const int v2 )
{
	if ( v1 != v2 )
	{
		if ( Max( v1, v2 ) >= n )
		{
			n = Max( v1, v2 ) + 1;
			const int nNewSize = n * 1.5;
			nodes.resize( nNewSize );
			dst.resize( nNewSize );
			pred.resize( nNewSize );
			graphComponent.resize( nNewSize, -1 );
		}

		nodes[v1].push_back( v2 );
		nodes[v2].push_back( v1 );

		FindUpperComponent( v1 );
		FindUpperComponent( v2 );

		if ( graphComponent[v1] == -1 )
			graphComponent[v1] = v1;
		if ( graphComponent[v2] == - 1 )
			graphComponent[v2] = v2;

		NI_ASSERT_T( graphComponent[graphComponent[v1]] == graphComponent[v1], "Wrong upper graph component" );
		NI_ASSERT_T( graphComponent[graphComponent[v2]] == graphComponent[v2], "Wrong upper graph component" );

		graphComponent[ Max(graphComponent[v1], graphComponent[v2]) ] = Min(graphComponent[v1], graphComponent[v2]);
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraph::ComputePath( const int _v1, const int _v2 )
{
	v1 = _v1;
	v2 = _v2;

	std::unordered_set<int> waitingSet;
	for ( int i = 0; i < GetNNodes(); ++i )
	{
		dst[i] = -1;
		waitingSet.insert( i );
	}
	dst[v1] = 0;
	pred[v1] = -1;

	bool bFinished = false;
	while ( !bFinished && !waitingSet.empty() )
	{
		int nBestNode = -1;
		float fBestDist = 0.0f;
		for ( std::unordered_set<int>::const_iterator iter = waitingSet.begin(); iter != waitingSet.end(); ++iter )
		{
			if ( dst[*iter] != -1 && ( nBestNode == -1 || dst[*iter] < fBestDist ) )
			{
				nBestNode = *iter;
				fBestDist = dst[*iter];
			}
		}

		// ������ ������, ������� � v1, ���, ���� ����� ���� �� v2
		if ( nBestNode == -1 || nBestNode == v2 )
			bFinished = true;
		else
		{
			NI_ASSERT_T( dst[nBestNode] >= 0, "Wrong length of path" );
			waitingSet.erase( nBestNode );
			for ( std::list<int>::const_iterator iter = nodes[nBestNode].begin(); iter != nodes[nBestNode].end(); ++iter )
			{
				if ( waitingSet.find( *iter ) != waitingSet.end() )
				{
					const float fLengthThroughTheBestNode = dst[nBestNode] + GetEdgeLength( nBestNode, *iter );
					if ( dst[*iter] == -1 || dst[*iter] > fLengthThroughTheBestNode )
					{
						dst[*iter] = fLengthThroughTheBestNode;
						pred[*iter] = nBestNode;
					}
				}
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CGraph::GetPathLength() const
{
	return dst[v2];
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraph::GetPath( std::list<int> *pPath ) const
{
	pPath->clear();

	if ( GetPathLength() != -1.0f )
	{
		int nCurNode = v2;
		while ( nCurNode != v1 )
		{
			pPath->push_front( nCurNode );
			nCurNode = pred[nCurNode];
		}

		pPath->push_front( v1 );
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CGraph::Clear()
{
	nodes.clear();
	dst.clear();
	pred.clear();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGraph::IsInOneGraphComponent( const int v1, const int v2 )
{
	FindUpperComponent( v1 );
	FindUpperComponent( v2 );

	return graphComponent[v1] == graphComponent[v2];
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
