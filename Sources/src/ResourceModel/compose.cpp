#include "StdAfx.h"

#include "compose.h"

#include <algorithm>
#include <iterator>

#include "../Anim/Animation.h"
#include "../RandomMapGen/IB_Types.h"

namespace NResourceModel
{

namespace NCompose
{

void SAnimationDesc::AddUsedFrame( int frame )
{
	if ( std::find( usedFrames.begin(), usedFrames.end(), frame ) == usedFrames.end() )
		usedFrames.push_back( frame );
}

int SAnimationDesc::GetUsedFrameIndex( int frame ) const
{
	std::vector<int>::const_iterator pos = std::find( usedFrames.begin(), usedFrames.end(), frame );
	if ( pos == usedFrames.end() )
		return -1;
	return std::distance( usedFrames.begin(), pos );
}

int GetActionFromName( const std::string &szAnimName, bool *pbKnown )
{
	if ( pbKnown != 0 )
		*pbKnown = true;
	std::string szName = szAnimName;
	NStr::ToLower( szName );
	if ( szName == "idle" )
		return ANIMATION_IDLE;
	else if ( szName == "idle down" )
		return ANIMATION_IDLE_DOWN;
	else if ( szName == "aiming" )
		return ANIMATION_AIMING;
	else if ( szName == "aiming down" )
		return ANIMATION_AIMING_DOWN;
	else if ( szName == "aiming trench" )
		return ANIMATION_AIMING_TRENCH;
	else if ( szName == "run" )
		return ANIMATION_MOVE;
	else if ( szName == "crawl" )
		return ANIMATION_CRAWL;
	else if ( szName == "shoot" )
		return ANIMATION_SHOOT;
	else if ( szName == "shoot down" )
		return ANIMATION_SHOOT_DOWN;
	else if ( szName == "shoot trench" )
		return ANIMATION_SHOOT_TRENCH;
	else if ( szName == "throw" )
		return ANIMATION_THROW;
	else if ( szName == "throw trench" )
		return ANIMATION_THROW_TRENCH;
	else if ( szName == "death" )
		return ANIMATION_DEATH;
	else if ( szName == "death down" )
		return ANIMATION_DEATH_DOWN;
	else if ( szName == "use up" )
		return ANIMATION_USE;
	else if ( szName == "use down" )
		return ANIMATION_USE_DOWN;
	else if ( szName == "pointing" )
		return ANIMATION_POINTING;
	else if ( szName == "binoculars" )
		return ANIMATION_BINOCULARS;
	else if ( szName == "radio" )
		return ANIMATION_RADIO;
	else if ( ( szName == "default" ) || ( szName == "effect" ) )
		return ANIMATION_IDLE;
	else if ( szName == "install" )
		return ANIMATION_INSTALL;
	else if ( szName == "uninstall" )
		return ANIMATION_UNINSTALL;
	else if ( szName == "stand to lie cross" )
		return ANIMATION_LIE;
	else if ( szName == "lie to stand cross" )
		return ANIMATION_STAND;
	else if ( szName == "throw down" )
		return ANIMATION_THROW_DOWN;
	else if ( szName == "idle2" )
		return ANIMATION_IDLE2;
	else if ( szName == "prisoning" )
		return ANIMATION_PRISONING;

	if ( pbKnown != 0 )
		*pbKnown = false;
	return 0;
}

CPtr<IImage> BuildAnimations( std::vector<SAnimationDesc> *pSrc, SSpriteAnimationFormat *pDst, const std::vector<std::string> &szFileNames,
                              bool bProcessImages, DWORD dwMinAlpha, SExportOutcome &outcome )
{
	std::unordered_map<int, int> indices;
	std::vector<std::string> szNewFileNames;
	szNewFileNames.reserve( 1000 );
	{
		std::unordered_map<std::string, int> nameIndices;
		std::unordered_map<std::string, bool> filechecks;
		for ( int i = 0; i < szFileNames.size(); ++i )
		{
			if ( filechecks[szFileNames[i]] == false )
			{
				filechecks[szFileNames[i]] = true;
				szNewFileNames.push_back( szFileNames[i] );
				indices[i] = szNewFileNames.size() - 1;
				nameIndices[szFileNames[i]] = szNewFileNames.size() - 1;
			}
			else
				indices[i] = nameIndices[szFileNames[i]];
		}
	}
	SSpriteAnimationFormat &animations = *pDst;
	std::vector<SAnimationDesc> &animdescs = *pSrc;
	int nMaxAnimationIndex = 0;
	for ( int i = 0; i < animdescs.size(); ++i )
	{
		SAnimationDesc &animdesc = animdescs[i];
		bool bKnown = true;
		nMaxAnimationIndex = Max( nMaxAnimationIndex, GetActionFromName( animdesc.szName, &bKnown ) );
		if ( !bKnown )
			outcome.warnings.push_back( "Don't know animation \"" + animdesc.szName + "\"" );
	}
	animations.animations.resize( nMaxAnimationIndex + 1 );
	for ( int i = 0; i < animdescs.size(); ++i )
	{
		SAnimationDesc &animdesc = animdescs[i];
		animdesc.usedFrames.clear();
		NStr::ToLower( animdesc.szName );
		SSpriteAnimationFormat::SSpriteAnimation *pAnimation = &( animations.animations[GetActionFromName( animdesc.szName )] );
		pAnimation->fSpeed = animdesc.fSpeed;
		pAnimation->bCycled = animdesc.bCycled;
		pAnimation->dirs.resize( animdesc.dirs.size() );
		pAnimation->nFrameTime = animdesc.nFrameTime;
		for ( int j = 0; j < pAnimation->dirs.size(); ++j )
		{
			pAnimation->dirs[j].frames.resize( animdesc.dirs[j].frames.size() );
			for ( int k = 0; k < animdesc.dirs[j].frames.size(); ++k )
			{
				pAnimation->dirs[j].frames[k] = indices[animdesc.dirs[j].frames[k]];
				animdesc.AddUsedFrame( pAnimation->dirs[j].frames[k] );
			}
		}
		std::sort( animdesc.usedFrames.begin(), animdesc.usedFrames.end() );
		for ( int j = 0; j < pAnimation->dirs.size(); ++j )
		{
			for ( int k = 0; k < pAnimation->dirs[j].frames.size(); ++k )
				pAnimation->dirs[j].frames[k] = animdesc.GetUsedFrameIndex( pAnimation->dirs[j].frames[k] );
		}
		pAnimation->rects.resize( animdesc.usedFrames.size() );
	}
	IImageProcessor *pIP = GetImageProcessor();
	std::vector<CPtr<IImage>> holders;
	std::vector<IImage *> images;
	holders.reserve( 1000 );
	images.reserve( 1000 );
	for ( std::vector<std::string>::const_iterator it = szNewFileNames.begin(); it != szNewFileNames.end(); ++it )
	{
		CPtr<IImage> pImage = NImageExport::LoadPicture( *it, outcome );
		if ( pImage == 0 )
			return 0;
		holders.push_back( pImage );
		images.push_back( pImage );
	}
	if ( images.empty() )
	{
		outcome.szError = "Composing images failed: there is no frame to compose";
		return 0;
	}
	std::vector<RECT> rects( images.size() );
	std::vector<RECT> rectsMain( images.size() );
	CPtr<IImage> pImage;
	if ( bProcessImages )
		pImage = pIP->ComposeImages( &( images[0] ), &( rects[0] ), &( rectsMain[0] ), images.size() );
	else
	{
		if ( images.size() != 1 )
		{
			outcome.szError = "Can't compose sprite w/o image pre-processing for non-single image";
			return 0;
		}
		rects[0].left = rectsMain[0].left = 0;
		rects[0].top = rectsMain[0].top = 0;
		rects[0].right = rectsMain[0].right = images[0]->GetSizeX();
		rects[0].bottom = rectsMain[0].bottom = images[0]->GetSizeY();
		pImage = pIP->CreateImage( images[0]->GetSizeX(), images[0]->GetSizeY() );
		if ( pImage != 0 )
			pImage->CopyFrom( images[0], 0, 0, 0 );
	}
	if ( pImage == 0 )
	{
		outcome.szError = "Composing images failed!";
		return 0;
	}
	for ( int i = 0; i < rects.size(); ++i )
	{
		if ( ( rects[i].left == rects[i].right ) || ( rects[i].top == rects[i].bottom ) )
			outcome.warnings.push_back( "Image \"" + szNewFileNames[i] + "\" is empty. May be this is a 4 dir animation" );
	}
	CUnsafeImageAccessor unsafeImageAccessor = pImage;
	for ( int i = 0; i < animdescs.size(); ++i )
	{
		SAnimationDesc &animdesc = animdescs[i];
		SSpriteAnimationFormat::SSpriteAnimation *pAnimation = &( animations.animations[GetActionFromName( animdesc.szName )] );
		for ( int j = 0; j < pAnimation->rects.size(); ++j )
		{
			const RECT &rcSubRect = rects[animdesc.usedFrames[j]];
			pAnimation->rects[j].maps.x1 = ( float( rcSubRect.left ) + 0.5f ) / float( pImage->GetSizeX() );
			pAnimation->rects[j].maps.x2 = ( float( rcSubRect.right ) + 0.5f ) / float( pImage->GetSizeX() );
			pAnimation->rects[j].maps.y1 = ( float( rcSubRect.top ) + 0.5f ) / float( pImage->GetSizeY() );
			pAnimation->rects[j].maps.y2 = ( float( rcSubRect.bottom ) + 0.5f ) / float( pImage->GetSizeY() );
			const RECT &rcBase = rectsMain[animdesc.usedFrames[j]];
			// MFC read the first entry of the unordered map and an empty map
			// was undefined; here it is no shift. The entries of one
			// animation hold one shift, so which one is first does not matter.
			CVec2 ptFrame = animdesc.frames.empty() ? VNULL2 : animdesc.frames.begin()->second;
			pAnimation->rects[j].rect.Set( rcSubRect.left - rcBase.left - ptFrame.x,
			                               rcSubRect.top - rcBase.top - ptFrame.y,
			                               rcSubRect.right - rcBase.left - ptFrame.x,
			                               rcSubRect.bottom - rcBase.top - ptFrame.y );
			pAnimation->rects[j].fDepthLeft = 0.0f;
			pAnimation->rects[j].fDepthRight = 0.0f;
			if ( dwMinAlpha > 0 )
			{
				// MFC's right edge column is rcSubRect.right itself, which is
				// one past the frame and one past the picture for a frame
				// at its right border; that column is skipped here.
				int nXIndex = rcSubRect.left;
				for ( int nYIndex = ( rcSubRect.bottom - 1 ); nYIndex >= rcSubRect.top; --nYIndex )
				{
					if ( unsafeImageAccessor[nYIndex][nXIndex].a >= dwMinAlpha )
					{
						pAnimation->rects[j].fDepthLeft = rcSubRect.bottom - 1 - nYIndex - pAnimation->rects[j].rect.maxy;
					}
				}
				nXIndex = rcSubRect.right;
				if ( nXIndex < pImage->GetSizeX() )
					for ( int nYIndex = ( rcSubRect.bottom - 1 ); nYIndex >= rcSubRect.top; --nYIndex )
					{
						if ( unsafeImageAccessor[nYIndex][nXIndex].a >= dwMinAlpha )
						{
							pAnimation->rects[j].fDepthRight = rcSubRect.bottom - 1 - nYIndex - pAnimation->rects[j].rect.maxy;
						}
					}
			}
		}
	}
	return pImage;
}

CVec2 Origin2DPosition( const SGroundCamera &camera, const CVec2 &vOrigin )
{
	// The matrix applied to (-vOrigin) and to the origin, subtracted: the
	// translation cancels and only the camera's linear part is left.
	const GridProjection projection( camera );
	const SVec2 vOriginScreen = projection.Pos3To2( SVec3{ -vOrigin.x, -vOrigin.y, 0 } );
	const SVec2 vSpriteScreen = projection.Pos3To2( SVec3{ 0, 0, 0 } );
	return CVec2( vOriginScreen.x - vSpriteScreen.x, vOriginScreen.y - vSpriteScreen.y );
}

namespace
{

// One half of ComposeSingleObjectPack: the picture packed by the engine's
// sprite set builder, which draws pass into the packed result around
// lockedTilesCenter.
CPtr<IImage> PackSprites( SSpritesPack &pack, IImage *pImage, const CVec2 &zeroPos, const CArray2D<BYTE> &pass, const CVec2 &vLockedTilesCenter, SExportOutcome &outcome )
{
	CSpritesPackBuilder::SPackParameter param;
	param.pImage = pImage;
	param.center = CTPoint<int>( zeroPos.x, zeroPos.y );
	param.lockedTiles = pass;
	param.lockedTilesCenter = CTPoint<int>( vLockedTilesCenter.x, vLockedTilesCenter.y );
	CPtr<IImage> pPacked = CSpritesPackBuilder::Pack( &pack, param, 256, 5 );
	if ( pPacked == 0 )
		outcome.szError = "cannot pack the sprites of the picture";
	return pPacked;
}

}

bool ComposeSingleObjectPack( const SExportContext &context, const NImageExport::SGamma &gamma, EGFXPixelFormat lowFormat,
                              const std::string &szSprite, const std::string &szShadow, const std::string &szName,
                              const CVec2 &zeroPos, const CArray2D<BYTE> &pass, const CVec2 &vLockedTilesCenter, SExportOutcome &outcome )
{
	CPtr<IImage> pSpriteImage = NImageExport::LoadPicture( szSprite, outcome );
	if ( pSpriteImage == 0 )
		return false;
	CPtr<IImage> pShadowImage = NImageExport::LoadPicture( szShadow, outcome );
	if ( pShadowImage == 0 )
		return false;
	if ( pSpriteImage->GetSizeX() != pShadowImage->GetSizeX() || pSpriteImage->GetSizeY() != pShadowImage->GetSizeY() )
	{
		outcome.szError = "The size of sprite does not equal the size of shadow: " + szSprite + " is " + std::to_string( pSpriteImage->GetSizeX() ) + "x" +
		                  std::to_string( pSpriteImage->GetSizeY() ) + ", " + szShadow + " is " + std::to_string( pShadowImage->GetSizeX() ) + "x" + std::to_string( pShadowImage->GetSizeY() );
		return false;
	}

	SSpritesPack spritePack;
	CPtr<IImage> pPackedSprite = PackSprites( spritePack, pSpriteImage, zeroPos, pass, vLockedTilesCenter, outcome );
	if ( pPackedSprite == 0 )
		return false;
	if ( !NImageExport::SaveCompressedTexture( context, pPackedSprite, szName, gamma, lowFormat, outcome ) ||
	     !NImageExport::SaveSpritesPack( context, spritePack, szName + ".san", outcome ) )
		return false;

	CPtr<IImage> pInverseSprite = pSpriteImage->Duplicate();
	pInverseSprite->SharpenAlpha( 128 );
	pInverseSprite->InvertAlpha();
	RECT rc = { 0, 0, pInverseSprite->GetSizeX(), pInverseSprite->GetSizeY() };
	pShadowImage->ModulateAlphaFrom( pInverseSprite, &rc, 0, 0 );
	pShadowImage->SetColor( DWORD( 0 ) );

	SSpritesPack shadowPack;
	CPtr<IImage> pPackedShadow = PackSprites( shadowPack, pShadowImage, zeroPos, CArray2D<BYTE>(), VNULL2, outcome );
	if ( pPackedShadow == 0 )
		return false;
	return NImageExport::SaveShadowTexture( context, pPackedShadow, szName + "s", outcome ) &&
	       NImageExport::SaveSpritesPack( context, shadowPack, szName + "s.san", outcome );
}

bool ComposeNoisePack( const SExportContext &context, const NImageExport::SGamma &gamma, EGFXPixelFormat lowFormat,
                       const std::string &szSprite, const std::string &szNoise, const std::string &szName,
                       const CVec2 &zeroPos, SExportOutcome &outcome )
{
	CPtr<IImage> pSpriteImage = NImageExport::LoadPicture( szSprite, outcome );
	if ( pSpriteImage == 0 )
		return false;
	CPtr<IImage> pNoiseImage = NImageExport::LoadPicture( szNoise, outcome );
	if ( pNoiseImage == 0 )
		return false;
	if ( pSpriteImage->GetSizeX() != pNoiseImage->GetSizeX() || pSpriteImage->GetSizeY() != pNoiseImage->GetSizeY() )
	{
		outcome.szError = "The size of building image is not equal to the size of noise file: " + szSprite + " is " + std::to_string( pSpriteImage->GetSizeX() ) + "x" +
		                  std::to_string( pSpriteImage->GetSizeY() ) + ", " + szNoise + " is " + std::to_string( pNoiseImage->GetSizeX() ) + "x" + std::to_string( pNoiseImage->GetSizeY() );
		return false;
	}
	CPtr<IImage> pInverseSprite = pSpriteImage->Duplicate();
	pInverseSprite->SharpenAlpha( 128 );
	pInverseSprite->InvertAlpha();
	RECT rc = { 0, 0, pInverseSprite->GetSizeX(), pInverseSprite->GetSizeY() };
	pNoiseImage->ModulateAlphaFrom( pInverseSprite, &rc, 0, 0 );

	SSpritesPack noisePack;
	CPtr<IImage> pPackedNoise = PackSprites( noisePack, pNoiseImage, zeroPos, CArray2D<BYTE>(), VNULL2, outcome );
	if ( pPackedNoise == 0 )
		return false;
	return NImageExport::SaveCompressedTexture( context, pPackedNoise, szName, gamma, lowFormat, outcome ) &&
	       NImageExport::SaveSpritesPack( context, noisePack, szName + ".san", outcome );
}

bool ComposeSpritesPack( const SExportContext &context, const NImageExport::SGamma &gamma, EGFXPixelFormat lowFormat,
                         std::vector<SPackPicture> &pictures, const std::string &szName, bool *pbShadowFailed, SExportOutcome &outcome )
{
	if ( pbShadowFailed != 0 )
		*pbShadowFailed = false;
	CSpritesPackBuilder::CPackParameters spriteParams, shadowParams;
	for ( SPackPicture &picture : pictures )
	{
		CSpritesPackBuilder::SPackParameter param;
		param.pImage = picture.pSprite;
		param.center = CTPoint<int>( picture.zeroPos.x, picture.zeroPos.y );
		param.lockedTiles = picture.pass;
		param.lockedTilesCenter = CTPoint<int>( picture.vPassOrigin.x, picture.vPassOrigin.y );
		spriteParams.push_back( param );

		CPtr<IImage> pInverseSprite = picture.pSprite->Duplicate();
		pInverseSprite->SharpenAlpha( 128 );
		pInverseSprite->InvertAlpha();
		RECT rc = { 0, 0, pInverseSprite->GetSizeX(), pInverseSprite->GetSizeY() };
		picture.pShadow->ModulateAlphaFrom( pInverseSprite, &rc, 0, 0 );
		picture.pShadow->SetColor( DWORD( 0 ) );
		CSpritesPackBuilder::SPackParameter shadow;
		shadow.pImage = picture.pShadow;
		shadow.center = param.center;
		shadowParams.push_back( shadow );
	}

	SSpritesPack spritePack;
	CPtr<IImage> pPackedSprite = CSpritesPackBuilder::Pack( &spritePack, spriteParams, 256, 5 );
	if ( pPackedSprite == 0 )
	{
		outcome.szError = "cannot pack the sprites of " + szName;
		return false;
	}
	if ( !NImageExport::SaveCompressedTexture( context, pPackedSprite, szName, gamma, lowFormat, outcome ) ||
	     !NImageExport::SaveSpritesPack( context, spritePack, szName + ".san", outcome ) )
		return false;

	SSpritesPack shadowPack;
	CPtr<IImage> pPackedShadow = CSpritesPackBuilder::Pack( &shadowPack, shadowParams, 256, 5 );
	if ( pPackedShadow == 0 )
	{
		if ( pbShadowFailed != 0 )
			*pbShadowFailed = true;
		return true;
	}
	return NImageExport::SaveShadowTexture( context, pPackedShadow, szName + "s", outcome ) &&
	       NImageExport::SaveSpritesPack( context, shadowPack, szName + "s.san", outcome );
}

bool SaveShadowFile( const std::string &szSprite, const std::string &szShadow, const std::string &szTempShadow, SExportOutcome &outcome )
{
	CPtr<IImage> pSpriteImage = NImageExport::LoadPicture( szSprite, outcome );
	if ( pSpriteImage == 0 )
		return false;
	CPtr<IImage> pInverseSprite = pSpriteImage->Duplicate();
	pInverseSprite->SharpenAlpha( 100 );
	pInverseSprite->InvertAlpha();

	CPtr<IImage> pShadowImage = NImageExport::LoadPicture( szShadow, outcome );
	if ( pShadowImage == 0 )
		return false;
	if ( pInverseSprite->GetSizeX() != pShadowImage->GetSizeX() || pInverseSprite->GetSizeY() != pShadowImage->GetSizeY() )
	{
		outcome.szError = "The size of sprite does not equal the size of shadow: " + szSprite + " is " + std::to_string( pSpriteImage->GetSizeX() ) + "x" +
		                  std::to_string( pSpriteImage->GetSizeY() ) + ", " + szShadow + " is " + std::to_string( pShadowImage->GetSizeX() ) + "x" + std::to_string( pShadowImage->GetSizeY() );
		return false;
	}
	RECT rc = { 0, 0, pInverseSprite->GetSizeX(), pInverseSprite->GetSizeY() };
	pShadowImage->ModulateAlphaFrom( pInverseSprite, &rc, 0, 0 );
	pShadowImage->SetColor( DWORD( 0 ) );
	return NImageExport::SaveTgaFile( szTempShadow, pShadowImage, outcome );
}

bool SaveIconFile( const SExportContext &context, const std::string &szSource, const std::string &szName, SExportOutcome &outcome )
{
	const int ICON_SIZE = 64;
	CPtr<IImage> pSrcImage = NImageExport::LoadPicture( szSource, outcome );
	if ( pSrcImage == 0 )
		return false;
	IImageProcessor *pIP = GetImageProcessor();

	const int nSizeX = pSrcImage->GetSizeX();
	const int nSizeY = pSrcImage->GetSizeY();
	int nMinX = nSizeX, nMinY = -1;
	int nMaxX = 0, nMaxY = 0;
	const SColor *pLFB = pSrcImage->GetLFB();
	for ( int y = 0; y < nSizeY; y++ )
	{
		int nCurMinX = -1;
		int nCurMaxX = 0;
		for ( int x = 0; x < nSizeX; x++ )
		{
			if ( pLFB[x + y * nSizeX].a )
			{
				nCurMaxX = x;
				if ( nCurMinX == -1 )
					nCurMinX = x;
			}
		}
		if ( nCurMinX >= 0 && nCurMinX < nMinX )
			nMinX = nCurMinX;
		if ( nCurMaxX > nMaxX )
			nMaxX = nCurMaxX;
		if ( nCurMaxX > 0 )
		{
			nMaxY = y;
			if ( nMinY == -1 )
				nMinY = y;
		}
	}
	if ( nMinY == -1 )
	{
		outcome.szError = "Error: image alpha is empty? " + szSource + ": can not create icon image";
		return false;
	}

	// MFC cropped only when the box differed from the picture, and used the
	// cropped image unconditionally afterwards (a null for a full-size box).
	CPtr<IImage> pMinImage = pSrcImage;
	if ( nMaxX - nMinX != nSizeX || nMaxY - nMinY != nSizeY )
	{
		pMinImage = pIP->CreateImage( nMaxX - nMinX, nMaxY - nMinY );
		SColor col;
		col.r = col.g = col.b = 146;
		col.a = 0;
		pMinImage->Set( col );
		RECT rc = { nMinX, nMinY, nMaxX, nMaxY };
		pMinImage->CopyFromAB( pSrcImage, &rc, 0, 0 );
	}

	const double fRateX = (double) ICON_SIZE / pMinImage->GetSizeX();
	const double fRateY = (double) ICON_SIZE / pMinImage->GetSizeY();
	const double fRate = (std::min)( fRateX, fRateY );
	CPtr<IImage> pScaleImage = pIP->CreateScale( pMinImage, fRate, ISM_LANCZOS3 );
	if ( pScaleImage == 0 )
	{
		outcome.szError = "Error: can not create icon file " + szName + " from " + szSource;
		return false;
	}

	CPtr<IImage> pResImage = pIP->CreateImage( ICON_SIZE, ICON_SIZE );
	SColor col;
	col.r = col.g = col.b = 146;
	col.a = 0;
	pResImage->Set( col );
	const int nScaledX = pScaleImage->GetSizeX();
	const int nScaledY = pScaleImage->GetSizeY();
	RECT rc = { 0, 0, nScaledX, nScaledY };
	if ( nScaledY < ICON_SIZE )
		pResImage->CopyFrom( pScaleImage, &rc, 0, ( ICON_SIZE - nScaledY ) / 2 );
	else if ( nScaledX < ICON_SIZE )
		pResImage->CopyFrom( pScaleImage, &rc, ( ICON_SIZE - nScaledX ) / 2, 0 );
	else
		pResImage = pScaleImage;
	return NImageExport::SaveTga( context, pResImage, szName, outcome );
}

}

}
