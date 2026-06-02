#ifndef __MATRIXEFFECTORJOGGING_H__
#define __MATRIXEFFECTORJOGGING_H__
class CMatrixEffectorJogging : public IMatrixEffectorJogging
{
	OBJECT_NORMAL_METHODS( CMatrixEffectorJogging );
	DECLARE_SERIALIZE;
	struct SJogging
	{
		float fPeriod1, fPeriod2;
		float fAmp1, fAmp2;
		float fPhase1, fPhase2;
		int operator&( IStructureSaver &ss );
		float GetValue( const float fTime ) const
		{
			if ( (fPeriod1 == 0) || (fPeriod2 == 0) )
				return 0;
			else
				return fAmp1 * cos( fTime*FP_2PI/fPeriod1 + fPhase1 ) + fAmp2 * cos( fTime*FP_2PI/fPeriod2 + fPhase2 );
		}
	};
	SHMatrix matResult;										// result matrix
	NTimer::STime timeStart;							// effect start time
	NTimer::STime lastUpdateTime;					// last update time
	SJogging jx, jy, jz;
	CMatrixEffectorJogging();
public:
	virtual void STDCALL SetupTimes( const NTimer::STime &_timeStart, const NTimer::STime &_timeLife )
	{
		timeStart = _timeStart;
	}
	virtual void STDCALL SetupData( float fPeriodX1, float fPeriodX2, float fAmpX1, float fAmpX2, float fPhaseX1, float fPhaseX2,
		                              float fPeriodY1, float fPeriodY2, float fAmpY1, float fAmpY2, float fPhaseY1, float fPhaseY2,
																	float fPeriodZ1, float fPeriodZ2, float fAmpZ1, float fAmpZ2, float fPhaseZ1, float fPhaseZ2 );
	virtual bool STDCALL Update( const NTimer::STime &time );
	virtual const SHMatrix& STDCALL GetMatrix() const { return matResult; }
};
#endif // __MATRIXEFFECTORJOGGING_H__
