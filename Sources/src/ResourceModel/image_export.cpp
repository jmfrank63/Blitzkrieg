#include "StdAfx.h"

#include "image_export.h"

#include <filesystem>

#include "../Anim/Animation.h"
#include "../Formats/fmtAnimation.h"
#include "../Image/Image.h"

namespace NResourceModel
{

namespace NImageExport
{

namespace
{

namespace fs = std::filesystem;

const char kGammaConfigName[] = "gamma.cfg";

std::string Slashed( const std::string &szName )
{
	std::string szResult = szName;
	for ( char &c : szResult )
		if ( c == '\\' )
			c = '/';
	return szResult;
}

// The staged file szName, created through the engine's file storage as the
// frames' CreateFileStream was. Null with outcome.szError when it cannot be.
CPtr<IDataStream> CreateStaged( const SExportContext &context, const std::string &szName, SExportOutcome &outcome )
{
	const fs::path file = fs::path( context.szStagingRoot ) / Slashed( szName );
	std::error_code ec;
	fs::create_directories( file.parent_path(), ec );
	if ( ec )
	{
		outcome.szError = "cannot create the folder " + file.parent_path().string() + ": " + ec.message();
		return 0;
	}
	std::string szDir = file.parent_path().string();
	if ( szDir.empty() || szDir.back() != '/' )
		szDir += '/';
	CPtr<IDataStorage> pStorage = CreateStorage( szDir.c_str(), STREAM_ACCESS_WRITE, STORAGE_TYPE_FILE );
	CPtr<IDataStream> pStream = pStorage != 0 ? pStorage->CreateStream( file.filename().string().c_str(), STREAM_ACCESS_WRITE ) : 0;
	if ( pStream == 0 )
		outcome.szError = "cannot create " + file.string();
	return pStream;
}

// A picture the engine can read, or null with outcome.szError naming the
// path and why: missing, or present and not a readable TGA/PNG/BMP.
CPtr<IImage> LoadPicture( const std::string &szSource, SExportOutcome &outcome )
{
	const fs::path file( Slashed( szSource ) );
	std::error_code ec;
	if ( !fs::is_regular_file( file, ec ) )
	{
		outcome.szError = "cannot read the picture " + file.string() + ": the file is missing";
		return 0;
	}
	std::string szDir = file.parent_path().string();
	if ( szDir.empty() || szDir.back() != '/' )
		szDir += '/';
	CPtr<IDataStorage> pStorage = OpenStorage( szDir.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
	CPtr<IDataStream> pStream = pStorage != 0 ? pStorage->OpenStream( file.filename().string().c_str(), STREAM_ACCESS_READ ) : 0;
	if ( pStream == 0 )
	{
		outcome.szError = "cannot read the picture " + file.string() + ": the file cannot be opened";
		return 0;
	}
	CPtr<IImage> pImage = GetImageProcessor()->LoadImage( pStream );
	if ( pImage == 0 || pImage->GetSizeX() <= 0 || pImage->GetSizeY() <= 0 )
	{
		outcome.szError = "cannot read the picture " + file.string() + ": it is not a TGA, PNG or BMP the engine reads, or it is truncated";
		return 0;
	}
	return pImage;
}

// One DDS of pImage in format, written as szName.
bool SaveDds( const SExportContext &context, IImage *pImage, EGFXPixelFormat format, const std::string &szName, SExportOutcome &outcome )
{
	IImageProcessor *pIP = GetImageProcessor();
	CPtr<IDDSImage> pDDS = pIP->Compress( pImage, format );
	if ( pDDS == 0 )
	{
		outcome.szError = "the engine cannot compress " + szName + " to pixel format " + std::to_string( int( format ) );
		return false;
	}
	{
		CPtr<IDataStream> pStream = CreateStaged( context, szName, outcome );
		if ( pStream == 0 )
			return false;
		if ( !pIP->SaveImageAsDDS( pStream, pDDS ) )
		{
			outcome.szError = "cannot write " + szName;
			return false;
		}
	}
	++outcome.nWritten;
	return true;
}

// SaveCompressedTexture of a mine or trench frame: the picture after the
// gamma correction, as the frame's compressed, low and high formats.
bool SaveCompressedTexture( const SExportContext &context, IImage *pSrc, const std::string &szName, const SGamma &gamma, SExportOutcome &outcome )
{
	CPtr<IImage> pImage = GetImageProcessor()->CreateGammaCorrection( pSrc, gamma.fBrightness, gamma.fContrast, gamma.fGamma );
	return SaveDds( context, pImage, GFXPF_DXT5, szName + "_c.dds", outcome ) &&
	       SaveDds( context, pImage, GFXPF_ARGB1555, szName + "_l.dds", outcome ) &&
	       SaveDds( context, pImage, GFXPF_ARGB8888, szName + "_h.dds", outcome );
}

// SaveCompressedShadow: the shadow formats of the frame base class, no gamma.
bool SaveCompressedShadow( const SExportContext &context, IImage *pSrc, const std::string &szName, SExportOutcome &outcome )
{
	return SaveDds( context, pSrc, GFXPF_DXT5, szName + "_c.dds", outcome ) &&
	       SaveDds( context, pSrc, GFXPF_ARGB4444, szName + "_l.dds", outcome ) &&
	       SaveDds( context, pSrc, GFXPF_ARGB8888, szName + "_h.dds", outcome );
}

// BuildAnimations for ComposeSingleObject's one animation, one direction,
// one frame, with the zero position at the origin and no depth: the source
// picture packed by the engine's ComposeImages, and the animation format
// describing where the picture lies in the result. The animation is
// "default", which GetActionFromName maps to ANIMATION_IDLE (0).
CPtr<IImage> BuildSingleFrame( IImage *pSource, SSpriteAnimationFormat &animations, SExportOutcome &outcome )
{
	IImage *pImages[1] = { pSource };
	RECT rect = {}, rectMain = {};
	CPtr<IImage> pImage = GetImageProcessor()->ComposeImages( pImages, &rect, &rectMain, 1 );
	if ( pImage == 0 )
	{
		outcome.szError = "Composing images failed!";
		return 0;
	}
	animations.animations.resize( 1 );
	SSpriteAnimationFormat::SSpriteAnimation &animation = animations.animations[0];
	animation.fSpeed = 0;
	animation.bCycled = false;
	animation.nFrameTime = 1000;
	animation.dirs.resize( 1 );
	animation.dirs[0].frames.assign( 1, 0 );
	animation.rects.resize( 1 );
	SSpriteRect &sprite = animation.rects[0];
	sprite.maps.x1 = ( float( rect.left ) + 0.5f ) / float( pImage->GetSizeX() );
	sprite.maps.x2 = ( float( rect.right ) + 0.5f ) / float( pImage->GetSizeX() );
	sprite.maps.y1 = ( float( rect.top ) + 0.5f ) / float( pImage->GetSizeY() );
	sprite.maps.y2 = ( float( rect.bottom ) + 0.5f ) / float( pImage->GetSizeY() );
	sprite.rect.Set( rect.left - rectMain.left, rect.top - rectMain.top, rect.right - rectMain.left, rect.bottom - rectMain.top );
	sprite.fDepthLeft = 0.0f;
	sprite.fDepthRight = 0.0f;
	return pImage;
}

bool SaveAnimation( const SExportContext &context, SSpriteAnimationFormat &animations, const std::string &szName, SExportOutcome &outcome )
{
	{
		CPtr<IDataStream> pStream = CreateStaged( context, szName, outcome );
		if ( pStream == 0 )
			return false;
		CPtr<IStructureSaver> pSS = CreateStructureSaver( pStream, IStructureSaver::WRITE );
		CSaverAccessor saver = pSS;
		saver.Add( 1, &animations );
	}
	++outcome.nWritten;
	return true;
}

}

SGamma ReadGammaConfig( const std::string &szProjectDirectory )
{
	fs::path directory( Slashed( szProjectDirectory ) );
	if ( directory.filename().empty() )
		directory = directory.parent_path();
	for ( ; !directory.empty(); directory = directory.parent_path() )
	{
		std::error_code ec;
		if ( fs::is_regular_file( directory / kGammaConfigName, ec ) )
		{
			CPtr<IDataStorage> pStorage = OpenStorage( ( directory.string() + "/" ).c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
			CPtr<IDataStream> pStream = pStorage != 0 ? pStorage->OpenStream( kGammaConfigName, STREAM_ACCESS_READ ) : 0;
			CPtr<IDataTree> pDT = pStream != 0 ? CreateDataTreeSaver( pStream, IDataTree::READ ) : 0;
			SGamma gamma;
			if ( pDT != 0 )
			{
				CTreeAccessor saver = pDT;
				saver.Add( "Brightness", &gamma.fBrightness );
				saver.Add( "Contrast", &gamma.fContrast );
				saver.Add( "Gamma", &gamma.fGamma );
			}
			return gamma;
		}
		if ( directory == directory.root_path() )
			break;
	}
	return SGamma();
}

bool ConvertAndSaveImage( const SExportContext &context, const std::string &szSource, const std::string &szName, const SGamma &gamma, SExportOutcome &outcome )
{
	CPtr<IImage> pImage = LoadPicture( szSource, outcome );
	if ( pImage == 0 )
		return false;
	return SaveCompressedTexture( context, pImage, szName, gamma, outcome );
}

bool ComposeSingleObject( const SExportContext &context, const std::string &szSprite, const std::string &szShadow, const std::string &szName, const SGamma &gamma, SExportOutcome &outcome )
{
	CPtr<IImage> pSprite = LoadPicture( szSprite, outcome );
	if ( pSprite == 0 )
		return false;
	CPtr<IImage> pShadow = LoadPicture( szShadow, outcome );
	if ( pShadow == 0 )
		return false;
	if ( pSprite->GetSizeX() != pShadow->GetSizeX() || pSprite->GetSizeY() != pShadow->GetSizeY() )
	{
		outcome.szError = "The size of sprite does not equal the size of shadow: " + szSprite + " is " + std::to_string( pSprite->GetSizeX() ) + "x" +
		                  std::to_string( pSprite->GetSizeY() ) + ", " + szShadow + " is " + std::to_string( pShadow->GetSizeX() ) + "x" + std::to_string( pShadow->GetSizeY() );
		return false;
	}

	SSpriteAnimationFormat spriteAnimations;
	CPtr<IImage> pPacked = BuildSingleFrame( pSprite, spriteAnimations, outcome );
	if ( pPacked == 0 )
		return false;
	if ( !SaveCompressedTexture( context, pPacked, szName, gamma, outcome ) || !SaveAnimation( context, spriteAnimations, szName + ".san", outcome ) )
		return false;

	// The shadow keeps its colour channel cleared and takes its alpha from
	// the inverse of the sharpened sprite alpha.
	CPtr<IImage> pInverseSprite = pSprite->Duplicate();
	pInverseSprite->SharpenAlpha( 128 );
	pInverseSprite->InvertAlpha();
	RECT rc = { 0, 0, pInverseSprite->GetSizeX(), pInverseSprite->GetSizeY() };
	pShadow->ModulateAlphaFrom( pInverseSprite, &rc, 0, 0 );
	pShadow->SetColor( DWORD( 0 ) );

	SSpriteAnimationFormat shadowAnimations;
	CPtr<IImage> pPackedShadow = BuildSingleFrame( pShadow, shadowAnimations, outcome );
	if ( pPackedShadow == 0 )
		return false;
	return SaveCompressedShadow( context, pPackedShadow, szName + "s", outcome ) && SaveAnimation( context, shadowAnimations, szName + "s.san", outcome );
}

bool CopyFileInto( const SExportContext &context, const std::string &szSource, const std::string &szName, SExportOutcome &outcome )
{
	const fs::path source( Slashed( szSource ) );
	const fs::path target = fs::path( context.szStagingRoot ) / Slashed( szName );
	std::error_code ec;
	if ( !fs::is_regular_file( source, ec ) )
	{
		outcome.szError = "Cannot copy file " + source.string() + ": the file is missing";
		return false;
	}
	fs::create_directories( target.parent_path(), ec );
	fs::copy_file( source, target, fs::copy_options::overwrite_existing, ec );
	if ( ec )
	{
		outcome.szError = "Cannot copy file " + source.string() + " to " + target.string() + ": " + ec.message();
		return false;
	}
	++outcome.nWritten;
	return true;
}

}

}
