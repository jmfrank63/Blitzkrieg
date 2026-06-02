#ifndef __SOUNDMANAGER_H__
#define __SOUNDMANAGER_H__
#include "..\Misc\BasicShare.h"
#include "SampleSounds.h"
class CSoundShare : public CBasicShare<std::string, CSoundSample, SFX_SAMPLE>
{
	bool b3D;
	float fDefaultMinDistance;
public:
	CSoundShare( int _nID, bool _b3D ) 
		: CBasicShare<std::string, CSoundSample, SFX_SAMPLE>( _nID, ".wav" ), b3D( _b3D ), fDefaultMinDistance( 45 ) {  }
	void SetMinDistance( float fMinDistance ) { fDefaultMinDistance = fMinDistance; }
};
class CSoundManager : public ISoundManager
{
	OBJECT_NORMAL_METHODS( CSoundManager );
	DECLARE_SERIALIZE;
	CSoundShare share2;										// 2D sounds share
	CSoundShare share3;										// 3D sounds share
	CObj<ISFX> pSFX;											// lock для звуковой системы
public:	
	CSoundManager() : share2( 109, false ), share3( 110, true ) {  }
	virtual ~CSoundManager();
	virtual void STDCALL Clear( const ISharedManager::EClearMode eMode, const int nUsage, const int nAmount );
	virtual void STDCALL SetSerialMode( ESharedDataSerialMode eSerialMode ) 
	{ 
		share2.SetSerialMode( eSerialMode ); 
		share3.SetSerialMode( eSerialMode ); 
	}
	virtual void STDCALL SetShareMode( ESharedDataSharingMode eShareMode ) 
	{ 
		share2.SetShareMode( eShareMode ); 
		share3.SetShareMode( eShareMode ); 
	}
	virtual bool STDCALL Init();
	CSoundSample* GetSample2D( const char *pszName ) { return share2.Get( pszName ); }
	CSoundSample* GetSample3D( const char *pszName ) { return share3.Get( pszName ); }
	virtual ISound* STDCALL GetSound2D( const char *pszName );
	virtual ISound* STDCALL GetSound3D( const char *pszName );
	virtual const char* STDCALL GetSoundName( ISound *pSound );
};
#endif // __SOUNDMANAGER_H__
