#include "StdAfx.h"
#include <io.h>

#include "editor.h"
#include "SpriteCompose.h"
#include "BuildCompose.h"
#include "frames.h"
#include "ParentFrame.h"

using std::vector;
using std::string;
typedef vector<string> CVectorOfStrings;

bool ComposeSingleObject( const char *pszObjFileName, const char *pszShadowFileName, const char *pszResultFileName, const CVec2 &zeroPos )
{
	IImageProcessor *pIP = GetImageProcessor();
	string szDir = pszResultFileName;
	string szShortFileName = szDir.substr( szDir.rfind('\\') + 1 );
	szDir = szDir.substr( 0, szDir.rfind('\\') + 1 );
	string szTempFileName;
	
	CVectorOfStrings fileNameVector;
	CVectorOfStrings invalidNameVector;
	vector<SAnimationDesc> animDescVector( 1 );
	SAnimationDesc &animDesc = animDescVector[0];
	animDesc.bCycled = false;
	animDesc.fSpeed = 0;
	const int nLastSprite = 1;
	animDesc.nFrameTime = 1000;
	animDesc.ptFrameShift = zeroPos;
	animDesc.szName = "default";
	
	fileNameVector.resize( nLastSprite );
	animDesc.dirs.resize( 1 );
	
	SAnimationDesc::SDirDesc &dirDesc = animDesc.dirs[ 0 ];
	dirDesc.ptFrameShift = zeroPos;
	dirDesc.frames.resize( 1 );
	dirDesc.frames[0] = 0;
	animDesc.frames[0] = zeroPos;
	fileNameVector[0] = pszObjFileName;
	
	if ( _access( pszObjFileName, 04 ) )
	{
/*
		string str = "Can not access sprite file  ";
		str += pszObjFileName;
		AfxMessageBox( str.c_str() );
*/
		return false;
	}

	SSpriteAnimationFormat spriteAnimFmt;
	CPtr<IImage> pImage = BuildAnimations( &animDescVector, &spriteAnimFmt, fileNameVector );
	
	if ( !pImage )
	{
		AfxMessageBox( "Composing images failed!" );
	}
	
	CPtr<IDataStorage> pSaveStorage = CreateStorage( szDir.c_str(), STREAM_ACCESS_WRITE, STORAGE_TYPE_FILE );
	SaveCompressedTexture( pImage, szShortFileName.c_str() );
	
	szTempFileName = szShortFileName;
	szTempFileName += ".san";
	CPtr<IDataStream> pSaveSAFStream = pSaveStorage->CreateStream( szTempFileName.c_str(), STREAM_ACCESS_WRITE );
	if ( !pSaveSAFStream )
		return false;
	CPtr<IStructureSaver> pSS = CreateStructureSaver( pSaveSAFStream, IStructureSaver::WRITE );
	CSaverAccessor saver = pSS;
	saver.Add( 1, &spriteAnimFmt );


	if ( pszShadowFileName[0] == '0' )
		return false;

	if ( _access( pszShadowFileName, 04 ) )
		return false;

	{
		CPtr<IDataStream> pBuildStream = OpenFileStream( pszObjFileName, STREAM_ACCESS_READ );
		if ( pBuildStream == 0 )
			return false;
		CPtr<IImage> pSpriteImage = pIP->LoadImage( pBuildStream );
		if ( pSpriteImage == 0 )
			return false;
		CPtr<IImage> pInverseSprite = pSpriteImage->Duplicate();
		pInverseSprite->SharpenAlpha( 128 );
		pInverseSprite->InvertAlpha();

		CPtr<IDataStream> pShadowStream = OpenFileStream( pszShadowFileName, STREAM_ACCESS_READ );
		if ( pShadowStream == 0 )
			return false;
		CPtr<IImage> pShadowImage = pIP->LoadImage( pShadowStream );
		if ( pShadowImage == 0 )
			return false;
		if ( pInverseSprite->GetSizeX() != pShadowImage->GetSizeX() || pInverseSprite->GetSizeY() != pShadowImage->GetSizeY() )
		{
			string szErr = "The size of sprite does not equal the size of shadow: ";
			szErr += pszObjFileName;
			szErr += ",  ";
			szErr += pszShadowFileName;

			NI_ASSERT_T( 0, szErr.c_str() );
			return false;
		}
		RECT rc;
		rc.left = 0;
		rc.top = 0;
		rc.right = pInverseSprite->GetSizeX();
		rc.bottom = pInverseSprite->GetSizeY();
		pShadowImage->ModulateAlphaFrom( pInverseSprite, &rc, 0, 0 );
		pShadowImage->SetColor( DWORD(0) );
		
		szTempFileName = theApp.GetEditorTempDir();
		szTempFileName += "shadow.tga";
		CPtr<IDataStream> pSaveShadowStream = OpenFileStream( szTempFileName.c_str(), STREAM_ACCESS_WRITE );
		pIP->SaveImageAsTGA( pSaveShadowStream, pShadowImage );
	}
	
	dirDesc.frames[0] = 1;
	animDesc.frames[0] = zeroPos;
	fileNameVector[0] = szTempFileName.c_str();
	
	pImage = BuildAnimations( &animDescVector, &spriteAnimFmt, fileNameVector );
	if ( !pImage )
	{
		AfxMessageBox( "Composing images failed!" );
	}
	
	szTempFileName = szShortFileName;
	szTempFileName += "s";
	SaveCompressedShadow( pImage, szTempFileName.c_str() );
	
	szTempFileName = szShortFileName;
	szTempFileName += "s.san";
	pSaveSAFStream = pSaveStorage->CreateStream( szTempFileName.c_str(), STREAM_ACCESS_WRITE );
	if ( !pSaveSAFStream )
		return false;
	pSS = CreateStructureSaver( pSaveSAFStream, IStructureSaver::WRITE );
	saver = pSS;
	saver.Add( 1, &spriteAnimFmt );
	
	return true;
}

bool ComposeSingleObjectPack( const char *pszObjFileName, const char *pszShadowFileName, const char *pszResultFileName, const CVec2 &zeroPos, const CArray2D<BYTE> &pass, const CVec2 &vOrigin )
{
	CSpritesPackBuilder::SPackParameter param;
	IImageProcessor *pIP = GetImageProcessor();
	
	CPtr<IDataStream> pBuildStream = OpenFileStream( pszObjFileName, STREAM_ACCESS_READ );
	if ( pBuildStream == 0 )
		return false;
	CPtr<IImage> pSpriteImage = pIP->LoadImage( pBuildStream );
	if ( pSpriteImage == 0 )
		return false;
	
	CVec2 vlockedTilesCenter = GetOrigin2DPosition( vOrigin );
	
	param.pImage = pSpriteImage;
	param.center = CTPoint<int>( zeroPos.x, zeroPos.y );
	param.lockedTiles = pass;
	param.lockedTilesCenter = CTPoint<int>( vlockedTilesCenter.x, vlockedTilesCenter.y );
	
	if ( !BuildSpritesPack( param, pszResultFileName ) )
		return false;
	string szTGAFile = pszResultFileName;
	szTGAFile += ".tga";

	{
		CPtr<IDataStream> pImageStream = OpenFileStream( szTGAFile.c_str(), STREAM_ACCESS_READ );
		NI_ASSERT( pImageStream != 0 );
		CPtr<IImage> pImage = pIP->LoadImage( pImageStream );
		NI_ASSERT( pImage != 0 );
		pImageStream = 0;
		SaveCompressedTexture( pImage, pszResultFileName );
		remove( szTGAFile.c_str() );
	}

	CPtr<IImage> pInverseSprite = pSpriteImage->Duplicate();
	pInverseSprite->SharpenAlpha( 128 );
	pInverseSprite->InvertAlpha();

	CPtr<IDataStream> pShadowStream = OpenFileStream( pszShadowFileName, STREAM_ACCESS_READ );
	if ( pShadowStream == 0 )
		return false;
	CPtr<IImage> pShadowImage = pIP->LoadImage( pShadowStream );
	if ( pShadowImage == 0 )
		return false;
	if ( pInverseSprite->GetSizeX() != pShadowImage->GetSizeX() || pInverseSprite->GetSizeY() != pShadowImage->GetSizeY() )
	{
		string szErr = "The size of sprite does not equal the size of shadow: ";
		szErr += pszObjFileName;
		szErr += ",  ";
		szErr += pszShadowFileName;

		NI_ASSERT_T( 0, szErr.c_str() );
		return false;
	}
	RECT rc;
	rc.left = 0;
	rc.top = 0;
	rc.right = pInverseSprite->GetSizeX();
	rc.bottom = pInverseSprite->GetSizeY();
	pShadowImage->ModulateAlphaFrom( pInverseSprite, &rc, 0, 0 );
	pShadowImage->SetColor( DWORD(0) );
	
	param.pImage = pShadowImage;
	param.center = CTPoint<int>( zeroPos.x, zeroPos.y );
	
  param.lockedTiles.Clear();
	param.lockedTilesCenter.x = 0;
	param.lockedTilesCenter.y = 0;
	
	string szRes = pszResultFileName;
	szRes += "s";
	if( !BuildSpritesPack( param, szRes.c_str() ) )
		return false;
	szTGAFile = szRes;
	szTGAFile += ".tga";
	
	{
		CPtr<IDataStream> pImageStream = OpenFileStream( szTGAFile.c_str(), STREAM_ACCESS_READ );
		NI_ASSERT( pImageStream != 0 );
		CPtr<IImage> pImage = pIP->LoadImage( pImageStream );
		NI_ASSERT( pImage != 0 );
		pImageStream = 0;
		SaveCompressedShadow( pImage, szRes.c_str() );
		remove( szTGAFile.c_str() );
	}
	return true;
}

bool ComposeImageToTexture( const char *pszSource, const char *pszResult, bool bCorrect )
{
	CPtr<IDataStream> pSourceStream = OpenFileStream( pszSource, STREAM_ACCESS_READ );
	if ( !pSourceStream )
		return false;
	
	CPtr<IImageProcessor> pIP = GetImageProcessor();
	CPtr<IImage> pSourceImage = pIP->LoadImage( pSourceStream );
	if ( !pSourceImage )
		return false;

	int nTempX = pSourceImage->GetSizeX(), nTempY = pSourceImage->GetSizeY();
	RECT sourceRC;
	sourceRC.left = 0;
	sourceRC.top = 0;
	sourceRC.right = nTempX;
	sourceRC.bottom = nTempY;
	int nSizeX = GetNextPow2( nTempX );
	int nSizeY = GetNextPow2( nTempY );
	CPtr<IImage> pDestImage = pIP->CreateImage( nSizeX, nSizeY );
	if ( !pDestImage )
		return false;
	pDestImage->CopyFrom( pSourceImage, &sourceRC, 0, 0 );

	SColor *pDest = pDestImage->GetLFB();
	SColor col( 0, 0xff, 0xff, 0xff );
	if ( nSizeX > nTempX )
	{
		for ( int y=0; y<nTempY; y++ )
		{
			for ( int x=nTempX; x<nSizeX; x++ )
			{
				pDest[y*nSizeX + x] = col;
			}
		}
	}

	for ( int y=nTempY; y<nSizeY; y++ )
	{
		for ( int x=0; x<nSizeX; x++ )
		{
			pDest[y*nSizeX + x] = col;
		}
	}

/*
	pDestImage->Set( SColor(0, 0xff, 0xff, 0xff ) );
	const SColor *pSrc = pSourceImage->GetLFB();
	for ( int y=0; y<nTempY; y++ )
	{
		for ( int x=0; x<nTempX; x++ )
			pDest[y*nSizeX + x] = pSrc[y*nTempX + x];
	}
*/

	if ( bCorrect )
	{
		SaveCompressedTexture( pDestImage, pszResult );
	}
	else
	{
		SaveTexture8888( pDestImage, pszResult );
	}

	return true;
}

CTRect<float> GetImageSize( const char *pszImageFile )
{
	CTRect<float> res( 0.0f, 0.0f, 0.0f, 0.0f );
	
	IDataStream *pStream = OpenFileStream( pszImageFile, STREAM_ACCESS_READ );
	if ( !pStream )
		return res;
	
	IImageProcessor *pIP = GetImageProcessor();
	IImage *pImage = pIP->LoadImage( pStream );
	if ( !pImage )
		return res;
	
	float fSizeX = GetNextPow2( pImage->GetSizeX() );
	float fSizeY = GetNextPow2( pImage->GetSizeY() );

	res.x1 = pImage->GetSizeX();
	res.y1 = pImage->GetSizeY();
	res.x2 = (float) ( pImage->GetSizeX() + 0.5f ) / fSizeX;
	res.y2 = (float) ( pImage->GetSizeY() + 0.5f ) / fSizeY;
	return res;
}
