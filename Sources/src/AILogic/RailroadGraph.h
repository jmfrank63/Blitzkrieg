#ifndef __RAILROADGRAPH_H__
#define __RAILROADGRAPH_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Graph.h"
#include "..\Misc\Spline.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IEdge;
class CEdgePoint : public IRefCount
{
	OBJECT_COMPLETE_METHODS( CEdgePoint );
	DECLARE_SERIALIZE;
	
	IEdge *pEdge;
	int nPart;
	float fT;
public:
	CEdgePoint() : nPart( -1 ), fT( -1.0f ), pEdge( 0 ) { }
	CEdgePoint( const CEdgePoint &edgePoint );
	CEdgePoint( IEdge *pEdge, const int nPart, const float fT ) : pEdge( 0 ) { Init( pEdge, nPart, fT ); }
	void Init( IEdge *pEdge, const int nPart, const float fT );

	const CVec2 Get2DPoint() const;
	const CVec2 GetTangent() const;
	IEdge* GetEdge() const { return pEdge; }
	bool IsLastPointOfEdge() const;
	bool IsEqual( CEdgePoint *pEdgePoint ) const;

	const bool Less( const CEdgePoint &point ) const
	{
		NI_ASSERT_T( pEdge == point.pEdge, "Can't compare edge point. Belong to different edges" );
		if ( nPart < point.nPart )
			return true;
		if ( nPart == point.nPart && fT < point.fT )
			return true;
		return false;
	}

	// ������������� ��������� ��� ��� �� ����� �� �������� ����� ( (v1, v2) -> (v2, v1) )
	void Reverse( IEdge *pReversedEdge );

	//
	const int GetNPart() const { return nPart; }
	const float GetT() const { return fT; }

	friend class CSplineEdge;
	friend class CZeroEdge;
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SEdgeLessFunctional
{
	bool operator ()( const CPtr<CEdgePoint> &point1, const CPtr<CEdgePoint> &point2 ) const { return point1->Less( *point2 ); }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ���������� ����� ������� �� ����� �����
const float fabs( CEdgePoint *p1, CEdgePoint *p2 );
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IEdge : public IRefCount
{
	virtual IEdge* CreateEdge( CEdgePoint *p1, CEdgePoint *p2 ) = 0;
	virtual IEdge* CreateReversedEdge() const = 0;

	virtual void SetNodesNumbers( const int _v1, const int _v2 ) = 0;
	virtual const int GetNParts() const = 0;
	virtual const CVec2 GetCoordinate( const int nPart, const float fT ) const = 0;
	virtual const CVec2 GetTangent( const int nPart, const float fT ) const = 0;

	virtual const CVec2 GetTangentOfBegin() const = 0;
	virtual const CVec2 GetTangentOfEnd() const = 0;

	virtual CEdgePoint* CreateFirstEdgePoint() = 0;
	virtual CEdgePoint* CreateLastEdgePoint() = 0;

	virtual const int GetFirstNode() const = 0;
	virtual const int GetLastNode() const = 0;

	virtual const CVec2 GetFirst2DPoint() const = 0;
	virtual const CVec2 GetLast2DPoint() const = 0;

	// ����� ��� �����
	virtual const float GetLength() const = 0;
	// ���������� ����� ����� ������� �� ���� �����
	virtual const float GetLength( CEdgePoint *p1, CEdgePoint *p2 ) = 0;
	virtual void GetClosestPoints( const CVec2 &vPoint, std::list< CPtr<CEdgePoint> > *pPoints, float *pfMinDist, const float fTolerance = SConsts::CLOSEST_TO_RAILROAD_POINT_TOLERANCE ) = 0;

	virtual bool IsLastPoint( const int nPart, const float fT ) const = 0;

	virtual CEdgePoint* MakeIndent( const CVec2 &vPointToMeasureDist, CEdgePoint *p1, CEdgePoint *p2, const float fDist ) = 0;
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSplineEdge : public IEdge
{
	DECLARE_SERIALIZE;

	static const int N_PARTS_FOR_LENGTH_CALCULATING;

	struct SEdgePart
	{
		DECLARE_SERIALIZE;
	public:
		CAnalyticBSpline2 spline;
		float fTBegin, fTEnd;

		SEdgePart() : fTBegin( -1.0f ), fTEnd( -1.0f ) { }
	};

	std::vector<SEdgePart> edgeParts;

	// ������ ������ ������ � ����� �����
	int v1, v2;
	float fEdgeLength;

	// ����� ����� nPart �� fBegin �� fEnd
	const float CalculateLengthOfEdgePart( const int nPart, const float fBegin, const float fEnd );
	// ����� ����� �����
	void CalculateEdgeLength();
	void Init( CEdgePoint *p1, CEdgePoint *p2 );
	CEdgePoint* MakeIndentOnOneSpline( const CVec2 &vPointToMeasureDist, const int nPart, const float fTBegin, const float fTEnd, float fDist );
public:
	CSplineEdge() : v1( -1 ), v2( -1 ), fEdgeLength( -1 ) { }
	explicit CSplineEdge( const struct SVectorStripeObject &edgeDescriptor );
	//
	CSplineEdge( CEdgePoint *p1, CEdgePoint *p2 ) { Init( p1, p2 ); }

	virtual IEdge* CreateReversedEdge() const;

	virtual void SetNodesNumbers( const int _v1, const int _v2 ) { v1 = _v1; v2 = _v2; }
	virtual const int GetNParts() const { return edgeParts.size(); }
	virtual const CVec2 GetCoordinate( const int nPart, const float fT ) const;
	virtual const CVec2 GetTangent( const int nPart, const float fT ) const;

	virtual const CVec2 GetTangentOfBegin() const;
	virtual const CVec2 GetTangentOfEnd() const;

	virtual CEdgePoint* CreateFirstEdgePoint();
	virtual CEdgePoint* CreateLastEdgePoint();

	virtual const int GetFirstNode() const { return v1; }
	virtual const int GetLastNode() const { return v2; }

	virtual const CVec2 GetFirst2DPoint() const;
	virtual const CVec2 GetLast2DPoint() const;

	virtual const float GetLength() const { NI_ASSERT_T( fEdgeLength >= 0.0f, "Edge length hasn't been initialized" ); return fEdgeLength; }
	// ���������� ����� ����� ������� �� ���� �����
	virtual const float GetLength( CEdgePoint *p1, CEdgePoint *p2 );
	virtual void GetClosestPoints( const CVec2 &vPoint, std::list< CPtr<CEdgePoint> > *pPoints, float *pfMinDist, const float fTolerance = SConsts::CLOSEST_TO_RAILROAD_POINT_TOLERANCE );

	virtual CEdgePoint* MakeIndent( const CVec2 &vPointToMeasureDist, CEdgePoint *p1, CEdgePoint *p2, const float fDist );

	virtual bool IsLastPoint( const int nPart, const float fT ) const;
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSimpleSplineEdge : public CSplineEdge
{
	OBJECT_COMPLETE_METHODS( CSimpleSplineEdge );
	DECLARE_SERIALIZE;
public:
	CSimpleSplineEdge() { }
	explicit CSimpleSplineEdge( const struct SVectorStripeObject &edgeDescriptor ) : CSplineEdge( edgeDescriptor ) { }
	//
	CSimpleSplineEdge( CEdgePoint *p1, CEdgePoint *p2 ) : CSplineEdge( p1, p2 ) { }

	IEdge* CreateEdge( CEdgePoint *p1, CEdgePoint *p2 );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CZeroEdge : public IEdge
{
	OBJECT_COMPLETE_METHODS( CZeroEdge );
	DECLARE_SERIALIZE;

	CVec2 vFirstPoint, vDir;
	float fTBegin, fTEnd;
	int v1, v2;
	float fLength;
public:
	CZeroEdge() : fLength( -1.0f ) { }
	// ������� �� ����� p1 ������ ����� �� ����� p2 ������� ����� �� �������� �����
	CZeroEdge( CEdgePoint *p1, CEdgePoint *p2 );

	IEdge* CreateEdge( CEdgePoint *p1, CEdgePoint *p2 );
	virtual IEdge* CreateReversedEdge() const;

	virtual void SetNodesNumbers( const int _v1, const int _v2 ) { v1 = _v1; v2 = _v2; }
	virtual const int GetNParts() const { return 1; }
	virtual const CVec2 GetCoordinate( const int nPart, const float fT ) const;
	virtual const CVec2 GetTangent( const int nPart, const float fT ) const;

	virtual const CVec2 GetTangentOfBegin() const;
	virtual const CVec2 GetTangentOfEnd() const;

	virtual CEdgePoint* CreateFirstEdgePoint();
	virtual CEdgePoint* CreateLastEdgePoint();

	virtual const int GetFirstNode() const { return v1; }
	virtual const int GetLastNode() const { return v2; }

	virtual const CVec2 GetFirst2DPoint() const { return vFirstPoint + vDir * fTBegin; }
	virtual const CVec2 GetLast2DPoint() const { return vFirstPoint + vDir * fTEnd; }

	virtual const float GetLength() const { NI_ASSERT_T( fLength >= 0.0f, "Edge length hasn't been initialized" ); return 0; }
	// ���������� ����� ����� ������� �� ���� �����
	virtual const float GetLength( CEdgePoint *p1, CEdgePoint *p2 ) { return 0; }
	virtual void GetClosestPoints( const CVec2 &vPoint, std::list< CPtr<CEdgePoint> > *pPoints, float *pfMinDist, const float fTolerance = SConsts::CLOSEST_TO_RAILROAD_POINT_TOLERANCE );

	virtual CEdgePoint* MakeIndent( const CVec2 &vPointToMeasureDist, CEdgePoint *p1, CEdgePoint *p2, const float fDist );

	virtual bool IsLastPoint( const int nPart, const float fT ) const;
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CRailroad : public CSplineEdge
{
	OBJECT_COMPLETE_METHODS( CRailroad );
	DECLARE_SERIALIZE;

	std::vector< CPtr<CEdgePoint> > intersectionPoints;
	int nIntersectionPoints;

	std::unordered_map< CPtr<CEdgePoint>, int, SDefaultPtrHash > intersectionPointToGraphNode;
public:
	CRailroad() : nIntersectionPoints( 0 ), intersectionPoints( 10 ) { }
	explicit CRailroad( const struct SVectorStripeObject &edgeDescriptor ) : nIntersectionPoints( 0 ), intersectionPoints( 10 ), CSplineEdge( edgeDescriptor ) { }

	void AddIntersectionPoint( CEdgePoint *pPoint );
	void SetEdges( class CRailroadGraph *pGraph );

	IEdge* CreateEdge( CEdgePoint *p1, CEdgePoint *p2 ) { return 0; }

	const int GetNodeByIntersectionPoint( CEdgePoint *pPoint );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CRailroadsIntersection
{
	DECLARE_SERIALIZE;
	
	CPtr<CEdgePoint> p1;
	CPtr<CEdgePoint> p2;
public:
	CRailroadsIntersection() { }
	CRailroadsIntersection( CEdgePoint *_p1, CEdgePoint *_p2 ) : p1( _p1 ), p2( _p2 ) { }

	CEdgePoint* GetPoint1() const { return p1; }
	CEdgePoint* GetPoint2() const { return p2; }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CRailroadGraphConstructor
{
	DECLARE_SERIALIZE;

	static const int F_RAILROAD_WIDTH_2;

	typedef std::list< CPtr<CRailroad> > CRailroadsList;
	CRailroadsList railroads;
	std::list<CRailroadsIntersection> intersections;

	//
	void SpliceRailroad( CRailroad *pRailroad, std::vector< CPtr<CEdgePoint> > *pPoints, int *pnLen );
	void FindIntersections( CRailroad *pRailroad1, CRailroad *pRailroad2 );
	void AddIntersectionEdge( const CRailroadsIntersection &intersection, class CRailroadGraph *pGraph );
public:
	CRailroadGraphConstructor() { }

	void Construct( const struct STerrainInfo &terrain, class CRailroadGraph *pGraph );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPointInfo
{
	int v;
	CVec2 vDir;

	SPointInfo() : v( -1 ) { }
	SPointInfo( int _v, const CVec2 &_vDir ) : v( _v ), vDir( _vDir ) { }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CRailroadGraph : public CGraph
{
	DECLARE_SERIALIZE;

	std::unordered_map< DWORD, CObj<IEdge> > edges;
	std::vector< CObj<CEdgePoint> > edgeNodes;

	//
	void LookForPoint( const int v, const CVec2 &vDir, std::unordered_set<int> *pVisitedPoints, std::list<SPointInfo> *pPointsList );
	// ���������� v2 �����, ��� ����� � ����� � ������������ vDir ��� �������� �� ����� ( v, v2 ) � dir ��� ��� ����� ����� � vDir
	// ���� v2 �� �������, �� ���������� -1
	void GetMovablePoint( const int v, const CVec2 &vDir, std::unordered_set<int> *pVisitedPoints, std::list<SPointInfo> *pPointsList );
public:
	CRailroadGraph() : edgeNodes( 10 ) { }
	void Clear();

	void AddEdge( IEdge *pEdge );
	virtual const float GetEdgeLength( const int v1, const int v2 );

	// vConnectionNode - ��������� ������� ������ ������ � ��� �� ���������� ���������, ��� � vConnectionNode, ���� vConnectionNode == -1, �� � �����
	void GetClosestPoints( const CVec2 &vPoint, std::list< CPtr<CEdgePoint> > *pPoints, float *pfMinDist, const int vConnectionNode, const float fTolerance = SConsts::CLOSEST_TO_RAILROAD_POINT_TOLERANCE );

	// ����� �� ���� ��������
	IEdge* GetEdge( const int v1, const int v2 );
	// edgePoint �� �������
	CEdgePoint* GetEdgePoint( const int v ) const;

	CEdgePoint* MakeIndent( const CVec2 &vDir, CEdgePoint *pPoint, const float fDist );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __RAILROADGRAPH_H__
