#ifndef __PFX_H__
#define __PFX_H__
#pragma ONCE
#include "Scene.h"
struct SParticleSourceInfo
{
	float fMaxCount;											// пиковое к-во партиклов
	float fMaxSize;												// пиковый размер
	float fAverageSize;										// средний размер партиклов 
	float fAverageCount;									// среднее к-во партиклов
};
#pragma pack( 1 )
struct SPartColor
{
	union
	{
		DWORD color;
		struct
		{
			BYTE b;
			BYTE g;
			BYTE r;
			BYTE a;
		};
	};
	operator DWORD() const { return color; }
};
#pragma pack()
struct SSimpleParticle 
{
	CTRect<float> rcMaps;
	CVec3 vPosition;
	SPartColor color;
	float fSize;  
	float fAngle;  
};
interface IParticleSource : public IRefCount
{
	virtual interface IGFXTexture* STDCALL GetTexture() const = 0;
	virtual const int STDCALL GetNumParticles() const = 0;
	virtual void STDCALL FillParticleBuffer( SSimpleParticle *buff ) const = 0;
	virtual const CVec3 STDCALL GetPos() const = 0;
	virtual void STDCALL SetPos( const CVec3 &vPos ) = 0;
	virtual const CVec3 STDCALL GetDirection() const = 0;
	virtual void STDCALL SetDirection( const SHMatrix &mDir ) = 0;
	virtual void STDCALL SetScale( float _fScale ) = 0;
	virtual void STDCALL Update( const NTimer::STime &time ) = 0;
	virtual void STDCALL SetStartTime( const NTimer::STime &time ) = 0;
	virtual const NTimer::STime STDCALL GetStartTime() const = 0;
	virtual const NTimer::STime STDCALL GetEffectLifeTime() const = 0;
	virtual bool STDCALL IsFinished() const = 0;
	virtual float STDCALL GetArea() const = 0;
	virtual void STDCALL Stop() = 0;
	virtual int STDCALL GetOptimalUpdateTime() const = 0;
	virtual void STDCALL SetSuspendedState( bool bState ) = 0;
};
interface IParticleSourceWithInfo
{
	virtual void STDCALL GetInfo( SParticleSourceInfo &info ) = 0;
};
interface IParticleManager : public ISharedManager
{
	enum { tidTypeID = PFX_MANAGER };
	virtual IParticleSource* STDCALL GetKeyBasedSource( const char *pszName ) = 0;
	virtual IParticleSource*  STDCALL GetSmokinParticleSource( const char *pszName ) = 0;
	virtual void STDCALL SetQuality( const float fQuality ) = 0;
};
#endif // __PFX_H__
