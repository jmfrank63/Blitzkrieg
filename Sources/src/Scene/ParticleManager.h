#ifndef __PARTICLEMANAGER_H__
#define __PARTICLEMANAGER_H__
#pragma ONCE
#include "../Misc/BasicShare.h"
#include "ParticleSourceData.h"
#include "SmokinParticleSourceData.h"
BASIC_SHARE_DECLARE( CParticleDataShare      , std::string, SParticleSourceData      , PFX_KEYDATA        , 107, "" );
BASIC_SHARE_DECLARE( CSmokinParticleDataShare, std::string, SSmokinParticleSourceData, PFX_COMPLEX_KEYDATA, 108, "" );
class CParticleDataManager : public IParticleManager
{
	OBJECT_COMPLETE_METHODS( CParticleDataManager );
	DECLARE_SERIALIZE;
	CParticleDataShare shareKeyBased;
	CSmokinParticleDataShare shareSmokin;
public:
	void STDCALL SetSerialMode( ESharedDataSerialMode eSerialMode ) 
	{ 
		shareKeyBased.SetSerialMode( eSerialMode ); 
		shareSmokin.SetSerialMode( eSerialMode );
	}
	void STDCALL SetShareMode( ESharedDataSharingMode eShareMode ) 
	{ 
		shareKeyBased.SetShareMode( eShareMode ); 
		shareSmokin.SetShareMode( eShareMode ); 
	}
	void STDCALL Clear( const ISharedManager::EClearMode eMode, const int nUsage, const int nAmount );
	bool STDCALL Init() { return shareKeyBased.Init() && shareSmokin.Init(); }
	IParticleSource*  STDCALL GetKeyBasedSource( const char *pszName );
	IParticleSource*  STDCALL GetSmokinParticleSource( const char *pszName );
	void STDCALL SetQuality( const float fQuality );
};
#endif
