#include "StdAfx.h"

#include "image_export.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "../Anim/Animation.h"
#include "../Formats/fmtAnimation.h"
#include "../Image/Image.h"
#include "../Image/ImageHelper.h"

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

// A structure file is three top chunks: 0 the object directory, 1 the main
// data, 2 the object content. MFC's saver wrote them 1, 0, 2 and every
// shipped .san has that order; the engine's Zig saver writes 0, 1, 2. The
// reader finds chunks by id, so both load the same, but the exporter keeps
// the shipped byte order so a re-save of a shipped file is identical. A chunk
// is [id][length]: a payload under 128 bytes takes the one byte length*2, a
// longer one the four bytes length*2+1.
bool MoveMainChunkFirst( const fs::path &file, SExportOutcome &outcome )
{
	std::string szBytes;
	{
		std::ifstream in( file, std::ios::binary );
		szBytes.assign( std::istreambuf_iterator<char>( in ), std::istreambuf_iterator<char>() );
	}
	std::vector<std::pair<unsigned char, std::string>> chunks;
	for ( size_t nPos = 0; nPos < szBytes.size(); )
	{
		const unsigned char id = (unsigned char)szBytes[nPos];
		size_t nHeader = 2, nLength = 0;
		if ( nPos + 2 > szBytes.size() )
			break;
		if ( ( (unsigned char)szBytes[nPos + 1] & 1 ) == 0 )
			nLength = (unsigned char)szBytes[nPos + 1] >> 1;
		else
		{
			if ( nPos + 5 > szBytes.size() )
				break;
			unsigned int dwLength = 0;
			memcpy( &dwLength, szBytes.data() + nPos + 1, 4 );
			nHeader = 5;
			nLength = dwLength >> 1;
		}
		if ( nPos + nHeader + nLength > szBytes.size() )
			break;
		chunks.emplace_back( id, szBytes.substr( nPos, nHeader + nLength ) );
		nPos += nHeader + nLength;
	}
	size_t nTotal = 0;
	for ( const auto &chunk : chunks )
		nTotal += chunk.second.size();
	if ( nTotal != szBytes.size() )
	{
		outcome.szError = "cannot order the chunks of " + file.string() + ": the saved structure is not a chunk sequence";
		return false;
	}
	std::stable_partition( chunks.begin(), chunks.end(), []( const std::pair<unsigned char, std::string> &chunk ) { return chunk.first == 1; } );
	std::string szOrdered;
	for ( const auto &chunk : chunks )
		szOrdered += chunk.second;
	std::ofstream out( file, std::ios::binary | std::ios::trunc );
	out.write( szOrdered.data(), std::streamsize( szOrdered.size() ) );
	if ( !out )
	{
		outcome.szError = "cannot write " + file.string();
		return false;
	}
	return true;
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

bool SaveCompressedTexture( const SExportContext &context, IImage *pSrc, const std::string &szName, const SGamma &gamma, EGFXPixelFormat compressedFormat, EGFXPixelFormat lowFormat, SExportOutcome &outcome )
{
	CPtr<IImage> pImage = GetImageProcessor()->CreateGammaCorrection( pSrc, gamma.fBrightness, gamma.fContrast, gamma.fGamma );
	return SaveDds( context, pImage, compressedFormat, szName + "_c.dds", outcome ) &&
	       SaveDds( context, pImage, lowFormat, szName + "_l.dds", outcome ) &&
	       SaveDds( context, pImage, GFXPF_ARGB8888, szName + "_h.dds", outcome );
}

bool SaveCompressedTexture( const SExportContext &context, IImage *pSrc, const std::string &szName, const SGamma &gamma, EGFXPixelFormat lowFormat, SExportOutcome &outcome )
{
	CPtr<IImage> pImage = GetImageProcessor()->CreateGammaCorrection( pSrc, gamma.fBrightness, gamma.fContrast, gamma.fGamma );
	return SaveDds( context, pImage, GFXPF_DXT5, szName + "_c.dds", outcome ) &&
	       SaveDds( context, pImage, lowFormat, szName + "_l.dds", outcome ) &&
	       SaveDds( context, pImage, GFXPF_ARGB8888, szName + "_h.dds", outcome );
}

bool SaveCompressedTexture( const SExportContext &context, IImage *pSrc, const std::string &szName, const SGamma &gamma, SExportOutcome &outcome )
{
	return SaveCompressedTexture( context, pSrc, szName, gamma, GFXPF_ARGB1555, outcome );
}

bool SaveCompressedTextureBestFormat( const SExportContext &context, IImage *pSrc, const std::string &szName, const SGamma &gamma, SExportOutcome &outcome )
{
	CPtr<IImage> pImage = GetImageProcessor()->CreateGammaCorrection( pSrc, gamma.fBrightness, gamma.fContrast, gamma.fGamma );
	return SaveDds( context, pImage, ChooseBestFormat( pImage, COMPRESSION_DXT ), szName + "_c.dds", outcome ) &&
	       SaveDds( context, pImage, ChooseBestFormat( pImage, COMPRESSION_LOW_QUALITY ), szName + "_l.dds", outcome ) &&
	       SaveDds( context, pImage, GFXPF_ARGB8888, szName + "_h.dds", outcome );
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
	if ( !MoveMainChunkFirst( fs::path( context.szStagingRoot ) / Slashed( szName ), outcome ) )
		return false;
	++outcome.nWritten;
	return true;
}

bool SaveShadowTexture( const SExportContext &context, IImage *pSrc, const std::string &szName, SExportOutcome &outcome )
{
	return SaveCompressedShadow( context, pSrc, szName, outcome );
}

bool SaveSpritesPack( const SExportContext &context, SSpritesPack &pack, const std::string &szName, SExportOutcome &outcome )
{
	DWORD dwSignature = SSpritesPack::SIGNATURE;
	{
		CPtr<IDataStream> pStream = CreateStaged( context, szName, outcome );
		if ( pStream == 0 )
			return false;
		CPtr<IStructureSaver> pSS = CreateStructureSaver( pStream, IStructureSaver::WRITE );
		CSaverAccessor saver = pSS;
		saver.Add( 1, &pack );
		saver.Add( 127, &dwSignature );
	}
	if ( !MoveMainChunkFirst( fs::path( context.szStagingRoot ) / Slashed( szName ), outcome ) )
		return false;
	++outcome.nWritten;
	return true;
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

bool ConvertAndSaveImageBestFormat( const SExportContext &context, const std::string &szSource, const std::string &szName, const SGamma &gamma, SExportOutcome &outcome )
{
	CPtr<IImage> pImage = LoadPicture( szSource, outcome );
	if ( pImage == 0 )
		return false;
	return SaveCompressedTextureBestFormat( context, pImage, szName, gamma, outcome );
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

CPtr<IImage> PadToPowerOfTwo( IImage *pSource )
{
	IImageProcessor *pIP = GetImageProcessor();
	const int nTempX = pSource->GetSizeX(), nTempY = pSource->GetSizeY();
	const int nSizeX = GetNextPow2( nTempX ), nSizeY = GetNextPow2( nTempY );
	RECT sourceRC = { 0, 0, nTempX, nTempY };
	CPtr<IImage> pDestImage = pIP->CreateImage( nSizeX, nSizeY );
	if ( pDestImage == 0 )
		return 0;
	pDestImage->CopyFrom( pSource, &sourceRC, 0, 0 );

	SColor *pDest = pDestImage->GetLFB();
	const SColor col( 0, 0xff, 0xff, 0xff );
	if ( nSizeX > nTempX )
	{
		for ( int y = 0; y < nTempY; y++ )
			for ( int x = nTempX; x < nSizeX; x++ )
				pDest[y * nSizeX + x] = col;
	}
	for ( int y = nTempY; y < nSizeY; y++ )
		for ( int x = 0; x < nSizeX; x++ )
			pDest[y * nSizeX + x] = col;
	return pDestImage;
}

bool ComposeImageToTexture( const SExportContext &context, const std::string &szSource, const std::string &szName, const SGamma &gamma, EGFXPixelFormat compressedFormat, EGFXPixelFormat lowFormat, bool bCorrect, SExportOutcome &outcome )
{
	CPtr<IImage> pSourceImage = LoadPicture( szSource, outcome );
	if ( pSourceImage == 0 )
		return false;
	CPtr<IImage> pDestImage = PadToPowerOfTwo( pSourceImage );
	if ( pDestImage == 0 )
	{
		outcome.szError = "cannot create the padded image for " + szSource;
		return false;
	}
	if ( bCorrect )
		return SaveCompressedTexture( context, pDestImage, szName, gamma, compressedFormat, lowFormat, outcome );
	return SaveDds( context, pDestImage, GFXPF_ARGB8888, szName + "_h.dds", outcome );
}

CTRect<float> GetImageSize( const std::string &szImage, SExportOutcome &outcome )
{
	CTRect<float> res( 0.0f, 0.0f, 0.0f, 0.0f );
	CPtr<IImage> pImage = LoadPicture( szImage, outcome );
	if ( pImage == 0 )
		return res;
	const float fSizeX = float( GetNextPow2( pImage->GetSizeX() ) );
	const float fSizeY = float( GetNextPow2( pImage->GetSizeY() ) );
	res.x1 = float( pImage->GetSizeX() );
	res.y1 = float( pImage->GetSizeY() );
	res.x2 = ( float( pImage->GetSizeX() ) + 0.5f ) / fSizeX;
	res.y2 = ( float( pImage->GetSizeY() ) + 0.5f ) / fSizeY;
	return res;
}

bool SaveTga( const SExportContext &context, IImage *pImage, const std::string &szName, SExportOutcome &outcome )
{
	{
		CPtr<IDataStream> pStream = CreateStaged( context, szName, outcome );
		if ( pStream == 0 )
			return false;
		if ( !GetImageProcessor()->SaveImageAsTGA( pStream, pImage ) )
		{
			outcome.szError = "cannot write " + szName;
			return false;
		}
	}
	++outcome.nWritten;
	return true;
}

bool SaveTgaFile( const std::string &szPath, IImage *pImage, SExportOutcome &outcome )
{
	const fs::path file( Slashed( szPath ) );
	std::string szDir = file.parent_path().string();
	if ( szDir.empty() || szDir.back() != '/' )
		szDir += '/';
	CPtr<IDataStorage> pStorage = CreateStorage( szDir.c_str(), STREAM_ACCESS_WRITE, STORAGE_TYPE_FILE );
	CPtr<IDataStream> pStream = pStorage != 0 ? pStorage->CreateStream( file.filename().string().c_str(), STREAM_ACCESS_WRITE ) : 0;
	if ( pStream == 0 || !GetImageProcessor()->SaveImageAsTGA( pStream, pImage ) )
	{
		outcome.szError = "cannot write " + file.string();
		return false;
	}
	return true;
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
	// The copy is written now: the up-to-date checks compare export times with
	// source times. Windows' CopyFile keeps the source's time, POSIX copies get
	// the current one; without this a copy older than the project never lets a
	// plain export skip on Windows.
	fs::last_write_time( target, fs::file_time_type::clock::now(), ec );
	++outcome.nWritten;
	return true;
}

}

}
