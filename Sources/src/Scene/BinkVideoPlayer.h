#ifndef __BINKVIDEOPLAYER_H__
#define __BINKVIDEOPLAYER_H__
#pragma ONCE
#include <bink.h>
struct SImagePart
{
	CPtr<IGFXTexture> pTexture;					// real vid-mem texture
	CTRect<long> rcSrcRect;							// source rect to blit uncompressed bink from
	CTRect<long> rcDstRect;							// destination rect to blit uncompressed bink data to (from source rect)
	CTRect<float> rcMaps;								// mapping coords
	CTRect<float> rcRect;								// drawing rect
	SImagePart() : rcSrcRect( 0, 0, 0, 0 ), rcDstRect( 0, 0, 0, 0 ), rcMaps( 0, 0, 0, 0 ), rcRect( 0, 0, 0, 0 ) {  }
};
typedef std::vector<SImagePart> CImagesList;
class CBinkVideoPlayer : public CTRefCount<IVideoPlayer>
{
	OBJECT_SERVICE_METHODS( CBinkVideoPlayer );
	DECLARE_SERIALIZE;
	CImagesList images;										// all subimages
	CTRect<float> rcDstRect;							// destination rect to render to
	bool bMaintainAspect;									// do we need maintain aspect ratio?
	DWORD dwCopyFlags;										// copy flags
	DWORD dwPlayFlags;										// play flags
	HBINK hBink;													// bink main handler
	bool bLooped;													// is looped movie?
	int nLastPlayedFrame;									// last frame, which was played
	bool bStopped;												// bink playing stopped, but handle was not closed
	int nShadingEffectStart;							// before draw
	int nShadingEffectFinish;							// after draw
	std::vector<char> buffer;							// buffer to play bink from memory
	std::string szFileName;								// bink file name (to restore)
	bool OpenBink( const char *pszFileName, DWORD dwOpenFlags, DWORD dwFlags );
	bool DoOneFrame( bool bCheckForStop = true );
	void CopyRects();
	void SetupRects();
public:
	CBinkVideoPlayer();
	virtual ~CBinkVideoPlayer();
	virtual void STDCALL SetTarget( interface IGFXTexture *pTexture, IGFX *pGFX );
	virtual void STDCALL SetDstRect( const RECT &_rcDstRect, bool bMaintainAspect );
	virtual void STDCALL SetLoopMode( bool _bLooped ) { bLooped = _bLooped; }
	virtual int STDCALL GetCurrentFrame() const;
	virtual bool STDCALL SetCurrentFrame( const int nFrame );
	virtual void SetShadingEffect( const int nEffect, bool bStart )
	{
		if ( bStart ) 
			nShadingEffectStart = nEffect;
		else
			nShadingEffectFinish = nEffect;
	}
	virtual bool STDCALL Update( const NTimer::STime &time, bool bForcedUpdate );
	virtual int STDCALL Play( const char *pszFileName, DWORD dwFlags, IGFX *pGFX, interface ISFX *pSFX );
	virtual bool STDCALL Stop();
	virtual bool STDCALL Pause( bool bPause );
	virtual bool STDCALL IsPlaying() const;
	virtual int STDCALL GetLength() const;
	virtual int STDCALL GetNumFrames() const;
	virtual bool STDCALL GetMovieSize( CVec2 *pSize ) const;
	virtual bool STDCALL Draw( interface IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor, int nType = -1 );
};
#endif // __BINKVIDEOPLAYER_H__
