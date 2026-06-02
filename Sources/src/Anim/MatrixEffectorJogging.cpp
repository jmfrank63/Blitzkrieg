#include "StdAfx.h"

#include "MatrixEffectorJogging.h"
int CMatrixEffectorJogging::SJogging::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &fPeriod1 );
	saver.Add( 2, &fPeriod2 );
	saver.Add( 3, &fAmp1 );
	saver.Add( 4, &fAmp2 );
	saver.Add( 5, &fPhase1 );
	saver.Add( 6, &fPhase2 );
	return 0;
}
CMatrixEffectorJogging::CMatrixEffectorJogging() 
: matResult( MONE ), lastUpdateTime( 0 )
{  
}
void CMatrixEffectorJogging::SetupData( float fPeriodX1, float fPeriodX2, float fAmpX1, float fAmpX2, float fPhaseX1, float fPhaseX2,
																				float fPeriodY1, float fPeriodY2, float fAmpY1, float fAmpY2, float fPhaseY1, float fPhaseY2,
																				float fPeriodZ1, float fPeriodZ2, float fAmpZ1, float fAmpZ2, float fPhaseZ1, float fPhaseZ2 )
{
	jx.fPeriod1 = fPeriodX1;
	jx.fPeriod2 = fPeriodX2;
	jx.fAmp1 = fAmpX1;
	jx.fAmp2 = fAmpX2;
	jx.fPhase1 = fPhaseX1;
	jx.fPhase2 = fPhaseX2;
	jy.fPeriod1 = fPeriodY1;
	jy.fPeriod2 = fPeriodY2;
	jy.fAmp1 = fAmpY1;
	jy.fAmp2 = fAmpY2;
	jy.fPhase1 = fPhaseY1;
	jy.fPhase2 = fPhaseY2;
	jz.fPeriod1 = fPeriodZ1;
	jz.fPeriod2 = fPeriodZ2;
	jz.fAmp1 = fAmpZ1;
	jz.fAmp2 = fAmpZ2;
	jz.fPhase1 = fPhaseZ1;
	jz.fPhase2 = fPhaseZ2;
}
int CMatrixEffectorJogging::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &matResult );
	saver.Add( 2, &timeStart );
	saver.Add( 3, &lastUpdateTime );
	saver.Add( 4, &jx );
	saver.Add( 5, &jy );
	saver.Add( 6, &jz );
	return 0;
}
bool CMatrixEffectorJogging::Update( const NTimer::STime &time )
{
	if ( time < timeStart )
		return true;

	const float fTime = float( time - timeStart ) / 1000.0f;	// time difference (from the start) in seconds
	const float fValue1 = jx.GetValue( fTime );
	const float fValue2 = jy.GetValue( fTime );
	CQuat quat( fValue1*ToRadian(1.0f), V3_AXIS_X );
	const CQuat q1( fValue2*ToRadian(1.0f), V3_AXIS_Y );
	quat *= q1;
	/*
	const CVec3 vShift = fCoeff1*V3_AXIS_Z*2.0f;
	matResult.Set( vShift, quat );
	*/
	matResult.Set( VNULL3, quat );
	return true;
}
