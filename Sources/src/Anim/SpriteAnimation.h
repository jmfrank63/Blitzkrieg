#ifndef __SPRITEANIMATION_H__
#define __SPRITEANIMATION_H__
class CSpriteAnimation : public ISpriteAnimation
{
	OBJECT_COMPLETE_METHODS( CSpriteAnimation );
	DECLARE_SERIALIZE;
	CPtr<SSpriteAnimationFormat> pAnimations; // all animations
	const SSpriteAnimationFormat::SSpriteAnimation *pAnimation; // current animation shortcut
	int nCurrAnim;												// current animation
	CScaleTimer timer;										// timer...
	std::vector<float> timescales;				// time scaling coeffs for each animation
	int nCurrDirection;										// current direction
	float fScale;													// scaling coeff
	SSpriteRect rect;											// temporal storage for return value
	int nFrameIndex;											// index for static non-animated frames
	bool ReAcquireAnimation()
	{
		if ( (pAnimations != 0) && (nCurrAnim != -1) )
		{
			pAnimation = pAnimations->GetAnimation( nCurrAnim );
			if ( pAnimation == 0 )
			{
				nCurrAnim = -1;
				return false;
			}
			else if ( pAnimation->dirs.empty() ) 
			{
				pAnimation = 0;
				nCurrAnim = -1;
				return false;
			}
			else
			{
				timer.SetScale( timescales[nCurrAnim] );
				return true;
			}
		}
		else
			return false;
	}
public:
	CSpriteAnimation()
		: pAnimations( 0 ), pAnimation( 0 ), nCurrAnim( -1 ), nCurrDirection( 0 ), fScale( 1 ), nFrameIndex( -1 ) {  }
	bool Init( SSpriteAnimationFormat *_pAnimations );
	virtual void STDCALL Visit( IAnimVisitor *pVisitor );
	virtual void STDCALL SetTime( DWORD time ) 
	{ 
		timer.Update( time ); 
	}
	virtual void STDCALL SetStartTime( DWORD time ) 
	{ 
		timer.Reset( time ); 
	}
	virtual void STDCALL SetAnimSpeedCoeff( float fCoeff ) 
	{ 
		if ( (nCurrAnim == ANIMATION_IDLE) || (nCurrAnim == ANIMATION_MOVE) || (nCurrAnim == ANIMATION_CRAWL) ) 
		{
			timer.SetScale( fCoeff ); 
			if ( nCurrAnim != -1 ) 
				timescales[nCurrAnim] = fCoeff; 
		}
	}
	virtual bool STDCALL SetAnimation( const int nAnim ) { nCurrAnim = nAnim; return ReAcquireAnimation(); }
	virtual int STDCALL GetAnimation() const { return nCurrAnim; };
	virtual int STDCALL GetLengthOf( const int nAnim )
	{
		if ( pAnimations )
		{
			const SSpriteAnimationFormat::SSpriteAnimation *pTempAnim = pAnimations->GetAnimation( nAnim );
			NI_ASSERT_SLOW_TF( pTempAnim != 0, NStr::Format("Can't get animation %d for the length check for \"%s\"", nAnim, pAnimations->GetSharedResourceName()), return 1 );
			return pTempAnim->GetLength();
		}
		else
			return 1;
	}
	virtual void STDCALL SetScale( float _fScale ) { fScale = _fScale; }
	virtual void STDCALL SetDirection( int nDirection ) { nCurrDirection = nDirection; }
	virtual const SSpriteRect& STDCALL GetRect()
	{
		if ( (pAnimation == 0) && !ReAcquireAnimation() )
			return rect;
		else if ( nFrameIndex == -1 )
			rect = pAnimation->GetRect( nCurrDirection, timer.Get() );
		else
			rect = pAnimation->GetRect( nFrameIndex );
		rect.rect *= fScale;
		return rect;
	}
	virtual float STDCALL GetSpeed() const { return pAnimation ? pAnimation->fSpeed : 1; }
	virtual void STDCALL SetFrameIndex( int nIndex ) { nFrameIndex = nIndex; }
	virtual int STDCALL GetFrameIndex() { return nFrameIndex; }
	virtual const bool STDCALL IsHit( const CVec3 &relpos, const CVec2 &point, CVec2 *pShift ) const;
	virtual const bool STDCALL IsHit( const CVec3 &relpos, const CTRect<float> &rcRect ) const;
	virtual const CScaleTimer& STDCALL GetScaleTimer() const { return timer; }
	virtual void STDCALL SetScaleTimer( const CScaleTimer &_timer ) { timer = _timer; }
};
class CComplexSprite : public CTRefCount<ISpriteAnimation>
{
	OBJECT_SERVICE_METHODS( CComplexSprite );
	DECLARE_SERIALIZE;
	CPtr<SSpritesPack> pSprites;
	CScaleTimer timer;										// timer...
	int nFrameIndex;											// current frame index (if it is)
	SSpriteRect spriteRect;								// temp sprite rect
	int GetSafeFrameIndex() const
	{
		if ( nFrameIndex == -1 )
			return 0;
		if ( pSprites == 0 || pSprites->sprites.empty() )
			return nFrameIndex;
		return Clamp( nFrameIndex, 0, int(pSprites->sprites.size()) - 1 );
	}
	const SSpritesPack::SSprite& GetSprite() const { return pSprites->sprites[GetSafeFrameIndex()]; }
public:
	bool Init( SSpritesPack *_pSprites ) { pSprites = _pSprites; nFrameIndex = 0; return true; }
	virtual void STDCALL Visit( IAnimVisitor *pVisitor );
	virtual void STDCALL SetTime( DWORD time ) { timer.Update( time ); }
	virtual void STDCALL SetStartTime( DWORD time ) { timer.Reset( time ); }
	virtual void STDCALL SetAnimSpeedCoeff( float fCoeff ) { timer.SetScale( fCoeff ); }
	virtual bool STDCALL SetAnimation( const int nAnim ) { return true; }
	virtual int STDCALL GetAnimation() const { return 0; };
	virtual int STDCALL GetLengthOf( const int nAnim ) { return 1; }
	virtual void STDCALL SetScale( float _fScale ) {  }
	virtual void STDCALL SetDirection( int nDirection ) {  }
	virtual const SSpriteRect& STDCALL GetRect()
	{
		const int nLocalFrameIndex = GetSafeFrameIndex();
		const CTRect<float> &rcBoundBox = pSprites->sprites[nLocalFrameIndex].GetBoundBox();
		spriteRect.rect.Set( rcBoundBox.x1, rcBoundBox.y1, rcBoundBox.x2, rcBoundBox.y2 );
		spriteRect.fDepthLeft = spriteRect.fDepthRight = pSprites->sprites[nLocalFrameIndex].fMinDepth;
		return spriteRect;
	}
	virtual float STDCALL GetSpeed() const { return 1; }
	virtual void STDCALL SetFrameIndex( int nIndex )
	{
		nFrameIndex = nIndex;
		if ( nIndex != -1 && pSprites != 0 && !pSprites->sprites.empty() )
			nFrameIndex = Clamp( nIndex, 0, int(pSprites->sprites.size()) - 1 );
	}
	virtual int STDCALL GetFrameIndex() { return nFrameIndex; }
	virtual const bool STDCALL IsHit( const CVec3 &relpos, const CVec2 &point, CVec2 *pShift ) const;
	virtual const bool STDCALL IsHit( const CVec3 &relpos, const CTRect<float> &rcRect ) const;
	virtual const CScaleTimer& STDCALL GetScaleTimer() const { return timer; }
	virtual void STDCALL SetScaleTimer( const CScaleTimer &_timer ) { timer = _timer; }
};
#endif // __SPRITEANIMATION_H__
