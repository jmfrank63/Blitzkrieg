#ifndef __MATRIXEFFECTOR_H__
#define __MATRIXEFFECTOR_H__
#pragma ONCE
class CMatrixEffectorRecoil : public ISceneEffectorRecoil
{
	OBJECT_NORMAL_METHODS( CMatrixEffectorRecoil );
	DECLARE_SERIALIZE;
	SHMatrix matResult;										// result matrix
	NTimer::STime timeStart;							// effect start time
	NTimer::STime timeLife;								// effect lifetime
	NTimer::STime lastUpdateTime;					// last update time
	CVec3 vAxis;
	float fAngle;
public:
	CMatrixEffectorRecoil() : matResult( MONE ), lastUpdateTime( 0 ), vAxis( V3_AXIS_X ), fAngle( 0 ) {  }
	virtual void STDCALL SetupTimes( const NTimer::STime &_timeStart, const NTimer::STime &_timeLife )
	{
		timeStart = _timeStart;
		timeLife = _timeLife;
	}
	virtual void STDCALL SetupData( float _fAngle, const CVec3 &_vAxis )
	{
		fAngle = _fAngle;
		vAxis = _vAxis;
	}
	virtual bool STDCALL Update( const NTimer::STime &time );
	virtual const SHMatrix& STDCALL GetMatrix() const { return matResult; }
};
class CMatrixEffectorJogging : public ISceneEffectorJogging
{
	OBJECT_NORMAL_METHODS( CMatrixEffectorJogging );
	DECLARE_SERIALIZE;
	SHMatrix matResult;										// result matrix
	NTimer::STime timeStart;							// effect start time
	NTimer::STime lastUpdateTime;					// last update time
	float fWeightCoeff;										// weight of the technics for jogging
public:
	CMatrixEffectorJogging() : matResult( MONE ), lastUpdateTime( 0 ), fWeightCoeff( 1.0f ) {  }
	virtual void STDCALL SetupTimes( const NTimer::STime &_timeStart, const NTimer::STime &_timeLife )
	{
		timeStart = _timeStart;
	}
	virtual void STDCALL SetupData( float _fWeightCoeff )
	{
		fWeightCoeff = _fWeightCoeff;
	}
	virtual bool STDCALL Update( const NTimer::STime &time );
	virtual const SHMatrix& STDCALL GetMatrix() const { return matResult; }
};
#endif // __MATRIXEFFECTOR_H__
