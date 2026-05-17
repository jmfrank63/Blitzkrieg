#include "StdAfx.h"
#include <io.h>

#include "..\Anim\Animation.h"
#include "..\Formats\fmtAnimation.h"
#include "..\GFX\GFX.h"
#include "..\Scene\Scene.h"
#include "..\Image\Image.h"
#include "..\Image\ImageHelper.h"

#include "SpriteCompose.h"
#include "ParentFrame.h"
#include "Frames.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SAnimationDesc::SDirDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Frames", &frames );
	saver.Add( "FrameShift", &ptFrameShift );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SAnimationDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Name", &szName );
	saver.Add( "Dirs", &dirs );
	saver.Add( "Frames", &frames );
	saver.Add( "FrameTime", &nFrameTime );
	saver.Add( "FrameShift", &ptFrameShift );
	saver.Add( "AnimSpeed", &fSpeed );
	saver.Add( "Cycled", &bCycled );
	return 0;
}

bool BuildSpritesPack( const CSpritesPackBuilder::CPackParameters &rPackParameters, const std::string &rszSpritesPackFileName )
{
  IImageProcessor *pImageProcessor = GetImageProcessor();
  NI_ASSERT_TF( pImageProcessor != 0,
								NStr::Format( "Can't get CImageProcessor %x", pImageProcessor ),
								return false );

	SSpritesPack spritesPack;
	DWORD dwSignature = SSpritesPack::SIGNATURE;

  CPtr<IImage> pSpritesPackImage = CSpritesPackBuilder::Pack( &spritesPack, rPackParameters, 256, 5 );
  if( pSpritesPackImage == 0 )
  {
    return false;
  }

  CPtr<IDataStream> pSpritesPackImageStream = CreateFileStream( ( rszSpritesPackFileName + ".tga" ).c_str(), STREAM_ACCESS_WRITE );
  NI_ASSERT_TF( pSpritesPackImageStream != 0,
								NStr::Format( "Can't open image file \"%s\" to write", ( rszSpritesPackFileName + ".tga") .c_str() ),
								return false );
  
  CPtr<IDataStream> pSpritesPackStructureStream = CreateFileStream( ( rszSpritesPackFileName + ".san" ).c_str(), STREAM_ACCESS_WRITE );
  NI_ASSERT_TF( pSpritesPackStructureStream != 0,
								NStr::Format( "Can't open structure file \"%s\" to write", ( rszSpritesPackFileName + ".san" ).c_str() ),
								return false );

	pImageProcessor->SaveImageAsTGA( pSpritesPackImageStream, pSpritesPackImage );

	CPtr<IStructureSaver> pSS = CreateStructureSaver( pSpritesPackStructureStream, IStructureSaver::WRITE );
	CSaverAccessor saver = pSS;
	saver.Add( 1, &spritesPack );
	saver.Add( 127, &dwSignature );
  return true;
}

bool BuildSpritesPack( const CSpritesPackBuilder::SPackParameter &rPackParameter, const std::string &rszSpritesPackFileName )
{
  IImageProcessor *pImageProcessor = GetImageProcessor();
  NI_ASSERT_TF( pImageProcessor != 0,
								NStr::Format( "Can't get CImageProcessor %x", pImageProcessor ),
								return false );

	SSpritesPack spritesPack;
	DWORD dwSignature = SSpritesPack::SIGNATURE;

  CPtr<IImage> pSpritesPackImage = CSpritesPackBuilder::Pack( &spritesPack, rPackParameter, 256, 5 );
  if( pSpritesPackImage == 0 )
  {
    return false;
  }

  CPtr<IDataStream> pSpritesPackImageStream = CreateFileStream( ( rszSpritesPackFileName + ".tga" ).c_str(), STREAM_ACCESS_WRITE );
  NI_ASSERT_TF( pSpritesPackImageStream != 0,
								NStr::Format( "Can't open image file \"%s\" to write", ( rszSpritesPackFileName + ".tga") .c_str() ),
								return false );
  
  CPtr<IDataStream> pSpritesPackStructureStream = CreateFileStream( ( rszSpritesPackFileName + ".san" ).c_str(), STREAM_ACCESS_WRITE );
  NI_ASSERT_TF( pSpritesPackStructureStream != 0,
								NStr::Format( "Can't open structure file \"%s\" to write", ( rszSpritesPackFileName + ".san" ).c_str() ),
								return false );

	pImageProcessor->SaveImageAsTGA( pSpritesPackImageStream, pSpritesPackImage );

	CPtr<IStructureSaver> pSS = CreateStructureSaver( pSpritesPackStructureStream, IStructureSaver::WRITE );
	CSaverAccessor saver = pSS;
	saver.Add( 1, &spritesPack );
	saver.Add( 127, &dwSignature );
  return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IImage* BuildAnimations( std::vector<SAnimationDesc> *pSrc, SSpriteAnimationFormat *pDst,
												 std::vector<std::string> &szFileNames, bool bProcessImages, DWORD dwMinAlpha )
{
	std::unordered_map<int, int> indices;
	std::vector<std::string> szNewFileNames;
	szNewFileNames.reserve( 1000 );
	{
		std::unordered_map<std::string, int> nameIndices;
		std::unordered_map<std::string, bool> filechecks;
		for ( int i=0; i<szFileNames.size(); ++i )
		{
			if ( filechecks[ szFileNames[i] ] == false )
			{
				filechecks[ szFileNames[i] ] = true;
				szNewFileNames.push_back( szFileNames[i] );
				indices[i] = szNewFileNames.size() - 1;
				nameIndices[ szFileNames[i] ] = szNewFileNames.size() - 1;
			}
			else
				indices[i] = nameIndices[ szFileNames[i] ];
		}
	}
	// fill dst structures
	SSpriteAnimationFormat &animations = *pDst;
	std::vector<SAnimationDesc> &animdescs = *pSrc;
	// find biggest index for number of animations
	int nMaxAnimationIndex = 0;
	for ( int i=0; i<animdescs.size(); ++i )
	{
		SAnimationDesc &animdesc = animdescs[i];
		nMaxAnimationIndex = Max( nMaxAnimationIndex, GetActionFromName( animdesc.szName ) );
	}
	animations.animations.resize( nMaxAnimationIndex + 1 );
	//
	for ( int i=0; i<animdescs.size(); ++i )
	{
		SAnimationDesc &animdesc = animdescs[i];
		animdesc.usedFrames.clear();
		NStr::ToLower( animdesc.szName );
		SSpriteAnimationFormat::SSpriteAnimation *pAnimation = &( animations.animations[GetActionFromName(animdesc.szName)] );
		pAnimation->fSpeed = animdesc.fSpeed;
		pAnimation->bCycled = animdesc.bCycled;
		pAnimation->dirs.resize( animdesc.dirs.size() );
		pAnimation->nFrameTime = animdesc.nFrameTime;
		// �������� ����� � �������� � ����� ������� � ���������� ������ ��������, ������������ � ���� ��������
		for ( int j=0; j<pAnimation->dirs.size(); ++j )
		{
			pAnimation->dirs[j].frames.resize( animdesc.dirs[j].frames.size() );
			for ( int k=0; k<animdesc.dirs[j].frames.size(); ++k )
			{
				pAnimation->dirs[j].frames[k] = indices[ animdesc.dirs[j].frames[k] ];
				animdesc.AddUsedFrame( pAnimation->dirs[j].frames[k] );
			}
		}
		// ��������� ������ ��������, ������������ � ���� ��������
		std::sort( animdesc.usedFrames.begin(), animdesc.usedFrames.end() );
		// ��������� ����� ������� � ��������� ������� ���� ��������
		for ( int j=0; j<pAnimation->dirs.size(); ++j )
		{
			for ( int k=0; k<pAnimation->dirs[j].frames.size(); ++k )
				pAnimation->dirs[j].frames[k] = animdesc.GetUsedFrameIndex( pAnimation->dirs[j].frames[k] );
		}
		//
		pAnimation->rects.resize( animdesc.usedFrames.size() );
	}
	IImageProcessor *pIP = GetImageProcessor();
	std::vector<IImage*> images;
	images.reserve( 1000 );
	//
	// load images
	for ( std::vector<std::string>::const_iterator it = szNewFileNames.begin(); it != szNewFileNames.end(); ++it )
	{
		CPtr<IDataStream> pStream = OpenFileStream( it->c_str(), STREAM_ACCESS_READ );
		NI_ASSERT_TF( pStream != 0, NStr::Format("Can't open image \"%s\"", it->c_str() ), break );
		IImage *pImage = pIP->LoadImage( pStream );
		NI_ASSERT_TF( pImage != 0, NStr::Format("Can't load image from the stream \"%s\"", it->c_str() ), return 0 );
		pImage->AddRef();
		images.push_back( pImage );
	}
	//
	// compose result image
	std::vector<RECT> rects( images.size() );
	std::vector<RECT> rectsMain( images.size() );
	IImage *pImage = 0;
	if ( bProcessImages )
		pImage = pIP->ComposeImages( &(images[0]), &(rects[0]), &(rectsMain[0]), images.size() );
	else
	{
		NI_ASSERT_T( images.size() == 1, "Can't compose sprite w/o image pre-processing for non-single image" );
		SetRect( &(rects[0]), 0, 0, images[0]->GetSizeX(), images[0]->GetSizeY() );
		SetRect( &(rectsMain[0]), 0, 0, images[0]->GetSizeX(), images[0]->GetSizeY() );
		pImage = pIP->CreateImage( images[0]->GetSizeX(), images[0]->GetSizeY() );
		pImage->CopyFrom( images[0], 0, 0, 0 );
	}
	// release source images
	for ( int i=0; i<images.size(); ++i )
		images[i]->Release();
	//
	for ( int i=0; i<rects.size(); ++i )
	{
		NI_ASSERT_TF( (rects[i].left != rects[i].right) && (rects[i].top != rects[i].bottom), NStr::Format("Image \"%s\" are empty. May be this is a 4 dir animation", szNewFileNames[i].c_str()), continue );
	}
	//
	// compose sprite animation data
	CUnsafeImageAccessor unsafeImageAccessor = pImage;
	for ( int i=0; i<animdescs.size(); ++i )
	{
		SAnimationDesc &animdesc = animdescs[i];
		SSpriteAnimationFormat::SSpriteAnimation *pAnimation = &( animations.animations[GetActionFromName(animdesc.szName)] );
		// �� ����� ������ ����� �������� � animdesc.usedFrames.
		// ���������� ��������� �� ��� ����� �����
		//
		for ( int j=0; j<pAnimation->rects.size(); ++j )
		{
			const RECT &rcSubRect = rects[ animdesc.usedFrames[j] ];
			pAnimation->rects[j].maps.x1 = ( float( rcSubRect.left   ) + 0.5f ) / float( pImage->GetSizeX() );
			pAnimation->rects[j].maps.x2 = ( float( rcSubRect.right  ) + 0.5f ) / float( pImage->GetSizeX() );
			pAnimation->rects[j].maps.y1 = ( float( rcSubRect.top    ) + 0.5f ) / float( pImage->GetSizeY() );
			pAnimation->rects[j].maps.y2 = ( float( rcSubRect.bottom ) + 0.5f ) / float( pImage->GetSizeY() );
			const RECT &rcBase = rectsMain[ animdesc.usedFrames[j]  ];
			// CRAP{ �.�. ������ � ����, ��� � ���� �������� ������� ����� ���� � �� ��
			CVec2 ptFrame = animdesc.frames.begin()->second;//animdesc.frames[nOldIndex];
			// CRAP}
			pAnimation->rects[j].rect.Set(	rcSubRect.left - rcBase.left - ptFrame.x,
																			rcSubRect.top - rcBase.top - ptFrame.y, 
																			rcSubRect.right - rcBase.left - ptFrame.x, 
																			rcSubRect.bottom - rcBase.top - ptFrame.y );
			pAnimation->rects[j].fDepthLeft = 0.0f;
			pAnimation->rects[j].fDepthRight = 0.0f;
			if ( dwMinAlpha > 0 )
			{
				int nXIndex = rcSubRect.left;
				for ( int nYIndex = ( rcSubRect.bottom - 1 ); nYIndex >= rcSubRect.top; --nYIndex )
				{
					if ( 	unsafeImageAccessor[nYIndex][nXIndex].a >= dwMinAlpha )
					{
						pAnimation->rects[j].fDepthLeft = rcSubRect.bottom - 1 - nYIndex - pAnimation->rects[j].rect.maxy;
					}
				}
				nXIndex = rcSubRect.right;
				for ( int nYIndex = ( rcSubRect.bottom - 1 ); nYIndex >= rcSubRect.top; --nYIndex )
				{
					if ( 	unsafeImageAccessor[nYIndex][nXIndex].a >= dwMinAlpha )
					{
						pAnimation->rects[j].fDepthRight = rcSubRect.bottom - 1 - nYIndex - pAnimation->rects[j].rect.maxy;
					}
				}
			}
		}
	}
	return pImage;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 ComputeSpriteNewZeroPos( const IVisObj *pSprite, const CVec3 &vZero3, const CVec2 &vZero2Shift )
{
	NI_ASSERT_TF( pSprite != 0, "Can't compute new sprite zero position from NULL sprite", return VNULL2 );

	IScene *pSG = GetSingleton<IScene>();
	// 2D cross position
	CVec2 vZero2;
	pSG->GetPos2( &vZero2, vZero3 );
	vZero2 += vZero2Shift;
	// 2D sprite position
	CVec2 vSprite2;
	pSG->GetPos2( &vSprite2, pSprite->GetPosition() );
	// new 2D sprite zero with respect to sprite's texture top-left corner
	CVec2 vZeroNew;
	{
		// �������� ��������� ���� �������� � �������� �����������
		const SSpriteInfo *pInfo = static_cast<const ISpriteVisObj*>(pSprite)->GetSpriteInfo();
		// �����. �� ������ �������� �� �������� ���� ������� (w, h)
		int w0 = pInfo->pTexture->GetSizeX( 0 );
		int h0 = pInfo->pTexture->GetSizeY( 0 );
		int w1 = ceil( pInfo->maps.left * w0 - 0.5f );
		int w2 = abs( pInfo->rect.left );
		int h1 = ceil( pInfo->maps.top * h0 - 0.5f );
		int h2 = abs( pInfo->rect.top );
		int w = w1 + w2;
		int h = h1 + h2;
		// ��������� ������ ���� ������������ ������ ��������
		vZeroNew = vZero2 - ( vSprite2 - CVec2(w, h) );
	}

	vZeroNew.x = ceil( vZeroNew.x );
	vZeroNew.y = ceil( vZeroNew.y );
	
	return vZeroNew;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec3 Get3DPosition( const CVec3 &vSpritePos, const CVec2 &vPointPos, const SSpritesPack &rSpritesPack, int nSpriteIndex, const CVec2 &vCenter, IGFX *pGFX )
{
	CMatrixStack<4> mstack;
	mstack.Push( pGFX->GetViewportMatrix() );
	mstack.Push( pGFX->GetProjectionMatrix() );
	mstack.Push( pGFX->GetViewMatrix() );
	const SHMatrix &matTransform = mstack();
	//
	CVec3 vTempPos;
	matTransform.RotateHVector( &vTempPos, vSpritePos );
	const CVec3 vSpriteScreenPos = vTempPos;
	// determine z-bias to keep z-buffer happy (treat sprites as vertical)
	CVec3 vecZ1, vecZ2;
	matTransform.RotateHVector( &vecZ1, CVec3(0, 0, 0) );
	matTransform.RotateHVector( &vecZ2, CVec3(0, 0, 2.0f/FP_SQRT_3) );
	const float fZBias = vecZ2.z - vecZ1.z;
	matTransform.RotateHVector( &vecZ2, CVec3(-FP_SQRT_2, FP_SQRT_2, 0) );
	const float fZBias2 = vecZ2.z - vecZ1.z;
	
	// calculate distance from zero line to object's border and write values to:
	float fZero2Border = 0.0f;
	float fBorder2Point = -vPointPos.y;

	//��������
	SSpritesPack::CSpritesList::const_iterator spritesListIterator = rSpritesPack.sprites.begin();
	for ( int nSpritePackIndex = 0; nSpritePackIndex < nSpriteIndex; ++nSpritePackIndex )
	{
		if ( spritesListIterator == rSpritesPack.sprites.end() )
		{
			break;
		}
		++spritesListIterator;
	}
	if ( spritesListIterator != rSpritesPack.sprites.end() )
	{
		CTPoint<int> pointPos( vPointPos.x, vPointPos.y ); 
		for ( SSpritesPack::SSprite::CSquaresList::const_iterator squareIterator = spritesListIterator->squares.begin(); squareIterator != spritesListIterator->squares.end(); ++squareIterator )
		{
			CTRect<int> actualSquareRect( static_cast<int>( squareIterator->vLeftTop.x ),
																		static_cast<int>( squareIterator->vLeftTop.y ),
																		static_cast<int>( squareIterator->vLeftTop.x + squareIterator->fSize ),
																		static_cast<int>( squareIterator->vLeftTop.y + squareIterator->fSize ) );
			{
				if ( ( pointPos.x >= actualSquareRect.minx ) &&
						 ( pointPos.y >= actualSquareRect.miny ) &&
						 ( pointPos.x < actualSquareRect.maxx ) &&
						 ( pointPos.y < actualSquareRect.maxy ) )
				{
					fZero2Border = squareIterator->fDepthLeft + ( ( ( squareIterator->fDepthRight - squareIterator->fDepthLeft ) * ( actualSquareRect.maxx - pointPos.x ) ) / ( 1.0f *( actualSquareRect.maxx - actualSquareRect.minx ) ) );
					fBorder2Point = ( -1 ) * ( fZero2Border + static_cast<float>( vPointPos.y ) ); 
					break;
				}
			}
		}
	}

	const float fZDepth = fZBias2*fZero2Border + fZBias*fBorder2Point;
	const CVec3 vScreenDepthPos = CVec3( vPointPos.x + vSpriteScreenPos.x, 
		                                   vPointPos.y + vSpriteScreenPos.y, vSpriteScreenPos.z + fZDepth );
	//
	SHMatrix matInverse;
	Invert( &matInverse, matTransform );
	CVec3 vPointPos3;
	matInverse.RotateHVector( &vPointPos3, vScreenDepthPos );
	vPointPos3 -= vSpritePos;
	//
	return vPointPos3;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec3 Get3DPosition( const CVec3 &vSpritePos, const CVec2 &vPointPos, const CVec2 &vCenter, const CImageAccessor &rImageAccessor, DWORD dwMinAlpha, IGFX *pGFX )
{
	CMatrixStack<4> mstack;
	mstack.Push( pGFX->GetViewportMatrix() );
	mstack.Push( pGFX->GetProjectionMatrix() );
	mstack.Push( pGFX->GetViewMatrix() );
	const SHMatrix &matTransform = mstack();
	//
	CVec3 vTempPos;
	matTransform.RotateHVector( &vTempPos, vSpritePos );
	const CVec3 vSpriteScreenPos = vTempPos;
	// determine z-bias to keep z-buffer happy (treat sprites as vertical)
	CVec3 vecZ1, vecZ2;
	matTransform.RotateHVector( &vecZ1, CVec3(0, 0, 0) );
	matTransform.RotateHVector( &vecZ2, CVec3(0, 0, 2.0f/FP_SQRT_3) );
	const float fZBias = vecZ2.z - vecZ1.z;
	matTransform.RotateHVector( &vecZ2, CVec3(-FP_SQRT_2, FP_SQRT_2, 0) );
	const float fZBias2 = vecZ2.z - vecZ1.z;
	
	// calculate distance from zero line to object's border and write values to:
	float fZero2Border = 0.0f;
	float fBorder2Point = -vPointPos.y;
	
	CTPoint<int> pointPos( static_cast<int>( vPointPos.x + vCenter.x ), static_cast<int>( vPointPos.y + vCenter.y ) );
	if ( ( pointPos.x >= 0 ) && ( pointPos.x < rImageAccessor->GetSizeX() ) )
	{
		for ( int nYIndex = ( rImageAccessor->GetSizeY() - 1 ); nYIndex >= 0; --nYIndex )
		{
			if ( rImageAccessor[nYIndex][pointPos.x].a >= dwMinAlpha )
			{
				fZero2Border = vCenter.y - static_cast<float>( nYIndex );
				fBorder2Point = static_cast<float>( nYIndex ) - vCenter.y - vPointPos.y;
				break;				
			}
		}
	}
	//
	const float fZDepth = fZBias2*fZero2Border + fZBias*fBorder2Point;
	const CVec3 vScreenDepthPos = CVec3( vPointPos.x + vSpriteScreenPos.x, 
		                                   vPointPos.y + vSpriteScreenPos.y, vSpriteScreenPos.z + fZDepth );
	//
	SHMatrix matInverse;
	Invert( &matInverse, matTransform );
	CVec3 vPointPos3;
	matInverse.RotateHVector( &vPointPos3, vScreenDepthPos );
	vPointPos3 -= vSpritePos;
	//
	return vPointPos3;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2 GetOrigin2DPosition( const CVec2 &vOrigin )
{
	IGFX *pGFX = GetSingleton<IGFX>();
	CMatrixStack<4> mstack;
	mstack.Push( pGFX->GetViewportMatrix() );
	mstack.Push( pGFX->GetProjectionMatrix() );
	mstack.Push( pGFX->GetViewMatrix() );
	const SHMatrix &matTransform = mstack();
	//
	CVec3 vOriginSreenPos, vSpriteScreenPos;
	matTransform.RotateHVector( &vOriginSreenPos, CVec3(-vOrigin.x, -vOrigin.y, 0) );
	matTransform.RotateHVector( &vSpriteScreenPos, VNULL3 );
	vOriginSreenPos -= vSpriteScreenPos;
	return CVec2( vOriginSreenPos.x, vOriginSreenPos.y );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int GetActionFromName( const std::string &szAnimName )
{
	std::string szName = szAnimName;
	NStr::ToLower( szName );
	//
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
	else if ( (szName == "default") || (szName == "effect") )
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
		
	//
	NI_ASSERT_T( 0, NStr::Format("Don't know animation \"%s\"", szAnimName.c_str()) );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::vector<std::string> CVectorOfStrings;
bool ComposeSingleSprite( const char *pszFileName, const char *pszResultingDir, const char *pszResName, bool bFence )
{
	CVectorOfStrings fileNameVector;
	std::vector<SAnimationDesc> animDescVector( 1 );
	SAnimationDesc &animDesc = animDescVector[0];
	animDesc.bCycled = false;
	animDesc.fSpeed = 0;
	const int nLastSprite = 1;
	CVec2 zeroPos( 0, 0 );
	if ( bFence )
	{
		IImageProcessor *pIP = GetImageProcessor();
		//�������� zeroPos, ��� ��� ������� ����� ���������� �����
		{
			CPtr<IDataStream> pStream = OpenFileStream( pszFileName, STREAM_ACCESS_READ );
			NI_ASSERT_T( pStream != 0, NStr::Format("Can't open file \"%s\" to compose sprite", pszFileName) );
			CPtr<IImage> pImage = pIP->LoadImage( pStream );
			NI_ASSERT( pImage != 0 );
			zeroPos.x = pImage->GetSizeX() / 2;
			zeroPos.y = pImage->GetSizeY() / 2;
		}
	}
	animDesc.nFrameTime = 1000;
	animDesc.ptFrameShift = zeroPos;
	animDesc.szName = "default";
	
	//��������� ������ directions
	fileNameVector.resize( nLastSprite );
	animDesc.dirs.resize( 1 );
	
	SAnimationDesc::SDirDesc &dirDesc = animDesc.dirs[ 0 ];
	dirDesc.ptFrameShift = zeroPos;
	dirDesc.frames.resize( 1 );
	dirDesc.frames[0] = 0;
	animDesc.frames[0] = zeroPos;
	fileNameVector[0] = pszFileName;
	
	if ( _access( pszFileName, 04 ) )
	{
		/*
		string str = "Can not access file  ";
		str += szSpriteFullName;
		AfxMessageBox( str.c_str() );
		*/
		return false;
	}
	
	if ( GetFileAttributes( pszFileName ) & FILE_ATTRIBUTE_DIRECTORY )
	{
		return false;		//����������
	}

	SSpriteAnimationFormat spriteAnimFmt;
	CPtr<IImage> pImage = BuildAnimations( &animDescVector, &spriteAnimFmt, fileNameVector, false );
	
	if ( !pImage )
	{
//		AfxMessageBox( "Composing images failed!" );
		return false;
	}

	//�������� .dds, .san �����
	std::string szTemp = pszResultingDir;
	szTemp += pszResName;
	SaveTexture8888( pImage, szTemp.c_str() );

	std::string szName = pszResName;
	szName += ".san";
	CPtr<IDataStorage> pSaveStorage = CreateStorage( pszResultingDir, STREAM_ACCESS_WRITE, STORAGE_TYPE_FILE );
	CPtr<IDataStream> pSaveSAFStream = pSaveStorage->CreateStream( szName.c_str(), STREAM_ACCESS_WRITE );
	if ( !pSaveSAFStream )
		return false;
	CPtr<IStructureSaver> pSS = CreateStructureSaver( pSaveSAFStream, IStructureSaver::WRITE );
	CSaverAccessor saver = pSS;
	saver.Add( 1, &spriteAnimFmt );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SaveCompressedTexture( IImage *pSrc, const char *pszDestFileName )
{
	CParentFrame *pFrame = g_frameManager.GetActiveFrame();
	IImageProcessor *pIP = GetSingleton<IImageProcessor>();
	CPtr<IImage> pImage = pIP->CreateGammaCorrection( pSrc, pFrame->GetBrightness(), pFrame->GetContrast(), pFrame->GetGamma() );
	CPtr<IDDSImage> pDDS;
	CPtr<IDataStream> pStream;
	std::string szName;

	//Compressed format
	szName = pszDestFileName;
	szName += "_c.dds";
	if ( pFrame->GetFrameType() == CFrameManager::E_MESH_FRAME )
		pDDS = pIP->Compress( pImage, ChooseBestFormat( pImage, COMPRESSION_DXT ) );
	else
		pDDS = pIP->Compress( pImage, (EGFXPixelFormat) pFrame->GetCompressedFormat() );
	pStream = CreateFileStream( szName.c_str(), STREAM_ACCESS_WRITE );
	pIP->SaveImageAsDDS( pStream, pDDS );
	
	//Low format
	szName = pszDestFileName;
	szName += "_l.dds";
	if ( pFrame->GetFrameType() == CFrameManager::E_MESH_FRAME )
		pDDS = pIP->Compress( pImage, ChooseBestFormat( pImage, COMPRESSION_LOW_QUALITY ) );
	else
		pDDS = pIP->Compress( pImage, (EGFXPixelFormat) pFrame->GetLowFormat() );
	pStream = CreateFileStream( szName.c_str(), STREAM_ACCESS_WRITE );
	pIP->SaveImageAsDDS( pStream, pDDS );
	
	//High format
	szName = pszDestFileName;
	szName += "_h.dds";
	pDDS = pIP->Compress( pImage, (EGFXPixelFormat) pFrame->GetHighFormat() );
	pStream = CreateFileStream( szName.c_str(), STREAM_ACCESS_WRITE );
	pIP->SaveImageAsDDS( pStream, pDDS );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SaveCompressedShadow( IImage *pSrc, const char *pszDestFileName )
{
	CParentFrame *pFrame = g_frameManager.GetActiveFrame();
	IImageProcessor *pIP = GetSingleton<IImageProcessor>();
	CPtr<IDDSImage> pDDS;
	CPtr<IDataStream> pStream;
	std::string szName;
	
	//Compressed format
	szName = pszDestFileName;
	szName += "_c.dds";
	pDDS = pIP->Compress( pSrc, (EGFXPixelFormat) pFrame->GetCompressedShadowFormat() );
	pStream = CreateFileStream( szName.c_str(), STREAM_ACCESS_WRITE );
	pIP->SaveImageAsDDS( pStream, pDDS );
	
	//Low format
	szName = pszDestFileName;
	szName += "_l.dds";
	pDDS = pIP->Compress( pSrc, (EGFXPixelFormat) pFrame->GetLowShadowFormat() );
	pStream = CreateFileStream( szName.c_str(), STREAM_ACCESS_WRITE );
	pIP->SaveImageAsDDS( pStream, pDDS );
	
	//High format
	szName = pszDestFileName;
	szName += "_h.dds";
	pDDS = pIP->Compress( pSrc, (EGFXPixelFormat) pFrame->GetHighShadowFormat() );
	pStream = CreateFileStream( szName.c_str(), STREAM_ACCESS_WRITE );
	pIP->SaveImageAsDDS( pStream, pDDS );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SaveTexture8888( IImage *pSrc, const char *pszDestFileName )
{
	IImageProcessor *pIP = GetSingleton<IImageProcessor>();
	std::string szName = pszDestFileName;
	szName += "_h.dds";
	CPtr<IDDSImage> pDDS = pIP->Compress( pSrc, GFXPF_ARGB8888 );
	CPtr<IDataStream> pStream = CreateFileStream( szName.c_str(), STREAM_ACCESS_WRITE );
	pIP->SaveImageAsDDS( pStream, pDDS );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SaveTexture8888( const char *pszSrc, const char *pszDestFileName )
{
	IImageProcessor *pIP = GetSingleton<IImageProcessor>();
	CPtr<IDataStream> pStream = OpenFileStream( pszSrc, STREAM_ACCESS_READ );
	NI_ASSERT( pStream != 0 );
	CPtr<IImage> pImage = pIP->LoadImage( pStream );
	SaveTexture8888( pImage, pszDestFileName );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
