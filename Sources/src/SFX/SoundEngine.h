#ifndef __SOUNDENGINE_H__
#define __SOUNDENGINE_H__
#include "AudioBackend.h"
#include "SampleSounds.h"
#include "StreamFadeOff.h"
typedef std::unordered_map<ISound*, int, SDefaultPtrHash> CSoundChannelMap;
typedef std::unordered_map<int, CPtr<ISound> > CChannelSoundMap;
class CSoundEngine : public ISFX
{
	friend class CPlayVisitor;
	OBJECT_NORMAL_METHODS( CSoundEngine );
	DECLARE_SERIALIZE;
	typedef NAudioBackend::SDriverInfo SDriverInfo;
	struct SMelodyInfo
	{
		DECLARE_SERIALIZE;
	public:
		std::string szName;
		bool bLooped;
		void Clear() { szName.clear(); } 
		bool IsValid() const { return !szName.empty(); }
	};
	typedef std::vector<SDriverInfo> CDriversInfo;
	CDriversInfo drivers;									// [0] is default driver
	NTimer::STime timeLastUpdate;
	SMelodyInfo curMelody;
	SMelodyInfo nextMelody;								// to fade melodies
	void *pStreamingSound;								// current streaming sound
	int nStreamingChannel;								// channel of this streaming sound
	NTimer::STime timeStreamFinished;			// time, last stream finished
	CSoundChannelMap channelsMap;					// sound => channel map
	CChannelSoundMap soundsMap;						// channel => sound map
	float fListenerDistance;							// listener distance from anchor
	CVec3 vLastListenerPos;
	bool bInited;
	bool bEnableSFX;											// enable SFXes playing
	bool bEnableStreaming;								// enable streaming playing
	bool bSoundCardPresent;								
	bool bPaused;													// is all SFX sounds paused?
	bool bStreamingPaused;								// is streaming sound paused
	BYTE cSFXMasterVolume;								// SFXes volume
	BYTE cStreamMasterVolume;							// streams volume
	float fStreamCurrentVolume;						// for fade streams ( 0.0f ... 1.0f )
	bool bStreamPlaying;
	
	CStreamFadeOff streamFadeOff;
	void ClearChannels();
	bool SearchDevices();
	void CloseStreaming();
	void ReEnableSounds();
	CSoundEngine();
	virtual ~CSoundEngine() { Done(); }
	
	void UpdateCameraPos( const CVec3 &vPos );
	void Update3DChannel( class CSound3D *pSound, int nChannel );

public:
	bool PlayNextMelody();
	void NotifyMelodyFinished();
	void MapSound( ISound *pSound, int nChannel );
	virtual BYTE STDCALL GetSFXMasterVolume() const { return cSFXMasterVolume; }
	virtual BYTE STDCALL GetStreamMasterVolume() const { return cStreamMasterVolume; }
	virtual IRefCount* STDCALL QI( int nInterfaceTypeID );
	virtual bool STDCALL IsInitialized();
	virtual bool STDCALL Init( HWND hWnd, int nDriver, ESFXOutputType output, int nMixRate, int nMaxChannels );
	virtual void STDCALL Done();
	virtual void STDCALL EnableSFX( bool bEnable ) { bEnableSFX = bEnable; ReEnableSounds(); }
	virtual void STDCALL EnableStreaming( bool bEnable ) { bEnableStreaming = bEnable; ReEnableSounds(); }
	virtual bool STDCALL IsSFXEnabled()const { return bEnableSFX && bSoundCardPresent; }
	virtual bool STDCALL IsStreamingEnabled()const { return bEnableStreaming && bSoundCardPresent; }
	virtual void STDCALL SetDistanceFactor( float fFactor );
	virtual void STDCALL SetRolloffFactor( float fFactor );
	virtual void STDCALL SetSFXMasterVolume( float fVolume );
	virtual void STDCALL SetStreamMasterVolume( float fVolume );
	virtual void STDCALL PlayStream( const char *pszFileName, bool bLooped = false, const unsigned int nTimeToFadePrevious = 0 );
	virtual void STDCALL PlayVideoStream( const char *pszFileName, bool bLooped = false );
	virtual void STDCALL StopStream( const unsigned int nTimeToFade = 0 );
	virtual bool STDCALL IsStreamPlaying() const;
	virtual void STDCALL SetStreamVolume( const float fVolume );
	virtual float STDCALL GetStreamVolume() const;

	virtual int STDCALL PlaySample( ISound *pSound, bool bLooped = false, unsigned int nStartPos=0 );
	virtual void STDCALL StopSample( ISound *pSound );
	virtual void STDCALL UpdateSample( ISound *pSound );
	virtual void STDCALL StopChannel( int nChannel );

	virtual void STDCALL Update( interface ICamera *pCamera );
	virtual bool STDCALL Pause( bool bPause );
	virtual bool STDCALL PauseStreaming( bool bPause );
	virtual bool STDCALL IsPaused();
	virtual bool STDCALL IsPlaying( ISound *pSound );

	unsigned int STDCALL GetCurrentPosition( ISound * pSound );
	virtual void STDCALL SetCurrentPosition( ISound * pSound, unsigned int pos );

};
#endif // __SOUNDENGINE_H__
