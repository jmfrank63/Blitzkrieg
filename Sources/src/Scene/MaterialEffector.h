#ifndef __MATERIALEFFECTOR_H__
#define __MATERIALEFFECTOR_H__
#pragma ONCE
class CMaterialEffector : ISceneMaterialEffector
{
	OBJECT_NORMAL_METHODS( CMaterialEffector );
	DECLARE_SERIALIZE;
	NTimer::STime nStartTime;
	NTimer::STime nDuration;
	BYTE bAlpha;
	DWORD dwSpecular;
	float fCoeff;
public:
	CMaterialEffector() : bAlpha( 0xFF ), dwSpecular( 0xFF000000 ), fCoeff( 0.0f ), nStartTime( 0 ), nDuration( 1 ) {  }
	virtual bool STDCALL Update( const NTimer::STime &time );
	virtual void STDCALL SetupTimes( const NTimer::STime &timeStart, const NTimer::STime &timeLife );
	virtual BYTE STDCALL GetAlpha() const;
	virtual DWORD STDCALL GetSpecular() const;
	virtual void STDCALL SetupData( BYTE bMaxAlpha, DWORD dwMaxSpecular );
};
#endif // __MATERIALEFFECTOR_H__
