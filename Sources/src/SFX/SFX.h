#ifndef __SFX_H__
#define __SFX_H__
#pragma ONCE
enum
{
	SFX_BASE_VALUE		= 0x10090000,
	SFX_SFX						= SFX_BASE_VALUE + 1,
	SFX_SAMPLE				= SFX_BASE_VALUE + 2,
	SFX_SOUND_MANAGER = SFX_BASE_VALUE + 3,
	SFX_SOUND_2D			= SFX_BASE_VALUE + 4,
	SFX_SOUND_3D			= SFX_BASE_VALUE + 5,
	SFX_PLAY_LIST			= SFX_BASE_VALUE + 6
};
enum ESFXOutputType
{
	SFX_OUTPUT_NO,
	SFX_OUTPUT_WINMM,
	SFX_OUTPUT_DSOUND,
	SFX_OUTPUT_A3D
};
interface ISound : public IRefCount
{
	virtual int STDCALL Visit( interface ISFXVisitor *pVisitor ) = 0;
	virtual void STDCALL SetPosition( const CVec3 &vPos3 ) = 0;
	virtual const CVec3 STDCALL GetPosition() = 0;
	virtual void STDCALL SetMinDistance( float fDistance ) = 0;
	virtual void STDCALL SetLooping( bool bEnable, int nStart = -1, int nEnd = -1 ) = 0;
	
	virtual void STDCALL SetVolume( float nVolume )=0;
	virtual float STDCALL GetVolume()const=0;
	
	virtual void STDCALL SetPan( float nPan )=0;
	virtual float STDCALL GetPan() const=0;
	
	virtual unsigned int STDCALL GetLenght()=0;
	virtual unsigned int STDCALL GetSampleRate()=0;
	
};
interface IPlayList : public IRefCount
{
	enum { ORDER_SEQUENTIAL, ORDER_RANDOM, ORDER_CYCLE };
	virtual void STDCALL Clear() = 0;
	virtual void STDCALL SetSequenceOrder( int nOrder ) = 0;
	virtual void STDCALL AddMelody( const char *pszFileName ) = 0;
	virtual const char* STDCALL GetNextMelody() = 0;
};
interface ISoundManager : public ISharedManager
{
	enum { tidTypeID = SFX_SOUND_MANAGER };
	virtual ISound* STDCALL GetSound2D( const char *pszName ) = 0;
	virtual ISound* STDCALL GetSound3D( const char *pszName ) = 0;
	virtual const char* STDCALL GetSoundName( ISound *pSound ) = 0;
};
interface ISFX : public IRefCount
{
	enum { tidTypeID = SFX_SFX };
	virtual IRefCount* STDCALL QI( int nInterfaceTypeID ) = 0;
	virtual bool STDCALL IsInitialized() = 0;
	virtual bool STDCALL Init( HWND hWnd, int nDriver, ESFXOutputType output, int nMixRate, int nMaxChannels ) = 0;
	virtual void STDCALL Done() = 0;
	virtual void STDCALL EnableSFX( bool bEnable ) = 0;
	virtual void STDCALL EnableStreaming( bool bEnable ) = 0;
	virtual bool STDCALL IsSFXEnabled()const=0;
	virtual bool STDCALL IsStreamingEnabled()const=0;
	virtual void STDCALL SetDistanceFactor( float fFactor ) = 0;
	virtual void STDCALL SetRolloffFactor( float fFactor ) = 0;
	virtual void STDCALL SetSFXMasterVolume( float fVolume ) = 0;
	virtual BYTE STDCALL GetSFXMasterVolume() const = 0;
	virtual void STDCALL SetStreamMasterVolume( float fVolume ) = 0;
	virtual void STDCALL PlayStream( const char *pszFileName, bool bLooped = false, const unsigned int nTimeToFadePrevious = 0 ) = 0;
	virtual void STDCALL StopStream( const unsigned int nTimeToFade = 0 ) = 0;
	virtual bool STDCALL IsStreamPlaying() const =0;
	virtual void STDCALL SetStreamVolume( const float fVolume ) = 0;
	virtual float STDCALL GetStreamVolume() const = 0;
	virtual int STDCALL PlaySample( ISound *pSound, bool bLooped = false, unsigned int nStartPos = 0 ) = 0;
	virtual void STDCALL UpdateSample( ISound *pSound ) = 0;
	virtual void STDCALL StopSample( ISound *pSound ) = 0;
	virtual void STDCALL StopChannel( int nChannel ) = 0;
	virtual void STDCALL Update( interface ICamera *pCamera ) = 0;
	virtual bool STDCALL Pause( bool bPause ) = 0;
	virtual bool STDCALL PauseStreaming( bool bPause ) = 0;
	virtual bool STDCALL IsPaused() = 0;
	
	virtual bool STDCALL IsPlaying( ISound *pSound )=0;

	virtual unsigned int STDCALL GetCurrentPosition( ISound * pSound )=0;
	virtual void STDCALL SetCurrentPosition( ISound * pSound, unsigned int pos )=0;
};
#endif // __SFX_H__
