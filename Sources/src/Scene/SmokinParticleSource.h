#ifndef __SMOKINPARTICLESOURCE_H__
#define __SMOKINPARTICLESOURCE_H__
#pragma ONCE
#include "PFX.h"
#include "SmokinParticleSourceData.h"
#include "KeyBasedParticleSource.h"
struct SExtendedParticleSource
{
	CPtr<IParticleSource> pSource;        // то, что летит
	CVec3 vSpeed;                         // просто скорость
	CVec3 vWind;                          // ветер для частицы
	STrackContext contextSpeed;						// контексты для расчета интегралов
	STrackContext contextZSpeed;          // 
	int operator&( IStructureSaver &ss );
};
class CSmokinParticleSource : public IParticleSource, public IParticleSourceWithInfo
{
	OBJECT_NORMAL_METHODS(CSmokinParticleSource);
	DECLARE_SERIALIZE;
	CPtr<SSmokinParticleSourceData> pData;			// данные об источнике
	NTimer::STime nStartTime;             // время рождения
	NTimer::STime nLastUpdateTime;				// время последнего обновления
	CVec3 vPosition;											// относительно положение источника
	float fDirectionPhi;                   // направление источника в сферической системе координат
	float fDirectionTheta;                 // направление источника в сферической системе координат
	CVec3 vDirection;                     // направление источника без учета pData
	float lastError;                      // поправка на нецелое число партиклов при последней генерации
	std::list<SExtendedParticleSource> particles; // они
	float fScale;                         // масштаб эффекта
	bool bStopped;                        // остановка эффекта
	STrackContext contextDensity;         // контекст для интегрирования плотности партиклов
	typedef CVec3 GetParticlePositionFunction( const float area, const CVec3 &vPosition );
	GetParticlePositionFunction *pfnGPPfunc;
public:
	virtual interface IGFXTexture* STDCALL GetTexture() const;
	virtual const int STDCALL GetNumParticles() const;
	virtual void STDCALL FillParticleBuffer( SSimpleParticle *buff ) const;
	virtual const CVec3 STDCALL GetPos() const;
	virtual void STDCALL SetPos( const CVec3 &vPos );
	virtual const CVec3 STDCALL GetDirection() const;
	virtual void STDCALL SetDirection( const SHMatrix &mDir );
	virtual void STDCALL SetScale( float _fScale );
	virtual void STDCALL Update( const NTimer::STime &time );
	virtual void STDCALL SetStartTime( const NTimer::STime &time );
	virtual const NTimer::STime STDCALL GetStartTime() const;
	virtual const NTimer::STime STDCALL GetEffectLifeTime() const;
	virtual bool STDCALL IsFinished() const;
	virtual void STDCALL GetInfo( SParticleSourceInfo &info );
	virtual float STDCALL GetArea() const;
	virtual void STDCALL Stop();
	virtual void Init( SSmokinParticleSourceData *_pData );
	virtual int STDCALL GetOptimalUpdateTime() const;
	virtual void STDCALL SetSuspendedState( bool bState );
};
#endif 
