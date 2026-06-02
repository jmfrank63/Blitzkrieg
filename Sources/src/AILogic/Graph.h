#ifndef __GRAPH_H__
#define __GRAPH_H__
#pragma ONCE
class CGraph
{
	DECLARE_SERIALIZE;

	std::vector<float> dst;
	std::vector<int> pred;

	std::vector<int> graphComponent;

	int v1, v2;

	void FindUpperComponent( const int v );
protected:
	std::vector< std::list<int> > nodes;
	int n;
public:
	CGraph() : n( 0 ), v1( -1 ), v2( -1 ) { }
	void Clear();

	const int GetNNodes() const { return n; }
	void AddEdge( const int v1, const int v2 );

	void ComputePath( const int v1, const int v2 );
	const float GetPathLength() const;
	void GetPath( std::list<int> *pPath ) const;

	virtual const float GetEdgeLength( const int v1, const int v2 ) = 0;

	bool IsInOneGraphComponent( const int v1, const int v2 );
};
#endif // __GRAPH_H__
