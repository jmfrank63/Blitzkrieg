#ifndef __OPENVIDEOPLAYER_H__
#define __OPENVIDEOPLAYER_H__
#pragma ONCE

class COpenVideoPlayer : public CTRefCount<IVideoPlayer>
{
	OBJECT_SERVICE_METHODS( COpenVideoPlayer );
	DECLARE_SERIALIZE;

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

	bool ProbeOpenVideo( const char *pszFileName );
public:
	COpenVideoPlayer();
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
