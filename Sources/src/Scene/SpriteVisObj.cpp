#include "StdAfx.h"

#include "SpriteVisObj.h"
#include "AnimVisitor.h"
#include "SceneScreenScale.h"
#include "../Misc/Win32Random.h"
#include "../Image/Image.h"
DWORD CSpriteVisObj::dwIdleData = 0;
CSpriteVisObj::CSpriteVisObj() 
{ 
	bComplexSprite = false;
	nViewTurn = 0;
	pFillTexture = 0;
	nFillFrame = -1;
	info.color = 0xffffffff;
	info.specular = 0xff000000; 
	info2.color = 0xffffffff; 
	info2.specular = 0xff000000; 
	nNextIdle = 0;
}
bool CSpriteVisObj::Init( IGFXTexture *_pTexture, ISpriteAnimation *_pAnimation ) 
{ 
	pTexture = _pTexture; 
	pAnim = _pAnimation; 
	if ( dwIdleData == 0 ) 
		dwIdleData = PackDWORD( GetGlobalVar("Scene.InfantryIdle.Interval", 10000), GetGlobalVar("Scene.InfantryIdle.Random", 5000) );
	RetrieveSpriteInfo();
	return true; 
}
static CExtractAnimVisitor animVisitor;
void CSpriteVisObj::RetrieveSpriteInfo()
{
	pAnim->SetDirection( NCameraTurn::ViewDirection( GetDir(), nViewTurn ) );
	pAnim->Visit( &animVisitor );
	if ( const SSpriteRect *pSprite = animVisitor.GetSpriteRect() ) 
	{
		bComplexSprite = false;
		info.maps = pSprite->maps;
		info.rect = pSprite->rect;
		info.pos = GetPos();
		info.pTexture = pTexture;
		info.fDepthLeft = pSprite->fDepthLeft;
		info.fDepthRight = pSprite->fDepthRight;
	}
	else if ( const SSpritesPack::SSprite *pSprite = animVisitor.GetComplexSprite() )
	{
		bComplexSprite = true;
		info2.pSprite = pSprite;
		info2.pos = GetPos();
		info2.pTexture = pTexture;
	}
}
// The Remastered camera turn lays an object's 2003 shadow sprite on the
// ground turned with it (M006 round 2). The artists drew only the shadow the
// object leaves in sight at step 0: where its own picture covers the ground
// (a trunk, the lower crown, a box's front) the sprite is empty, and turned,
// that hole lies between the object's foot and its shadow as lit ground. The
// hole is filled from the 2003 pictures alone: the object's pixels (its pack
// is the shadow pack's name without the last `s`, at the same frame) that lie
// in the convex hull of the shadow and the zero cross. The fill is squares
// of one of the shadow's own opaque texels, so it darkens the ground as the
// shadow does (the shadow pass darkens a pixel once); it is drawn only at a
// turned step, where the object no longer covers it.
namespace NShadowFill
{
	const int ALPHA_REF = 50;							// the sprite passes' alpha test
	typedef std::vector< std::pair< CTRect<float>, CTRect<float> > > CPieces;
	// A sprite's alpha on a canvas in screen pixels from its zero cross.
	struct SMask
	{
		int nX0, nY0, nSizeX, nSizeY;
		std::vector<BYTE> alpha;
		SMask() : nX0( 0 ), nY0( 0 ), nSizeX( 0 ), nSizeY( 0 ) {  }
		bool IsSet( const int x, const int y ) const
		{
			return x >= nX0 && y >= nY0 && x < nX0 + nSizeX && y < nY0 + nSizeY && alpha[( y - nY0 ) * nSizeX + x - nX0] >= ALPHA_REF;
		}
	};
	struct SFill
	{
		bool bValid;
		SSpritesPack::SSprite sprite;
		SFill() : bValid( false ) {  }
	};
	// The current frame of an animation: screen rects from the zero cross and their texture rects.
	static void GetPieces( ISpriteAnimation *pAnim, CPieces *pPieces )
	{
		animVisitor.Clear();
		pAnim->Visit( &animVisitor );
		if ( const SSpriteRect *pRect = animVisitor.GetSpriteRect() )
			pPieces->push_back( std::make_pair( CTRect<float>( pRect->rect.x1, pRect->rect.y1, pRect->rect.x2, pRect->rect.y2 ), pRect->maps ) );
		else if ( const SSpritesPack::SSprite *pSprite = animVisitor.GetComplexSprite() )
		{
			for ( SSpritesPack::SSprite::CSquaresList::const_iterator it = pSprite->squares.begin(); it != pSprite->squares.end(); ++it )
				pPieces->push_back( std::make_pair( CTRect<float>( it->vLeftTop.x, it->vLeftTop.y, it->vLeftTop.x + it->fSize, it->vLeftTop.y + it->fSize ), it->rcMaps ) );
		}
		animVisitor.Clear();
	}
	// A pack's atlas as the data holds it (any quality: texture rects are relative).
	static IImage* LoadAtlas( const std::string &szName )
	{
		static const char *pszSuffixes[] = { "_h.dds", "_c.dds", "_l.dds" };
		for ( int i = 0; i < 3; ++i )
		{
			CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( ( szName + pszSuffixes[i] ).c_str(), STREAM_ACCESS_READ );
			if ( pStream == 0 )
				continue;
			CPtr<IDDSImage> pDDS = GetImageProcessor()->LoadDDSImage( pStream );
			if ( pDDS != 0 )
				return GetImageProcessor()->Decompress( pDDS );
		}
		return 0;
	}
	static int Pixel( const float fCoord ) { return int( floorf( fCoord + 0.5f ) ); }
	static void Rasterize( ISpriteAnimation *pAnim, const IImage *pAtlas, SMask *pMask )
	{
		CPieces pieces;
		GetPieces( pAnim, &pieces );
		if ( pieces.empty() )
			return;
		int nX1 = 1 << 20, nY1 = 1 << 20, nX2 = -( 1 << 20 ), nY2 = -( 1 << 20 );
		for ( int i = 0; i < pieces.size(); ++i )
		{
			nX1 = Min( nX1, Pixel( pieces[i].first.x1 ) );
			nY1 = Min( nY1, Pixel( pieces[i].first.y1 ) );
			nX2 = Max( nX2, Pixel( pieces[i].first.x2 ) );
			nY2 = Max( nY2, Pixel( pieces[i].first.y2 ) );
		}
		pMask->nX0 = nX1;
		pMask->nY0 = nY1;
		pMask->nSizeX = Max( 0, nX2 - nX1 );
		pMask->nSizeY = Max( 0, nY2 - nY1 );
		pMask->alpha.assign( pMask->nSizeX * pMask->nSizeY, 0 );
		const int nSizeX = pAtlas->GetSizeX(), nSizeY = pAtlas->GetSizeY();
		const SColor *pTexels = pAtlas->GetLFB();
		for ( int i = 0; i < pieces.size(); ++i )
		{
			const CTRect<float> &rcScreen = pieces[i].first, &rcMaps = pieces[i].second;
			const int nLeft = Pixel( rcScreen.x1 ), nTop = Pixel( rcScreen.y1 ), nRight = Pixel( rcScreen.x2 ), nBottom = Pixel( rcScreen.y2 );
			for ( int y = nTop; y < nBottom; ++y )
			{
				const float fV = rcMaps.y1 + ( y - nTop + 0.5f ) / float( nBottom - nTop ) * ( rcMaps.y2 - rcMaps.y1 );
				const int nTexelY = Max( 0, Min( nSizeY - 1, int( floorf( fV * nSizeY ) ) ) );
				for ( int x = nLeft; x < nRight; ++x )
				{
					const float fU = rcMaps.x1 + ( x - nLeft + 0.5f ) / float( nRight - nLeft ) * ( rcMaps.x2 - rcMaps.x1 );
					const int nTexelX = Max( 0, Min( nSizeX - 1, int( floorf( fU * nSizeX ) ) ) );
					BYTE &alpha = pMask->alpha[( y - pMask->nY0 ) * pMask->nSizeX + x - pMask->nX0];
					alpha = Max( alpha, BYTE( pTexels[nTexelY * nSizeX + nTexelX].a ) );
				}
			}
		}
	}
	static long long Cross( const CTPoint<int> &o, const CTPoint<int> &a, const CTPoint<int> &b )
	{
		return (long long)( a.x - o.x ) * ( b.y - o.y ) - (long long)( a.y - o.y ) * ( b.x - o.x );
	}
	static bool PointLess( const CTPoint<int> &a, const CTPoint<int> &b ) { return a.x < b.x || ( a.x == b.x && a.y < b.y ); }
	// The convex hull (Andrew's monotone chain).
	static void Hull( std::vector< CTPoint<int> > points, std::vector< CTPoint<int> > *pHull )
	{
		std::sort( points.begin(), points.end(), PointLess );
		pHull->assign( 2 * points.size(), CTPoint<int>( 0, 0 ) );
		int k = 0;
		for ( int i = 0; i < points.size(); ++i )
		{
			while ( k >= 2 && Cross( (*pHull)[k - 2], (*pHull)[k - 1], points[i] ) <= 0 )
				--k;
			(*pHull)[k++] = points[i];
		}
		for ( int i = int( points.size() ) - 2, t = k + 1; i >= 0; --i )
		{
			while ( k >= t && Cross( (*pHull)[k - 2], (*pHull)[k - 1], points[i] ) <= 0 )
				--k;
			(*pHull)[k++] = points[i];
		}
		pHull->resize( Max( 0, k - 1 ) );
	}
	static bool IsInside( const std::vector< CTPoint<int> > &hull, const CTPoint<int> &point )
	{
		for ( int i = 0; i < hull.size(); ++i )
		{
			if ( Cross( hull[i], hull[( i + 1 ) % hull.size()], point ) < 0 )
				return false;
		}
		return true;
	}
	static void Make( const std::string &szShadow, ISpriteAnimation *pShadowAnim, SFill *pFill )
	{
		if ( szShadow.empty() || ( szShadow[szShadow.size() - 1] != 's' && szShadow[szShadow.size() - 1] != 'S' ) )
			return;
		const std::string szPicture = szShadow.substr( 0, szShadow.size() - 1 );
		CPtr<ISpriteAnimation> pPicture = GetSingleton<IAnimationManager>()->GetSpriteAnimation( ( szPicture + ".san" ).c_str() );
		CPtr<IImage> pShadowAtlas = LoadAtlas( szShadow );
		CPtr<IImage> pPictureAtlas = LoadAtlas( szPicture );
		if ( pPicture == 0 || pShadowAtlas == 0 || pPictureAtlas == 0 )
			return;
		if ( pShadowAnim->GetFrameIndex() >= 0 )
			pPicture->SetFrameIndex( pShadowAnim->GetFrameIndex() );
		SMask shadow, picture;
		Rasterize( pShadowAnim, pShadowAtlas, &shadow );
		Rasterize( pPicture, pPictureAtlas, &picture );
		std::vector< CTPoint<int> > points( 1, CTPoint<int>( 0, 0 ) ), hull;
		for ( int y = shadow.nY0; y < shadow.nY0 + shadow.nSizeY; ++y )
		{
			for ( int x = shadow.nX0; x < shadow.nX0 + shadow.nSizeX; ++x )
			{
				if ( shadow.IsSet( x, y ) )
					points.push_back( CTPoint<int>( x, y ) );
			}
		}
		Hull( points, &hull );
		if ( hull.size() < 3 )
			return;
		// The texel every square shows: an opaque one amid opaque ones, as the
		// turned and the filtered mapping read a texel or two around it.
		const int nSizeX = pShadowAtlas->GetSizeX(), nSizeY = pShadowAtlas->GetSizeY();
		const SColor *pTexels = pShadowAtlas->GetLFB();
		CTPoint<int> texel( -1, -1 );
		for ( int y = 2; y + 2 < nSizeY && texel.x < 0; ++y )
		{
			for ( int x = 2; x + 2 < nSizeX && texel.x < 0; ++x )
			{
				bool bOpaque = true;
				for ( int i = 0; i < 25 && bOpaque; ++i )
					bOpaque = pTexels[( y + i / 5 - 2 ) * nSizeX + x + i % 5 - 2].a >= 128;
				if ( bOpaque )
					texel = CTPoint<int>( x, y );
			}
		}
		if ( texel.x < 0 )
			return;
		const float fU = ( texel.x + 0.5f ) / nSizeX, fV = ( texel.y + 0.5f ) / nSizeY;
		// The hull's pixels the picture or the shadow covers: overlapping the
		// shadow, the fill leaves no seam where the two meet.
		const int nX0 = Min( picture.nX0, shadow.nX0 ), nY0 = Min( picture.nY0, shadow.nY0 );
		const int nW = Max( picture.nX0 + picture.nSizeX, shadow.nX0 + shadow.nSizeX ) - nX0;
		const int nH = Max( picture.nY0 + picture.nSizeY, shadow.nY0 + shadow.nSizeY ) - nY0;
		std::vector<BYTE> fill( nW * nH, 0 );
		for ( int y = 0; y < nH; ++y )
		{
			for ( int x = 0; x < nW; ++x )
			{
				const CTPoint<int> point( nX0 + x, nY0 + y );
				fill[y * nW + x] = ( picture.IsSet( point.x, point.y ) || shadow.IsSet( point.x, point.y ) ) && IsInside( hull, point );
			}
		}
		// Squares, each the largest from its first pixel not yet covered.
		std::vector<BYTE> covered( fill.size(), 0 );
		CTRect<float> rcBound( 1e30f, 1e30f, -1e30f, -1e30f );
		for ( int y = 0; y < nH; ++y )
		{
			for ( int x = 0; x < nW; ++x )
			{
				if ( fill[y * nW + x] == 0 || covered[y * nW + x] != 0 )
					continue;
				int n = 1;
				for ( bool bGrows = true; bGrows; )
				{
					bGrows = x + n < nW && y + n < nH;
					for ( int i = 0; i <= n && bGrows; ++i )
						bGrows = fill[( y + n ) * nW + x + i] != 0 && fill[( y + i ) * nW + x + n] != 0;
					if ( bGrows )
						++n;
				}
				for ( int i = 0; i < n * n; ++i )
					covered[( y + i / n ) * nW + x + i % n] = 1;
				SSpritesPack::SSprite::SSquare square;
				square.vLeftTop = CVec2( nX0 + x, nY0 + y );
				square.fSize = n;
				square.rcMaps.Set( fU, fV, fU, fV );
				square.fDepthLeft = square.fDepthRight = 0;
				pFill->sprite.squares.push_back( square );
				rcBound.Set( Min( rcBound.x1, square.vLeftTop.x ), Min( rcBound.y1, square.vLeftTop.y ),
										 Max( rcBound.x2, square.vLeftTop.x + n ), Max( rcBound.y2, square.vLeftTop.y + n ) );
			}
		}
		if ( pFill->sprite.squares.empty() )
			return;
		pFill->sprite.edge.rcBoundBox = rcBound;
		pFill->sprite.center = CTPoint<int>( 0, 0 );
		pFill->sprite.fMinDepth = 0;
		pFill->bValid = true;
	}
	// The fill of a shadow sprite's current frame, made once per pack and frame.
	static const SSpritesPack::SSprite* Get( IGFXTexture *pShadowTexture, ISpriteAnimation *pShadowAnim )
	{
		if ( pShadowTexture == 0 || pShadowAnim == 0 )
			return 0;
		static std::map<std::string, SFill> fills;
		const std::string szShadow = GetSingleton<ITextureManager>()->GetTextureName( pShadowTexture );
		const std::string szKey = NStr::Format( "%s#%d", szShadow.c_str(), pShadowAnim->GetFrameIndex() );
		std::map<std::string, SFill>::iterator pos = fills.find( szKey );
		if ( pos == fills.end() )
		{
			pos = fills.insert( std::make_pair( szKey, SFill() ) ).first;
			Make( szShadow, pShadowAnim, &pos->second );
		}
		return pos->second.bValid ? &pos->second.sprite : 0;
	}
}
bool CSpriteVisObj::Update( const NTimer::STime &time, bool bForced )
{
	// A camera turn (Remastered) draws the facing that shows the unit or
	// effect as the turned view sees it (NCameraTurn::ViewDirection), at once
	// even while the game time stands still.
	const int nCameraTurn = NCameraTurn::Step();
	if ( dwLastUpdateTime != time || bForced || nCameraTurn != nViewTurn )
	{
		nViewTurn = nCameraTurn;
		pAnim->SetTime( time );
		RetrieveSpriteInfo();
		dwLastUpdateTime = time;
		if ( pAnim->GetAnimation() == 0 && nNextIdle <= time )
		{
			pAnim->SetStartTime( time );
			const int nIdleInterval = dwIdleData >> 16;
			const int nIdleRandom = dwIdleData & 0x0000ffff;
			nNextIdle = time + nIdleInterval + NWin32Random::Random( -nIdleRandom, nIdleRandom );
		}
	}
	return true;
}
bool CSpriteVisObj::IsHit( const SHMatrix &matTransform, const CVec2 &point, CVec2 *pShift )
{
	CVec3 relpos;
	matTransform.RotateHVector( &relpos, GetPos() );
	CVec2 vHitPoint = point;
	NSceneScreenScale::UnscaleGameplaySpritePoint( &vHitPoint.x, &vHitPoint.y, relpos, GetSingleton<IGFX>()->GetScreenRect() );
	// A mirrored or flat-turned sprite (the Remastered camera turn) is hit where
	// it is drawn: the point is taken back into the 2003 picture's frame.
	if ( info.bMirror || info.nFlatTurn != 0 )
	{
		float fX = vHitPoint.x - relpos.x, fY = vHitPoint.y - relpos.y;
		info.UnturnOffset( &fX, &fY );
		vHitPoint.Set( relpos.x + fX, relpos.y + fY );
	}
	const bool bHit = pAnim->IsHit( relpos, vHitPoint, pShift );
	if ( bHit && pShift )
	{
		pShift->x = relpos.x - point.x;
		pShift->y = relpos.y - point.y;
	}
	return bHit;
}
bool CSpriteVisObj::IsHit( const SHMatrix &matTransform, const RECT &rect )
{
	CVec3 relpos;
	matTransform.RotateHVector( &relpos, GetPos() );
	// The picture is drawn scaled about relpos by the gameplay scale (a larger
	// window, a zoom step), so the rectangle is brought back into the
	// picture's own unscaled frame around the same point - the correction the
	// point pick above applies to its point. Unscaling is a positive scale
	// about one point, so the corners keep their order and "the picture's
	// centre inside the rectangle" answers the same as on screen.
	float fLeft = float( rect.left ), fTop = float( rect.top );
	float fRight = float( rect.right ), fBottom = float( rect.bottom );
	const CTRect<float> rcScreen = GetSingleton<IGFX>()->GetScreenRect();
	NSceneScreenScale::UnscaleGameplaySpritePoint( &fLeft, &fTop, relpos, rcScreen );
	NSceneScreenScale::UnscaleGameplaySpritePoint( &fRight, &fBottom, relpos, rcScreen );
	// Turned back as the point pick above; a mirror and quarter turns of the
	// ground keep a rectangle's sides along the screen's axes.
	if ( info.bMirror || info.nFlatTurn != 0 )
	{
		float fX1 = fLeft - relpos.x, fY1 = fTop - relpos.y, fX2 = fRight - relpos.x, fY2 = fBottom - relpos.y;
		info.UnturnOffset( &fX1, &fY1 );
		info.UnturnOffset( &fX2, &fY2 );
		fLeft = relpos.x + Min( fX1, fX2 );
		fRight = relpos.x + Max( fX1, fX2 );
		fTop = relpos.y + Min( fY1, fY2 );
		fBottom = relpos.y + Max( fY1, fY2 );
	}
	return pAnim->IsHit( relpos, CTRect<float>( fLeft, fTop, fRight, fBottom ) );
}
bool CSpriteVisObj::Draw( IGFX *pGFX )
{
	return false;
}
void CSpriteVisObj::SetScale( float sx, float, float )
{
	if ( pAnim )
	{
		pAnim->SetScale( sx );
		RetrieveSpriteInfo();
		RepositionIcons();
	}
}
void CSpriteVisObj::Visit( ISceneVisitor *pVisitor, int nType )
{
	if ( bComplexSprite ) 
	{
		if ( info2.pSprite == 0 ) 
			RetrieveSpriteInfo();
		pVisitor->VisitSprite( &info2, GetGameType(nType), GetPriority() );
	}
	else
		pVisitor->VisitSprite( &info, GetGameType(nType), GetPriority() );
	VisitShadowFill( pVisitor, nType );
	VisitIcons( pVisitor );
}
// A shadow laid turned with the ground (SetScreenTurn) is drawn with the part
// its object hid at step 0 (NShadowFill), at its place and turned with it.
void CSpriteVisObj::VisitShadowFill( ISceneVisitor *pVisitor, const int nType )
{
	const SBasicSpriteInfo &shown = bComplexSprite ? static_cast<const SBasicSpriteInfo&>( info2 ) : static_cast<const SBasicSpriteInfo&>( info );
	if ( shown.nFlatTurn == 0 || GetGameType( nType ) != SGVOGT_SHADOW )
		return;
	if ( pTexture != pFillTexture || pAnim->GetFrameIndex() != nFillFrame )
	{
		pFillTexture = pTexture;
		nFillFrame = pAnim->GetFrameIndex();
		fillInfo.pSprite = NShadowFill::Get( pTexture, pAnim );
	}
	if ( fillInfo.pSprite == 0 )
		return;
	fillInfo.pos = shown.pos;
	fillInfo.color = shown.color;
	fillInfo.specular = shown.specular;
	fillInfo.pTexture = shown.pTexture;
	fillInfo.nFlatTurn = shown.nFlatTurn;
	fillInfo.bMirror = shown.bMirror;
	pVisitor->VisitSprite( &fillInfo, SGVOGT_SHADOW, GetPriority() );
}
void CSpriteVisObj::RepositionIcons()
{
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_HORIZONTAL );
	
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_HORIZONTAL );
	
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_HORIZONTAL );
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_VERTICAL );
	
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_VERTICAL );
	
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_VCENTER | ICON_PLACEMENT_VERTICAL );
	RepositionIconsLocal( ICON_ALIGNMENT_RIGHT | ICON_ALIGNMENT_BOTTOM | ICON_PLACEMENT_VERTICAL );
}
void CSpriteVisObj::RepositionIconsLocal( DWORD placement )
{
	CTRect<float> rcRect;
	const int nFrameIndex = pAnim->GetFrameIndex();
	if ( nFrameIndex == -1 )
	{
		pAnim->SetFrameIndex( 0 );
		const SSpriteRect &rect = pAnim->GetRect();
		rcRect.Set( -7, -rect.rect.top, 7, -rect.rect.bottom );
		pAnim->SetFrameIndex( nFrameIndex );
	}
	else
	{
		const SSpriteRect &rect = pAnim->GetRect();
		rcRect.Set( -7, -rect.rect.top, 7, -rect.rect.bottom );
	}
	CObjVisObj::RepositionIconsLocal( placement, rcRect, 10 );
}
int CSpriteVisObj::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 20, static_cast<CObjVisObj*>(this) );
	saver.Add( 1, &pTexture );
	saver.Add( 2, &pAnim );
	saver.Add( 3, &info.rect );
	saver.Add( 4, &info.maps );
	saver.Add( 13, &bComplexSprite );
	if ( bComplexSprite ) 
	{
		saver.Add( 5, &info2.color );
		saver.Add( 6, &info2.specular );
		saver.Add( 7, &info2.relpos );
	}
	else
	{
		saver.Add( 5, &info.color );
		saver.Add( 6, &info.specular );
		saver.Add( 7, &info.relpos );
		saver.Add( 14, &info.fDepthLeft );
		saver.Add( 15, &info.fDepthRight );
	}
	saver.Add( 22, &nNextIdle );
	if ( saver.IsReading() )
	{
		info.pos = GetPos();
		info.pTexture = pTexture;
		info2.pos = GetPos();
		info2.pTexture = pTexture;
		if ( dwIdleData == 0 ) 
			dwIdleData = PackDWORD( GetGlobalVar("Scene.InfantryIdle.Interval", 10000), GetGlobalVar("Scene.InfantryIdle.Random", 5000) );
	}
	return 0;
}
