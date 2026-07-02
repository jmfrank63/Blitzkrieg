#ifndef __OPENVIDEOPLAYER_H__
#define __OPENVIDEOPLAYER_H__
#pragma ONCE

struct SOpenVideoImagePart
{
	CPtr<IGFXTexture> pTexture;
	CTRect<long> rcSrcRect;
	CTRect<long> rcDstRect;
	CTRect<float> rcMaps;
	CTRect<float> rcRect;
	SOpenVideoImagePart() : rcSrcRect( 0, 0, 0, 0 ), rcDstRect( 0, 0, 0, 0 ), rcMaps( 0, 0, 0, 0 ), rcRect( 0, 0, 0, 0 ) {  }
};
typedef std::vector<SOpenVideoImagePart> COpenVideoImagesList;

struct SOpenVideoDecoderState;

class COpenVideoPlayer : public CTRefCount<IVideoPlayer>
{
	OBJECT_SERVICE_METHODS( COpenVideoPlayer );
	DECLARE_SERIALIZE;

	COpenVideoImagesList images;
	CTRect<float> rcDstRect;
	bool bMaintainAspect;
	bool bLooped;
	int nShadingEffectStart;
	int nShadingEffectFinish;
	std::string szFileName;
	DWORD dwPlayFlags;
	bool bHasMovieInfo;
	CVec2 vMovieSize;
	int nFrameRateNumerator;
	int nFrameRateDenominator;
	int nGranuleShift;
	int nMovieLength;
	int nNumFrames;
	DWORD dwStartTime;
	bool bPlaying;
	bool bPaused;
	CPtr<IGFX> pRenderGFX;
	CPtr<ISFX> pVideoSFX;
	SOpenVideoDecoderState *pDecoderState;
	int nDecodedFrame;
	std::string szAudioStreamName;
	bool bAudioStreamPlaying;

	bool ProbeOpenVideo( const char *pszFileName );
	bool DecodeFirstFrame( const char *pszFileName, interface IGFX *pGFX );
	bool OpenDecoder( const char *pszFileName, interface IGFX *pGFX );
	bool DecodeNextFrame();
	void DestroyDecoder();
	bool FindOpenVideoAudioStreamName( const char *pszFileName, std::string *pAudioStreamName ) const;
	void PlayVideoAudioStream( interface ISFX *pSFX );
	void StopVideoAudioStream();
	void SetupRects();
public:
	COpenVideoPlayer();
	virtual ~COpenVideoPlayer();
	virtual void STDCALL SetTarget( interface IGFXTexture *pTexture, interface IGFX *pGFX );
	virtual void STDCALL SetDstRect( const RECT &_rcDstRect, bool _bMaintainAspect );
	virtual void STDCALL SetLoopMode( bool _bLooped ) { bLooped = _bLooped; }
	virtual int STDCALL GetCurrentFrame() const;
	virtual bool STDCALL SetCurrentFrame( const int nFrame );
	virtual void SetShadingEffect( const int nEffect, bool bStart );
	virtual bool STDCALL Update( const NTimer::STime &time, bool bForcedUpdate );
	virtual int STDCALL Play( const char *pszFileName, DWORD dwFlags, interface IGFX *pGFX, interface ISFX *pSFX );
	virtual bool STDCALL Stop();
	virtual bool STDCALL Pause( bool bPause );
	virtual bool STDCALL IsPlaying() const;
	virtual int STDCALL GetLength() const;
	virtual int STDCALL GetNumFrames() const;
	virtual bool STDCALL GetMovieSize( CVec2 *pSize ) const;
	virtual bool STDCALL Draw( interface IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor, int nType = -1 );
};

#endif // __OPENVIDEOPLAYER_H__
