#ifndef __VIDEOPLAYER_H__
#define __VIDEOPLAYER_H__
#pragma ONCE

class CVideoPlayer : public CTRefCount<IVideoPlayer>
{
	OBJECT_SERVICE_METHODS( CVideoPlayer );
	DECLARE_SERIALIZE;

	CPtr<IVideoPlayer> pPlayer;
	CPtr<IGFXTexture> pTargetTexture;
	IGFX *pTargetGFX;
	CTRect<float> rcDstRect;
	bool bHasTarget;
	bool bHasDstRect;
	bool bMaintainAspect;
	bool bLooped;
	int nShadingEffectStart;
	int nShadingEffectFinish;
	std::string szFileName;
	DWORD dwPlayFlags;

	void CreateBackend( const char *pszFileName );
	void ApplyState();
public:
	CVideoPlayer();
	virtual void STDCALL SetTarget( interface IGFXTexture *pTexture, interface IGFX *pGFX );
	virtual void STDCALL SetDstRect( const RECT &_rcDstRect, bool _bMaintainAspect );
	virtual void STDCALL SetLoopMode( bool _bLooped );
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

#endif // __VIDEOPLAYER_H__
