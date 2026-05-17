#ifndef __SOUNDENGINE_H__
#define __SOUNDENGINE_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "SampleSounds.h"
#include "StreamFadeOff.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::unordered_map<ISound*, int, SDefaultPtrHash> CSoundChannelMap;
typedef std::unordered_map<int, CPtr<ISound> > CChannelSoundMap;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSoundEngine : public ISFX
{
	OBJECT_NORMAL_METHODS( CSoundEngine );
	DECLARE_SERIALIZE;
	//
	struct SDriverInfo
	{
		std::string szDriverName;
		bool isHardware3DAccelerated;				// this driver supports hardware accelerated 3d sound.
		bool supportEAXReverb;							// this driver supports EAX reverb
		bool supportA3DOcclusions;					// this driver supports (A3D) geometry occlusions
		bool supportA3DReflections;					// this driver supports (A3D) geometry reflections
		bool supportReverb;									// this driver supports EAX2/A3D3 reverb  
	};
	struct SMelodyInfo
	{
		DECLARE_SERIALIZE;
	public:
		std::string szName;
		bool bLooped;
		void Clear() { szName.clear(); } 
		bool IsValid() const { return !szName.empty(); }
		//
	};
	// initialization info - drivers
	typedef std::vector<SDriverInfo> CDriversInfo;
	CDriversInfo drivers;									// [0] is default driver
	//
	NTimer::STime timeLastUpdate;
	//
	// streams
	SMelodyInfo curMelody;
	SMelodyInfo nextMelody;								// to fade melodies
	FSOUND_STREAM *pStreamingSound;				// current streaming sound
	int nStreamingChannel;								// channel of this streaming sound
	NTimer::STime timeStreamFinished;			// time, last stream finished
	//
	// channels management
	CSoundChannelMap channelsMap;					// sound => channel map
	CChannelSoundMap soundsMap;						// channel => sound map
	//
	float fListenerDistance;							// listener distance from anchor
	CVec3 vLastListenerPos;
	bool bInited;
	bool bEnableSFX;											// enable SFXes playing
	bool bEnableStreaming;								// enable streaming playing
	bool bSoundCardPresent;								
	bool bPaused;													// is all SFX sounds paused?
	bool bStreamingPaused;								// is streaming sound paused
	//
	BYTE cSFXMasterVolume;								// SFXes volume
	BYTE cStreamMasterVolume;							// streams volume
	float fStreamCurrentVolume;						// for fade streams ( 0.0f ... 1.0f )
	bool bStreamPlaying;
	
	CStreamFadeOff streamFadeOff;
	//
	void ClearChannels();
	//
	bool SearchDevices();
	// streaming
	void CloseStreaming();
	//
	void ReEnableSounds();
	//
	CSoundEngine();
	virtual ~CSoundEngine() { Done(); }
	
	void UpdateCameraPos( const CVec3 &vPos );

public:
	// internal-use service functions
	bool PlayNextMelody();
	void NotifyMelodyFinished();
	void MapSound( ISound *pSound, int nChannel );
	//
	virtual BYTE STDCALL GetSFXMasterVolume() const { return cSFXMasterVolume; }
	virtual BYTE STDCALL GetStreamMasterVolume() const { return cStreamMasterVolume; }
	//
	virtual IRefCount* STDCALL QI( int nInterfaceTypeID );
	// init and close sound system
	virtual bool STDCALL IsInitialized();
	virtual bool STDCALL Init( HWND hWnd, int nDriver, ESFXOutputType output, int nMixRate, int nMaxChannels );
	virtual void STDCALL Done();
	//
	// enable SFXes and streaming
	virtual void STDCALL EnableSFX( bool bEnable ) { bEnableSFX = bEnable; ReEnableSounds(); }
	virtual void STDCALL EnableStreaming( bool bEnable ) { bEnableStreaming = bEnable; ReEnableSounds(); }
	virtual bool STDCALL IsSFXEnabled()const { return bEnableSFX && bSoundCardPresent; }
	virtual bool STDCALL IsStreamingEnabled()const { return bEnableStreaming && bSoundCardPresent; }
	//
	// setup
	virtual void STDCALL SetDistanceFactor( float fFactor );
	virtual void STDCALL SetRolloffFactor( float fFactor );
	// set SFX master volume. valid range [0..1]
	virtual void STDCALL SetSFXMasterVolume( float fVolume )
	{
		Clamp( fVolume, 0.0f, 1.0f );
		cSFXMasterVolume = BYTE( fVolume * 255.0f );
	}
	// set streams master volume. valid range [0..1]
	virtual void STDCALL SetStreamMasterVolume( float fVolume );
	//
	// streaming sound
	virtual void STDCALL PlayStream( const char *pszFileName, bool bLooped = false, const unsigned int nTimeToFadePrevious = 0 );
	virtual void STDCALL StopStream( const unsigned int nTimeToFade = 0 );
	virtual bool STDCALL IsStreamPlaying() const;
	virtual void STDCALL SetStreamVolume( const float fVolume );
	virtual float STDCALL GetStreamVolume() const;

	//
	// sample sounds
	virtual int STDCALL PlaySample( ISound *pSound, bool bLooped = false, unsigned int nStartPos=0 );
	virtual void STDCALL StopSample( ISound *pSound );
	virtual void STDCALL UpdateSample( ISound *pSound );
	virtual void STDCALL StopChannel( int nChannel );

	// Update sounds ( that is needed for 3D sounds )
	virtual void STDCALL Update( interface ICamera *pCamera );
	//
	virtual bool STDCALL Pause( bool bPause );
	virtual bool STDCALL PauseStreaming( bool bPause );
	virtual bool STDCALL IsPaused();
	virtual bool STDCALL IsPlaying( ISound *pSound );

	unsigned int STDCALL GetCurrentPosition( ISound * pSound );
	virtual void STDCALL SetCurrentPosition( ISound * pSound, unsigned int pos );

};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SOUNDENGINE_H__
