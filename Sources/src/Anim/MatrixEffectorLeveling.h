#ifndef __MATRIXEFFECTORLEVELING_H__
#define __MATRIXEFFECTORLEVELING_H__
class CMatrixEffectorLeveling : public IMatrixEffector
{
	OBJECT_NORMAL_METHODS( CMatrixEffectorLeveling );
	DECLARE_SERIALIZE;
	SHMatrix matResult;										// current result matrix
	DWORD dwReferNormale;									// last setuped normale
	CVec3 vDesiredNormale;								// desired normale to reach
	float fPhi, fTheta;										// theta and phi angles
	CQuat lastQuat;												// last quaternion
	float fCoeff;
	NTimer::STime lastUpdateTime;					// last update time
	NTimer::STime timeSetuped;						// time, normale was setuped
	CMatrixEffectorLeveling();
public:
	virtual void STDCALL SetupTimes( const NTimer::STime &_timeStart, const NTimer::STime &_timeLife )
	{
	}
	virtual void STDCALL SetupData( const CVec3 &vNormale, const NTimer::STime &currTime );
	virtual bool STDCALL Update( const NTimer::STime &time );
	virtual const SHMatrix& STDCALL GetMatrix() const { return matResult; }
	virtual const CVec3& STDCALL GetNormal() const 
	{ 
		return vDesiredNormale; 
	}
};
#endif // __MATRIXEFFECTORLEVELING_H__
