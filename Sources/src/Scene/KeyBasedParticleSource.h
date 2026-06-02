#ifndef __KEYBASEDPARTICLESOURCE_H__
#define __KEYBASEDPARTICLESOURCE_H__
#pragma ONCE
#include "PFX.h"
#include "ParticleSourceData.h"
class CKeyBasedParticleSource : public IParticleSource, public IParticleSourceWithInfo
{
	OBJECT_NORMAL_METHODS(CKeyBasedParticleSource);
	DECLARE_SERIALIZE;
	CPtr<SParticleSourceData> pData;			
	NTimer::STime nStartTime;            
	NTimer::STime nLastUpdateTime;				
	NTimer::STime nLastParticleUpdate;   
	CVec3 vPosition;											
	float fDirectionPhi;                  
	float fDirectionTheta;               
	CVec3 vDirection;                     
	CPtr<IGFXTexture> pTexture;           
	float lastError;                      
	std::vector< CTRect<float> > rcRects;  
	std::list<SExtendedParticle> particles; 
	float fScale;                         
	bool bStopped;                        
	bool bSuspended;               
	STrackContext contextDensity;         
	typedef CVec3 GetParticlePositionFunction( const float area, const CVec3 &vPosition );
	GetParticlePositionFunction *pfnGPPfunc; 
public:
	virtual interface IGFXTexture* STDCALL GetTexture() const;
	virtual const int STDCALL GetNumParticles() const;
	virtual void STDCALL FillParticleBuffer( SSimpleParticle *buff ) const;
	virtual const CVec3 STDCALL GetPos() const;
	virtual void STDCALL SetPos( const CVec3 &vPos );
	virtual const CVec3 STDCALL GetDirection() const;
	virtual void STDCALL SetScale( float _fScale );
	virtual void STDCALL SetDirection( const SHMatrix &mDir );
	virtual void STDCALL Update( const NTimer::STime &time );
	virtual void STDCALL SetStartTime( const NTimer::STime &time );
	virtual const NTimer::STime STDCALL GetStartTime() const;
	virtual const NTimer::STime STDCALL GetEffectLifeTime() const;
	virtual bool STDCALL IsFinished() const;
	virtual void STDCALL GetInfo( SParticleSourceInfo &info );
	virtual float STDCALL GetArea() const;
	virtual void STDCALL Stop();
	virtual void Init( SParticleSourceData *_pData );
	virtual int STDCALL GetOptimalUpdateTime() const;
	virtual void STDCALL SetSuspendedState( bool bState );
};
class CParticleGenerator
{
	static float fStartAngle;
	static float nCurrParticle;
	static float fStep;
public:
	static CVec3 GetParticlePositionSquare( const float area, const CVec3 &vPosition );
	static CVec3 GetParticlePositionDisk( const float area, const CVec3 &vPosition );
	static CVec3 GetParticlePositionCircle( const float area, const CVec3 &vPosition );
	static void ResetGenerator( int nNextNumParticles );
};
#endif // __KEYBASEDPARTICLESOURCE_H__
