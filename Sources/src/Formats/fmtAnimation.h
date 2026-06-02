#ifndef __FMTSPRITEANIMATION_H__
#define __FMTSPRITEANIMATION_H__
#pragma ONCE
struct SSpriteAnimationFormat : public ISharedResource
{
private:
	OBJECT_NORMAL_METHODS( SSpriteAnimationFormat );
	DECLARE_SERIALIZE;
	SHARED_RESOURCE_METHODS( nRefData.a, "Sprite.Animation" );
public:
	struct SSpriteAnimation
	{
		struct SDir
		{
			std::vector<short> frames;				// direction frame indices from the 'rects' array
			int operator&( interface IStructureSaver &ss );
		};
		std::vector<SSpriteRect> rects;			// each sprite (frame) descriptions
		std::vector<SDir> dirs;							// directions
		int nFrameTime;											// one frame show time
		float fSpeed;												// <translation speed>
		bool bCycled;												// cycled or one-shot animation
		int operator&( interface IStructureSaver &ss );
		const SSpriteRect& GetRect( const int nDirection, const int nTime ) const
		{
			const int nNumDirs = dirs.size();
			NI_ASSERT_SLOW_TF( nNumDirs != 0, "Number of directions == 0 in sprite animation data", return rects[0] );
			const int nDir = ( ( ( nDirection + (32768 / nNumDirs) ) & 0xffff ) / (65536 / nNumDirs) ) % nNumDirs;
			const SDir &dir = dirs[nDir];
			int nFrame;
			if ( bCycled )
				nFrame = ( nTime / nFrameTime ) % dir.frames.size();
			else
			{
				if ( nTime < nFrameTime * dir.frames.size() )
					nFrame = nTime / nFrameTime;
				else
					nFrame = dir.frames.size() - 1;
			}
			return rects[ dir.frames[nFrame] ];
		}
		const SSpriteRect& GetRect( int nIndex ) const { return rects[ dirs[0].frames[nIndex] ]; }
		const int GetLength() const { return nFrameTime * dirs[0].frames.size(); }
	};
	typedef std::vector<SSpriteAnimation> CAnimations;
	CAnimations animations;
	const SSpriteAnimation* GetAnimation( int nAnim ) const
	{
		NI_ASSERT_SLOW_TF( nAnim < animations.size(), NStr::Format("Can't find animation %d for \"%s\"", nAnim, GetSharedResourceName()), return 0 );
		return nAnim < animations.size() ? &( animations[nAnim] ) : 0;
	}
	virtual void STDCALL SwapData( ISharedResource *pResource )
	{
		SSpriteAnimationFormat *pRes = dynamic_cast<SSpriteAnimationFormat*>( pResource );
		NI_ASSERT_TF( pRes != 0, "shared resource is not a SSpriteAnimationFormat", return );
		CAnimations temp = animations;
		animations = pRes->animations;
		pRes->animations = temp;
	}
	virtual void STDCALL ClearInternalContainer() {  }
	virtual bool STDCALL Load( const bool bPreLoad = false );
};
#endif // __FMTSPRITEANIMATION_H__
