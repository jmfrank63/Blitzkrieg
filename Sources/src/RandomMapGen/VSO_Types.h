#if !defined(__VSO__Types__)
#define __VSO__Types__

#include "VA_Types.h"
#include "Polygons_Types.h"
#include "../RandomMapGen/Resource_Types.h"

class CVSOBuilder
{
public:
	static const float DEFAULT_WIDTH;
	static const float DEFAULT_HEIGHT;
	static const float DEFAULT_STEP;
	static const float DEFAULT_OPACITY;

private:
	CVSOBuilder() {}

	struct SVSOCircle : public CCircle
	{
		EClassifyRotation classifyRotation;
		CVec2 vCreationPoint;

		bool CreateVSOCircleFromDirection( const CVec2 &vBegin, const CVec2 &vEnd, float _fRadius, EClassifyRotation _classifyRotation, bool bBegin = true );
		bool GetTangentPoint( const CVec2 &v, CVec2 *pTangentPoint ) const;
		bool GetPointsSequence( const CVec2 &v, int nSegmentsCount, std::list<CVec2> *pPointsSequence ) const;
	};
	
public:
	struct SBackupKeyPoints
	{
	private:
		struct SKeyPoint
		{
			float fWidth;
			float fOpacity;
		};
		std::list<SKeyPoint> keyPoints;

	public:
		void SaveKeyPoints( const SVectorStripeObject &rVectorStripeObject );
		void LoadKeyPoints( SVectorStripeObject *pVectorStripeObject );
		void AddKeyPoint( int nKeyPointIndex, float fWidth = DEFAULT_WIDTH, float fOpacity = DEFAULT_OPACITY );
		void RemoveKeyPoint( int nKeyPointIndex );
		
		void InsertToBegin( float fWidth, float fOpacity );
		void InsertToRBegin( float fWidth, float fOpacity );
		void SetBeginOpacity( float fOpacity );
		void SetRBeginOpacity( float fOpacity );
		void Clear();
	};

private:
	static int SliceSpline( const class CAnalyticBSpline2 &spline,
													std::list<SVectorStripeObjectPoint> *pPoints,
													float *pfRest,
													const float fStep );
	static void SampleCurve( const std::vector<CVec3> &rControlPoints,
													 std::vector<SVectorStripeObjectPoint> *pPoints,
													 float fStep, 
													 float fWidth,
													 float fOpacity );
	static void SmoothCurveWidth( std::vector<SVectorStripeObjectPoint> *pPoints );
	
	static bool GetPointsSequence( const SVSOCircle &rCircleBegin, const SVSOCircle &rCircleEnd, int nSegmentsCountBegin, int nSegmentsCountEnd, std::list<CVec2> *pPointsSequence );
	static bool GetPointsSequence( const CVec2 &vBegin0, const CVec2 &vEnd0, float fRadius0, int nSegmentsCount0, bool bBegin0,
																 const CVec2 &vBegin1, const CVec2 &vEnd1, float fRadius1, int nSegmentsCount1, bool bBegin1,
																 std::list<CVec2> *pPointsSequence );
	
public:
	static bool UpdateZ( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, CVec3 *pPos );
	static bool UpdateZ( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, SVectorStripeObject *pVectorStripeObject );
	static bool Update( SVectorStripeObject *pVectorStripeObject, bool bKeepKeyPoints, float fStep, float fWidth, float fOpacity );
	static bool GetVSOPointPolygon( const SVectorStripeObject &rVectorStripeObject, int nPointIndex, std::vector<CVec3> *pPolygon, float fRelWidth = 1.0f );
	static bool FindPath( const CVec2 &vBegin0, const CVec2 &vEnd0, bool bBegin0,
												const CVec2 &vBegin1, const CVec2 &vEnd1, bool bBegin1,
												float fRadius, int nSegmentsCount, float fMinEdgeLength, float fDistance, float fDisturbance, 
												std::list<CVec2> *pPointsSequence, const std::vector<std::vector<CVec2> > &rLockedPolygons, std::list<CVec2> *pUsedPoints,
												int nDepth = 0 );
	static bool MergeVSO( SVectorStripeObject *pVSO0, bool bVSO0Begin,
												SVectorStripeObject *pVSO1, bool bVSO1Begin );

	static float GetVSOEdgeHeght( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, const SVectorStripeObject &rVectorStripeObject, bool bBegin, bool bFirst );

	template<class Type>
	static bool CreateVSO( SVectorStripeObject *pVSO, const std::string &rVSODescName, const Type &rVSOControlPoints )
	{
		SVectorStripeObjectDesc vsoDesc;
		if ( LoadDataResource( rVSODescName, "", false, 0, "VSODescription", vsoDesc ) )
		{
			*( static_cast<SVectorStripeObjectDesc*>( pVSO ) ) = vsoDesc;
			pVSO->szDescName = rVSODescName;

			pVSO->points.clear();
			pVSO->controlpoints.clear();
			for ( typename Type::const_iterator addPontsIterator = rVSOControlPoints.begin(); addPontsIterator != rVSOControlPoints.end(); ++addPontsIterator )
			{
				pVSO->controlpoints.push_back( GetPointType( *addPontsIterator, static_cast<CVec3*>( 0 ) ) );
			}
			UniquePolygon<std::vector<CVec3>, CVec3>( &( pVSO->controlpoints ), RMGC_MINIMAL_VIS_POINT_DISTANCE );
			return true;
		}
		return false;
	}
	
	static std::string GetDescriptionName( const std::string &rszBeginVSODesc, const std::string &rszEndVSODesc )
	{
		return rszBeginVSODesc;
	}
};
#endif // #if !defined(__VSO__Types__)
