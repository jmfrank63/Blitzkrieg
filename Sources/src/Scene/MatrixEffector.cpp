#include "StdAfx.h"

#include "MatrixEffector.h"
int CMatrixEffectorRecoil::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &matResult );
	saver.Add( 2, &timeStart );
	saver.Add( 3, &timeLife );
	saver.Add( 4, &lastUpdateTime );
	saver.Add( 5, &vAxis );
	saver.Add( 6, &fAngle );
	return 0;
}
bool CMatrixEffectorRecoil::Update( const NTimer::STime &time )
{
	if ( time < timeStart )
		return true;
	else if ( time >= timeStart + timeLife )
		return false;
	NTimer::STime timeDiff = time - timeStart;
	if ( timeDiff < timeLife / 10 )
	{
		float fCoeff = float( timeDiff ) / float( timeLife / 10 );
		CQuat quat( fAngle*fCoeff, vAxis );
		quat.MinimizeRotationAngle();
		quat.DecompEulerMatrix( &matResult );
	} 
	else if ( timeDiff >= timeLife / 10 )
	{
		float fCoeff = float( timeLife - timeDiff ) / float( timeLife - timeLife / 10 );
		CQuat quat( fAngle*fCoeff, vAxis );
		quat.MinimizeRotationAngle();
		quat.DecompEulerMatrix( &matResult );
	}
	else
		matResult = MONE;
	return true;
}
int CMatrixEffectorJogging::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &matResult );
	saver.Add( 2, &timeStart );
	saver.Add( 3, &lastUpdateTime );
	saver.Add( 4, &fWeightCoeff );
	return 0;
}
bool CMatrixEffectorJogging::Update( const NTimer::STime &time )
{
	if ( time < timeStart )
		return true;

	const float fPeriod1 = 2.0f;
	const float fPeriod2 = 2.0f;

	const float fDiff = float( time - timeStart ) / 1000.0f * FP_2PI;	// difference in seconds
	const float fCoeff1 = cos( fDiff * fPeriod1 );
	const float fCoeff2 = sin( fDiff * fPeriod2 );
	CQuat quat( fCoeff1*ToRadian(2.0f), V3_AXIS_X );
	CQuat q1( fCoeff2*ToRadian(2.0f), V3_AXIS_Y );
	quat *= q1;
	const CVec3 vShift = fCoeff1*V3_AXIS_Z*10.0f;
	matResult.Set( vShift, quat );
	return true;
}
