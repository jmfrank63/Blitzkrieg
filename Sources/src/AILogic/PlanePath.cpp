#include "stdafx.h"

#include "PlanePath.h"
#include "BasePathUnit.h"
#include "Aviation.h"
#include "Trigonometry.h"

#include "MPLog.h"
#include "float.h"
extern NTimer::STime curTime;
bool CPlaneSmoothPath::CLinePathFraction::CalcPoint( const CVec2 &vFormationOffset, CVec2 *vPosition, WORD *wUnitDir, float *pfCurvatureRadius, CVec2 *pvSpeed, const float fOffset ) const
{
	const float fPathOffset = - vFormationOffset.x - fOffset;
	if ( fPathOffset >= fLength ) 
		return false;		// the formation point is not near this fraction
	NI_ASSERT_T( fPathOffset >= 0, "wrong offset" );
	
	const CVec2 vNormalStart = vCurPoint + ( vStart - vCurPoint ) * fPathOffset / fLength;
	const CVec2 vLine = vCurPoint - vStart;

	CVec2 vNormalVector ( vLine.y, -vLine.x );
	Normalize( &vNormalVector );
	
	*vPosition = vFormationOffset.y * vNormalVector + vNormalStart;
	*wUnitDir = GetDirectionByVector( vLine );
	*pfCurvatureRadius = 0;
	*pvSpeed = vLine;
	return true;
}
void CPlaneSmoothPath::CArcPathFraction::CalcLenght() 
{ 
	const float fDiff = fabs( float( wFrom ) - float( wCurAngle ) );

	fLenght = fabs( 2 * PI * flyCircle.r * ( fDiff ) / 65535 ); 
}
CPlaneSmoothPath::CArcPathFraction::CArcPathFraction( const CCircle &_flyCircle, const WORD wDirFrom, const int _nAngleSingn )
	: flyCircle( _flyCircle ), wFrom( wDirFrom ), wCurAngle( wDirFrom ), nAngleSingn( _nAngleSingn ), fLenght ( 0 )
{ 
}
void CPlaneSmoothPath::CArcPathFraction::SetCurPos( const SPathVisitor &rVisitor ) 
{ 
	wCurAngle = rVisitor.wCurAngle; 
	CalcLenght(); 
}
void CPlaneSmoothPath::CArcPathFraction::SetFinished( const SPathVisitor &rVisitor ) 
{ 
	wCurAngle = rVisitor.wCurAngle; 
	CalcLenght(); 
}
bool CPlaneSmoothPath::CArcPathFraction::CalcPoint( const CVec2 &vFormationOffset, CVec2 *vPosition, WORD *wUnitDir, float *pfCurvatureRadius, CVec2 *pvSpeed, const float fOffset ) const
{
	const float fPathOffset = - vFormationOffset.x - fOffset;
	if ( fPathOffset >= fLenght ) 
		return false;		// the formation point is not near this fraction
	NI_ASSERT_T( fPathOffset >= 0, "wrong offset" );
	
	const WORD wOffsetAngle = wCurAngle - nAngleSingn * WORD( 65535 * fPathOffset / ( 2.0f * PI * flyCircle.r ));
	
	const CVec2 vRadius = GetVectorByDirection( wOffsetAngle );
	*vPosition = flyCircle.center + vRadius * ( nAngleSingn * vFormationOffset.y + flyCircle.r );
	*pvSpeed = CVec2( - nAngleSingn * vRadius.y, nAngleSingn * vRadius.x ) ;//* ( 1.0f + nAngleSingn * vFormationOffset.y / flyCircle.r );
	*wUnitDir = GetDirectionByVector( *pvSpeed );
	*pfCurvatureRadius = flyCircle.r * nAngleSingn;
	return true;
}
CPlaneSmoothPath::CPlaneSmoothPath( const float _fTurnRadiusMin, const float _fTurnRadiusMax, const float _fSpeed, const float fVerTurnRatio, const bool _bTrackHistory )
: fTurnRadiusMax( _fTurnRadiusMax ), 
	fTurnRadiusMin( _fTurnRadiusMin ), 
	fVerTurnRatio ( fVerTurnRatio ), 
	pUnit( 0 ), bTrackHistory( _bTrackHistory ),
	eState( HS_HEIGHT_OK ), circePoint( VNULL2 ), vCurAngleSpeed( VNULL2 ), bByCircle( false ),
	fTurnR( _fTurnRadiusMin ),
	fAngleSpeed( 0 ),
	bGainHeight( false ),
	bToHorisontal( false ),
	bSmoothTurn( true )
{
	fSpeed = _fSpeed;
	bFinished = true;
	pPath = 0;
}
void CPlaneSmoothPath::SetAviationUnit( IAviationUnit *_pPlane, IBasePathUnit *_pPathUnit )
{
	pUnit = _pPathUnit;
	pPlane = _pPlane;
}
void CPlaneSmoothPath::SetOwner( IBasePathUnit *_pUnit ) 
{ 
	pUnit = _pUnit; 
	NI_ASSERT_T( dynamic_cast<CAviation*>( _pUnit ) != 0, "only CAviation allowed" );
	CAviation * pAviation = static_cast<CAviation*>( _pUnit );
	pPlane = pAviation;
}
IBasePathUnit* CPlaneSmoothPath::GetOwner() const
{
	return pUnit;
}
void CPlaneSmoothPath::SetTurnRadius( float fTurnRadius )
{
	fTurnR = fTurnRadius;
	float fSpeedFactor = fSpeed / fTurnR;
	fAngleSpeed = fSpeedFactor * 65536 / ( 2.0f * PI );
	fSpeedFactor *= (float)SConsts::AI_SEGMENT_DURATION;
	vAngleSpeed = CVec2( NTrg::Cos( fSpeedFactor ), NTrg::Sin( fSpeedFactor ) );
}
void CPlaneSmoothPath::CompareWithBest( const CVec2 &p, CVec2 *bestPoint, WORD *wBestAngle, const CCircle &circle, const short int sign )
{
	WORD final = GetDirectionByVector( p - circle.center );
	WORD start = GetDirectionByVector( pPath->GetStartPoint() - circle.center );

	WORD angle;
	if ( sign > 0 )
		angle = final - start;
	else
		angle = start - final;

	if ( angle <= *wBestAngle )
	{
		*wBestAngle = angle;
		*bestPoint = p;
		startAngle = start;
		finishAngle = final;
		angleSign = sign;
		flyCircle = circle;
	}
}
float CPlaneSmoothPath::GetCurvatureRadius() const
{
	if( bByCircle )
		return angleSign * fTurnR;
	else
		return 0;
}
CVec2 CPlaneSmoothPath::GetCurvatureCenter() const
{
	if ( bByCircle )
		return flyCircle.center;
	else
		return CVec2( float(1e15), float(1e15) );
}
bool CPlaneSmoothPath::Init( IBasePathUnit *_pUnit, IPath *_pPath, bool _bSmoothTurn, bool bCheckTurn )
{
	NI_ASSERT_T( dynamic_cast<CAviation*>( _pUnit ) != 0, "only CAviation allowed" );
	CAviation * pAviation = static_cast<CAviation*>( _pUnit );
	IAviationUnit * pAviationUnit = pAviation;
	return Init( _pUnit, pAviationUnit, _pPath, _bSmoothTurn, bCheckTurn );
}
void CPlaneSmoothPath::FinishPath()
{
	CPathFraction * pFinishedFraction = pathHistory.back();
	if ( bByCircle )
	{
		NI_ASSERT_T( dynamic_cast<CArcPathFraction*>( pFinishedFraction ) != 0, "not arc path fraction after circle fly" );
		SPathVisitor v( GetDirectionByVector( pUnit->GetCenter() - flyCircle.center ) );
		pFinishedFraction->SetFinished( v );
	}
	else
	{
		NI_ASSERT_T( dynamic_cast<CLinePathFraction*>( pFinishedFraction ) != 0, "not line path fraction" );
		SPathVisitor v( pUnit->GetCenter() );
		pFinishedFraction->SetFinished( v );
	}
	if ( 0 == pFinishedFraction->GetLength() )
		pathHistory.pop_back(); 
}
bool CPlaneSmoothPath::Init( interface IBasePathUnit *_pPathUnit, interface IAviationUnit *pAviationUnit, IPath *_pPath, bool _bSmoothTurn, bool bCheckTurn ) 
{ 
	pUnit = _pPathUnit;
	segmentTime = curTime - 1;

	pPlane = pAviationUnit;
	bSmoothTurn = _bSmoothTurn;
	SetTurnRadius( bSmoothTurn ? fTurnRadiusMax : fTurnRadiusMin );
	CPtr<IPath> p = _pPath;
	NI_ASSERT_T( dynamic_cast<CPlanePath*>(_pPath) != 0, "wrong call" );
	CPtr<CPlanePath> pPathTmp = static_cast<CPlanePath*>( _pPath );
	
	if ( pPath && !bFinished &&
			 pPathTmp->GetFinishZ() == pPath->GetFinishZ() &&
			 pPathTmp->GetFinishPoint() == pPath->GetFinishPoint() )
	{
		return true; // path already initted.
	}

	if ( bTrackHistory )
	{
		if ( pathHistory.empty() )
		{
			pathHistory.push_back( new CLinePathFraction( _pPathUnit->GetCenter() - _pPathUnit->GetDirVector() * 20000) );
			bByCircle = false;
		}
		FinishPath();
	}

	pPath = pPathTmp;
	CVec2 vDirVector( pUnit->GetDirVector() );
	CCircle c1, c2;
	GetCirclesByTangent( vDirVector, pPath->GetStartPoint(), fTurnR, &c1, &c2 );
	
	CVec2 p1, p2, bestPoint;
	WORD wBestAngle = 65535;
	const CVec2 &finishPoint = pPath->GetFinishPoint();
	const CVec2 &startPoint = pPath->GetStartPoint();

	bool bFoundCircle = false;
	short int sign = Sign( STriangle( c1.center, startPoint, startPoint + pUnit->GetDirVector() ) );
	if ( FindTangentPoints( pPath->GetFinishPoint(), c1, &p1, &p2 ) )
	{
		if ( Sign( STriangle( c1.center, p1, finishPoint ) ) == sign )
			CompareWithBest( p1, &bestPoint, &wBestAngle, c1, sign );
		if ( Sign( STriangle( c1.center, p2, finishPoint ) ) == sign )
			CompareWithBest( p2, &bestPoint, &wBestAngle, c1, sign );
		bFoundCircle = true;
	}
		

	sign = Sign( STriangle( c2.center, startPoint, startPoint + pUnit->GetDirVector() ) );
	if ( FindTangentPoints( finishPoint, c2, &p1, &p2 ) )
	{
		if ( Sign( STriangle( c2.center, p1, finishPoint ) ) == sign )
			CompareWithBest( p1, &bestPoint, &wBestAngle, c2, sign );
				
		if ( Sign( STriangle( c2.center, p2, finishPoint ) ) == sign )
			CompareWithBest( p2, &bestPoint, &wBestAngle, c2, sign );
		bFoundCircle = true;
	}
	
	if ( !bFoundCircle || fabs2( bestPoint - startPoint ) <= 10.0f )
	{
		bestPoint = startPoint;
		dirByLine = finishPoint - bestPoint;

		Normalize( &dirByLine );
		bByCircle = false;
		if ( bTrackHistory )
		{
			CLinePathFraction * pFraction = new CLinePathFraction( startPoint );
			pathHistory.push_back( pFraction );
		}
		vCurAngleSpeed = VNULL2;
	}
	else
	{
		NI_ASSERT_T( bFoundCircle, "circle NOT FOUND, but decided that it is curcle" );
		if ( !bFoundCircle )
		{
			flyCircle.center = VNULL2;
			flyCircle.r = 1000;
		}
		vCurAngleSpeed.x = vAngleSpeed.x;
		vCurAngleSpeed.y = angleSign * vAngleSpeed.y;
		bByCircle = true;
		if ( bTrackHistory ) // track history
		{
			CArcPathFraction * pFraction = new CArcPathFraction( flyCircle, startAngle, angleSign );
			pathHistory.push_back( pFraction );
		}
	}
	bFinished = false;


	if ( IsHeightOK( pUnit, pPlane, pPath->GetFinishZ(), fAngleSpeed * fVerTurnRatio ) )
	{
		eState = HS_HEIGHT_OK;
		fDistanceToChangeHeight = 0;
		bToHorisontal = true;
		bGainHeight = false;
	}
	else 
	{
		bToHorisontal = false;
		dirByLine = pUnit->GetDirVector();

		const float dZ = pPath->GetFinishZ() - pUnit->GetZ();
		
		CVec2 vDesiredDir;
		if ( pPath->GetFinishZ() > pUnit->GetZ() )// набор высоты
			vDesiredDir = GetVectorByDirection( pPlane->GetClimbingAngle() + 65535*3/4 );
		else																			// пикирование
			vDesiredDir = GetVectorByDirection( -pPlane->GetDivingAngle() + 65535*3/4 );
		
		fDistanceToChangeHeight = CalcCriticalDistance( vDesiredDir, fVerTurnRatio, fTurnR ) + dZ / vDesiredDir.y * vDesiredDir.x;

		CVec2 vSpeedHorVer = pPlane->GetSpeedHorVer();
		bGainHeight = vSpeedHorVer.y != 0;
	}

	
	return true;
}
void CPlaneSmoothPath::CalculateMemberInfo( const CVec2 &vFormationOffset, SMemberInfo *pMemberInfo ) const
{
	NI_ASSERT_T( bTrackHistory, "canot calc formation offset without history" );
	NI_ASSERT_T( vFormationOffset.x <= 0, "cannot predict future. sorry." );
	float fOffset = 0;
	for ( CPathHistory::const_reverse_iterator it = pathHistory.rbegin(); it != pathHistory.rend(); ++it )
	{
		CPathFraction * pFraction = *it;
		if ( pFraction->CalcPoint(	vFormationOffset, 
																&pMemberInfo->vWorldPosition, 
																&pMemberInfo->wDirection, 
																&pMemberInfo->fCurvatureRadius,
																&pMemberInfo->vSpeed,
																fOffset) )
		{
			pMemberInfo->vSpeed *= fSpeed;
			return ;
		}
		else
			fOffset += pFraction->GetLength();
	}

	NI_ASSERT_T( false, "out of path limits" );
}
void CPlaneSmoothPath::ClearUnisedHistory()
{
	/*
	for ( CPathHistory::iterator it = pathHistory.begin(); it != pathHistory.end(); )
	{
		if ( (*it)->IsActive() ) return;
		it = pathHistory.erase( it );
	}
	*/
}
float CPlaneSmoothPath::CalcCriticalDistance( const CVec2 &vSpeedHorVer, const float _fVerTurnRatio, const float _fTurnRadius )
{
	float fAlpha = 0.5f * NTrg::ASin( vSpeedHorVer.y / fabs( vSpeedHorVer ) );
	float fCrit = 2 * _fTurnRadius / _fVerTurnRatio * sqr( NTrg::Sin( fAlpha ) );
	return fCrit;
}
float CPlaneSmoothPath::Calc2DDistanceToGo() const
{
	if ( bFinished )
	{
		return 0;
	}

	if ( bByCircle )
	{
		const WORD curAngle = GetDirectionByVector( pUnit->GetCenter() - flyCircle.center );		
		const WORD angleToFly = angleSign * ( finishAngle - curAngle );
		const float fDistCircle = angleToFly * fTurnR / 65535 ;
		const float fDistLine = fabs( pPath->GetFinishPoint() -  flyCircle.center + fTurnR * GetVectorByDirection(finishAngle) );
		return fDistCircle + fDistLine;
	}
	else
	{
		return fabs( pPath->GetFinishPoint() - pUnit->GetCenter() );
	}
}
const CVec3 CPlaneSmoothPath::GetPoint( NTimer::STime timeDiff )
{
	if ( segmentTime == curTime )
	{
		const CVec3 vResult = pPlane->GetNewPoint();
		return vResult;
	}
	segmentTime = curTime;
	
	const CVec2 vFormerPosition( pUnit->GetCenter() );
	const WORD wFormerDir( pUnit->GetDir() );
	CVec2 result( vFormerPosition );

	CVec2 vSpeedHorVer = pPlane->GetSpeedHorVer();

	const float fDistToGo = Calc2DDistanceToGo();
	if ( !bGainHeight && ( fDistToGo <= fDistanceToChangeHeight * 2 || 
				DirsDifference( GetDirectionByVector( pPath->GetFinishPoint() - pUnit->GetCenter() ),
												pUnit->GetDir() ) < 65535 * 45 / 360 ) )
		bGainHeight = true;

	float fCurrentZ = pUnit->GetZ();
	CVec2 vDesiredSpeed( fSpeed, 0.0f ); // по умолчанию - полет горизонтальный.
	CVec2 vTmpSpeed( fSpeed, 0.0f );
	
	if ( bGainHeight )
	{
		const float fFinalZ = pPath->GetFinishZ();
		WORD wAlpha = fAngleSpeed * fVerTurnRatio * timeDiff;
		float fAlpha = 2 * PI / 65535 * wAlpha;

		if ( fFinalZ != -1.0f )
		{
			float dZ = fFinalZ - fCurrentZ;
			float fHCrit = CalcCriticalDistance( vSpeedHorVer, fVerTurnRatio, fTurnR );

			if ( bToHorisontal )
			{
				if ( 0.0f == vSpeedHorVer.y ) // достигли горизонтального полета
					bToHorisontal = false;
			}
			else if	( fHCrit > fabs( dZ ) ||
					( vSpeedHorVer.y < 0.0f && fCurrentZ < fHCrit + SConsts::PLANE_MIN_HEIGHT ) ||
						IsHeightOK( pUnit, pPlane, pPath->GetFinishZ(), fAngleSpeed * fVerTurnRatio )
					)
			{// обработать выход из пикирования/кабрирования.
				bToHorisontal = true;
			}
			else if ( dZ < 0.0f ) // снижение
			{
				CVec2 vDesiredSpeed2 ( fDistToGo, dZ );
				Normalize( &vDesiredSpeed2 );
				vDesiredSpeed = GetVectorByDirection( - pPlane->GetDivingAngle() + 65535*3/4 );

				if ( fabs(vDesiredSpeed2.y) < fabs(vDesiredSpeed.y) ) 
				{
					vDesiredSpeed = vDesiredSpeed2;	// разрешить
				}
				vDesiredSpeed *= fSpeed;
			}
			else if ( dZ > 0.0f ) // подъем
			{
				vDesiredSpeed = GetVectorByDirection( pPlane->GetClimbingAngle() + 65535*3/4 );
				CVec2 vDesiredSpeed2 ( fDistToGo, dZ );
				Normalize( &vDesiredSpeed2 );
				if ( fabs(vDesiredSpeed2.y) < fabs(vDesiredSpeed.y) ) 
				{
					vDesiredSpeed = vDesiredSpeed2 ;	// разрешить
				}
				vDesiredSpeed *= fSpeed;
			}

			if ( wAlpha >= DirsDifference(	GetDirectionByVector( vDesiredSpeed ), 
																			GetDirectionByVector( vSpeedHorVer ) ) )
			{
				vTmpSpeed = vDesiredSpeed;
			}
			else
			{
				if ( vSpeedHorVer.x * vDesiredSpeed.y - vSpeedHorVer.y * vDesiredSpeed.x > 0 ) 
					fAlpha = fAlpha;						// поднять нос
				else 
					fAlpha= -fAlpha;						// опустить нос

				const float cosAlpha = NTrg::Cos(fAlpha);
				const float sinAlpha = NTrg::Sin(fAlpha);

				vTmpSpeed.x = vSpeedHorVer.x * cosAlpha - vSpeedHorVer.y * sinAlpha;
				vTmpSpeed.y = vSpeedHorVer.x * sinAlpha + vSpeedHorVer.y * cosAlpha;
			}
		}

  	if ( !bGainHeight || vSpeedHorVer.x == 0 )// либо начало, либо полет по горизонтали
		{
			vSpeedHorVer = vDesiredSpeed;
		}
		else
		{
			vSpeedHorVer = vTmpSpeed;
		}

		pPlane->SetSpeedHorVer( vSpeedHorVer );
		if ( !bByCircle )
		{
			pUnit->UpdateDirection( dirByLine );
			result += dirByLine * timeDiff * vSpeedHorVer.x;
		}

		fCurrentZ += vSpeedHorVer.y * timeDiff;
	}

	if ( bByCircle )	
	{
		const CVec2 vRes( flyCircle.center + ((pUnit->GetCenter() - flyCircle.center) ^ vCurAngleSpeed) );

		result = result + ( vRes - result ) * vSpeedHorVer.x / fSpeed;

		const CVec2 rVec = result - flyCircle.center;

		if ( fabs2( rVec ) > 0.01f ) 
		{
			if ( angleSign > 0 )
				pUnit->UpdateDirection( CVec2( -rVec.y, rVec.x ) );
			else
				pUnit->UpdateDirection( CVec2( rVec.y, -rVec.x ) );
		}

		const WORD curAngle = GetDirectionByVector( result - flyCircle.center );		
		const WORD angleToFly = DirsDifference( finishAngle, curAngle );
		
		if ( angleToFly / (fAngleSpeed * vSpeedHorVer.x / fSpeed ) <= timeDiff )
			finishAngle = curAngle;
		if ( curAngle == finishAngle )
		{
			bByCircle = false;
			const CVec2 vPathFinishPoint = pPath->GetFinishPoint();
			dirByLine = vPathFinishPoint - pUnit->GetCenter();

			if ( Normalize( &dirByLine ) ) // unit may be at destination point already. normalize will fail.
			{
				pUnit->UpdateDirection( dirByLine );
			}
			else
				dirByLine = pUnit->GetDirVector();

			if ( bTrackHistory )
			{
				CPathFraction * pFinishedFraction = pathHistory.back();
				NI_ASSERT_T( dynamic_cast<CArcPathFraction*>( pFinishedFraction ) != 0, "not arc path fraction after circle fly" );
				SPathVisitor v( curAngle );
				pFinishedFraction->SetFinished( v );
				CLinePathFraction * pNewFraction = new CLinePathFraction( result );
				pathHistory.push_back( pNewFraction );
			}
		}
		else if ( bTrackHistory )
		{
			CPathFraction * pFraction = pathHistory.back();
			NI_ASSERT_T( dynamic_cast<CArcPathFraction*>( pFraction ) != 0, "not arc path fraction after circle fly" );
			SPathVisitor v( curAngle );
			pFraction->SetCurPos( v );
		}
	}
	else if ( bTrackHistory )
	{
		CPathFraction * pFraction = pathHistory.back();
		NI_ASSERT_T( dynamic_cast<CLinePathFraction*>( pFraction ) != 0, "not line path fraction durnig straight flyght" );
		SPathVisitor v( result );
		pFraction->SetCurPos( v );
	}

	if ( !bFinished )
	{
		float timeToFinish = fabs2( pPath->GetFinishPoint() - result ) / sqr( fSpeed );
		if ( timeToFinish <= fabs2( timeDiff ) )
			Stop();
	}

	const CVec3 vResult( result, fCurrentZ );
	return vResult;
} 
bool CPlaneSmoothPath::IsFinished() const 
{ 
	return bFinished || !pPath; 
}
bool CPlaneSmoothPath::IsHeightOK( const IBasePathUnit *pUnit, const float fZ, const float fAngleSpeed )
{
	NI_ASSERT_T( dynamic_cast<const CAviation*>(pUnit)!=0, "only planes can move by plane's path" );
	const IAviationUnit *pAviation = static_cast<const CAviation*>( pUnit );
	return IsHeightOK( pUnit, pAviation, fZ, fAngleSpeed );
}
bool CPlaneSmoothPath::IsHeightOK( const IBasePathUnit *pUnit, const IAviationUnit *pPlane, const float fZ, const float fAngleSpeed )
{
	CVec2 vSpeedHorVer = pPlane->GetSpeedHorVer();
	float fCurrentZ = pUnit->GetZ();
	WORD wAlpha = fAngleSpeed * SConsts::AI_SEGMENT_DURATION;
	return	fabs( fCurrentZ - fZ ) < static_cast<int>( SConsts::TILE_SIZE ) &&
		wAlpha >= DirsDifference(	GetDirectionByVector(1,0),GetDirectionByVector(vSpeedHorVer) );			
}
bool CPlaneSmoothPath::Init( interface IMemento *pMemento, interface IBasePathUnit *pUnit )
{
	NI_ASSERT_T( false, "Wrong call" );

	return true;
}
bool CPlaneSmoothPath::InitByFormationPath( CFormation *pFormation, IBasePathUnit *pUnit  )
{
	NI_ASSERT_T( false, "Wrong call" );

	return true;
}
IMemento* CPlaneSmoothPath::GetMemento() const
{
	NI_ASSERT_T( false, "Wrong call" );
	return 0;
}
bool CPlaneInFormationSmoothPath::CanGoBackward() const 
{ 
	return false; 
}
void CPlaneInFormationSmoothPath::SetOwner( interface IBasePathUnit *pUnit ) 
{ 
	NI_ASSERT_T(dynamic_cast<CAviation*>(pUnit) != 0, "wrong call" ); 
	pOwner = static_cast<CAviation*>( pUnit );
}
IBasePathUnit* CPlaneInFormationSmoothPath::GetOwner() const
{
	return pOwner;
}
void CPlaneInFormationSmoothPath::Init( class CAviation *_pOwner ) 
{ 
	pOwner = _pOwner; 
	pFormation = pOwner->GetPlanesFormation(); 
}
const CVec2& CPlaneInFormationSmoothPath::GetFinishPoint() const 
{ 
	return pFormation->GetCurPath()->GetFinishPoint(); 
}
bool CPlaneInFormationSmoothPath::IsFinished() const 
{ 
	return pFormation->GetCurPath()->IsFinished(); 
}
const CVec3 CPlaneInFormationSmoothPath::GetPoint( NTimer::STime timeDiff ) 
{ 
	const CVec3 vNewPoint = pFormation->GetCurPath()->GetPoint( timeDiff );
	const CVec3 vNewPlanePoint( pFormation->GetPointByFormationOffset( pOwner->GetPlaneShift() ),
															pFormation->GetZ() );
	pFormation->SetNewPos( vNewPoint );
	
	return vNewPlanePoint;
}
float CPlaneInFormationSmoothPath::GetCurvatureRadius() const
{
	return pFormation->GetCurPath()->GetCurvatureRadius();
}
CVec2 CPlaneInFormationSmoothPath::GetCurvatureCenter() const
{
	return pFormation->GetCurPath()->GetCurvatureCenter();
}
float& CPlaneInFormationSmoothPath::GetSpeedLen() 
{ 
	return pFormation->GetCurPath()->GetSpeedLen(); 
}
