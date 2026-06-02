#include "StdAfx.h"
#include "VSO_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

void CVSOBuilder::SBackupKeyPoints::SaveKeyPoints( const SVectorStripeObject &rVectorStripeObject )
{
	keyPoints.clear();
	for ( std::vector<SVectorStripeObjectPoint>::const_iterator pointIterator = rVectorStripeObject.points.begin(); pointIterator != rVectorStripeObject.points.end(); ++pointIterator )
	{
		if ( pointIterator->bKeyPoint )
		{
			SKeyPoint keyPoint;
			keyPoint.fWidth = pointIterator->fWidth;
			keyPoint.fOpacity = pointIterator->fOpacity;
			keyPoints.push_back( keyPoint );
		}
	}
}

void CVSOBuilder::SBackupKeyPoints::LoadKeyPoints( SVectorStripeObject *pVectorStripeObject )
{
	NI_ASSERT_T( pVectorStripeObject != 0,
							 NStr::Format( "Wrong parameter: %x\n", pVectorStripeObject ) );

	std::list<SKeyPoint>::const_iterator keyPointIterator = keyPoints.begin();
	for ( std::vector<SVectorStripeObjectPoint>::iterator pointIterator = pVectorStripeObject->points.begin(); pointIterator != pVectorStripeObject->points.end(); ++pointIterator )
	{
		if ( pointIterator->bKeyPoint && ( keyPointIterator != keyPoints.end() ) )
		{
			pointIterator->fWidth = keyPointIterator->fWidth;
			pointIterator->fOpacity = keyPointIterator->fOpacity;
			++keyPointIterator;
		}
	}
}

void CVSOBuilder::SBackupKeyPoints::AddKeyPoint( int nKeyPointIndex, float fWidth, float fOpacity )
{
	SKeyPoint keyPoint;
	keyPoint.fWidth = fWidth;
	keyPoint.fOpacity = fOpacity;
	
	if ( nKeyPointIndex < 0 )
	{
		keyPoints.push_front( keyPoint );
	}
	else if ( nKeyPointIndex >= keyPoints.size() )
	{
		keyPoints.push_back( keyPoint );
	}
	else
	{
		std::list<SKeyPoint>::iterator keyPointIterator = keyPoints.begin();
		for ( int nIndex = 0; nIndex < nKeyPointIndex; ++nIndex )
		{
			++keyPointIterator;
		}
		keyPoints.insert( keyPointIterator, keyPoint );
	}
}

void CVSOBuilder::SBackupKeyPoints::RemoveKeyPoint( int nKeyPointIndex )
{
	if ( ( nKeyPointIndex >= 0 ) && ( nKeyPointIndex < keyPoints.size() ) )
	{
		std::list<SKeyPoint>::iterator keyPointIterator = keyPoints.begin();
		for ( int nIndex = 0; nIndex < nKeyPointIndex; ++nIndex )
		{
			++keyPointIterator;
		}
		keyPoints.erase( keyPointIterator );
	}
}

void CVSOBuilder::SBackupKeyPoints::InsertToBegin( float fWidth, float fOpacity  )
{
	SKeyPoint keyPoint;
	keyPoint.fWidth = fWidth;
	keyPoint.fOpacity = fOpacity;
	keyPoints.push_front( keyPoint );
}

void CVSOBuilder::SBackupKeyPoints::InsertToRBegin( float fWidth, float fOpacity )
{
	SKeyPoint keyPoint;
	keyPoint.fWidth = fWidth;
	keyPoint.fOpacity = fOpacity;
	keyPoints.push_back( keyPoint );
}

void CVSOBuilder::SBackupKeyPoints::SetBeginOpacity( float fOpacity )
{
	keyPoints.begin()->fOpacity = fOpacity;
}

void CVSOBuilder::SBackupKeyPoints::SetRBeginOpacity( float fOpacity )
{
	keyPoints.rbegin()->fOpacity = fOpacity;
}

void CVSOBuilder::SBackupKeyPoints::Clear()
{
	keyPoints.clear();
}

bool CVSOBuilder::SVSOCircle::CreateVSOCircleFromDirection( const CVec2 &vBegin, const CVec2 &vEnd, float _fRadius, EClassifyRotation _classifyRotation, bool bBegin )
{
	r = _fRadius;
	classifyRotation = _classifyRotation;

	if ( bBegin )
	{
		vCreationPoint = vBegin;
	}
	else
	{
		vCreationPoint = vEnd;
	}

	CVec2 vNormal = GetNormal( vEnd - vBegin );
	Normalize( &vNormal );
	if ( classifyRotation == CR_COUNTERCLOCKWISE )
	{
		center = vCreationPoint + vNormal * r;
	}
	else if ( classifyRotation == CR_CLOCKWISE )
	{
		center = vCreationPoint - vNormal * r;
	}
	else
	{
		return false;
	}
	return true;
}

bool CVSOBuilder::SVSOCircle::GetTangentPoint( const CVec2 &v, CVec2 *pTangentPoint ) const
{
	const CVec2 vCenterV = center - v;
	const float fDistance2 = fabs2( vCenterV );
	const float fRadius2 = sqr( r );
	if ( fDistance2 < fRadius2 )
	{
		return false;
	}
	else if ( fDistance2 == fRadius2 )
	{
		*pTangentPoint = v;
	}
	else
	{
		const float fLeg2 = fDistance2 - fRadius2;
		const float fCossin = float( sqrt( fLeg2 ) ) * r / fDistance2;
		const float fCos2 = fLeg2 / fDistance2;

		if ( classifyRotation == CR_COUNTERCLOCKWISE )
		{
			pTangentPoint->x = v.x + ( vCenterV.x * fCos2 ) - ( vCenterV.y * fCossin );
			pTangentPoint->y = v.y + ( vCenterV.x * fCossin ) + ( vCenterV.y * fCos2 );
		}
		else if ( classifyRotation == CR_CLOCKWISE )
		{
			pTangentPoint->x = v.x + ( vCenterV.x * fCos2 ) + ( vCenterV.y * fCossin );
			pTangentPoint->y = v.y - ( vCenterV.x * fCossin ) + ( vCenterV.y * fCos2 );
		}
		else
		{
			return false;
		}
	}
	return true;
}

bool CVSOBuilder::SVSOCircle::GetPointsSequence( const CVec2 &v, int nSegmentsCount, std::list<CVec2> *pPointsSequence ) const
{
	NI_ASSERT_TF( pPointsSequence != 0,
							  NStr::Format( "Wrong parameter: pPointsSequence %x\n", pPointsSequence ),
								return false );

	float fStartPolarAngle = GetPolarAngle( vCreationPoint - center );
	const float fEndPolarAngle = GetPolarAngle( v - center );
	const float fAngleStep = FP_2PI / nSegmentsCount;
	if ( classifyRotation == CR_CLOCKWISE )
	{
		if ( fStartPolarAngle < fEndPolarAngle )
		{
			fStartPolarAngle += FP_2PI;	
		}
		for ( float fAngle = fStartPolarAngle; fAngle > fEndPolarAngle; fAngle -= fAngleStep )
		{
			pPointsSequence->push_back( center + CreateFromPolarCoord( r, fAngle ) );
		}
	}
	else
	{
		if ( fStartPolarAngle > fEndPolarAngle )
		{
			fStartPolarAngle -= FP_2PI;
		}
		for ( float fAngle = fStartPolarAngle; fAngle < fEndPolarAngle; fAngle += fAngleStep )
		{
			pPointsSequence->push_back( center + CreateFromPolarCoord( r, fAngle ) );
		}
	}
	const float fStepSize2 = fabs2( CreateFromPolarCoord( r, fStartPolarAngle ) - CreateFromPolarCoord( r, fStartPolarAngle + fAngleStep ) );
	if ( fabs2( *( pPointsSequence->rbegin() ) - v ) > ( fStepSize2 / 4.0f ) )
	{
		pPointsSequence->push_back( v );
	}
	return true;
}
