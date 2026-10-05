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

}

}
