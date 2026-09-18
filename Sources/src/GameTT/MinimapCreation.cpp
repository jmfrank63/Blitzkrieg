#include "StdAfx.h"

#include "MinimapCreation.h"

#include "../GFX/GFX.H"
#include "../GFX/GFXHelper.h"
#include "../Image/Image.h"
#include "../Main/ScenarioTracker.h"
#include "../Platform/Paths.h"
#include "../RandomMapGen/IB_Types.h"
#include "../RandomMapGen/MiniMap_Types.h"
#include "../RandomMapGen/MapInfo_Types.h"
#include "../Misc/HPTimer.h"

#include <cctype>
#include <cstdio>
#include <filesystem>

namespace
{
	// The map as the game reads it: the .xml when it is newer than the .bzm.
	struct SMapSource
	{
		std::string szStreamName;
		bool bXML;
		SStorageElementStats stats;
	};
	bool GetMapSource( const std::string &szTerrainName, SMapSource *pSource )
	{
		IDataStorage *pStorage = GetSingleton<IDataStorage>();
		SStorageElementStats statsXML, statsBZM;
		Zero( statsXML );
		Zero( statsBZM );
		pStorage->GetStreamStats( ( szTerrainName + ".xml" ).c_str(), &statsXML );
		pStorage->GetStreamStats( ( szTerrainName + ".bzm" ).c_str(), &statsBZM );
		pSource->bXML = statsXML.mtime > statsBZM.mtime;
		pSource->stats = pSource->bXML ? statsXML : statsBZM;
		pSource->szStreamName = szTerrainName + ( pSource->bXML ? ".xml" : ".bzm" );
		return !( pSource->stats.mtime == 0 );
	}
	// A hash of the map file (FNV-1a, 64 bit), so an edited map gets a new
	// picture however its timestamps moved.
	std::string GetSourceRevision( const std::string &szStreamName )
	{
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szStreamName.c_str(), STREAM_ACCESS_READ );
		if ( !pStream )
			return std::string();
		unsigned long long hash = 14695981039346656037ULL;
		unsigned char buffer[65536];
		for ( int nRead = pStream->Read( buffer, sizeof( buffer ) ); nRead > 0; nRead = pStream->Read( buffer, sizeof( buffer ) ) )
		{
			for ( int i = 0; i < nRead; ++i )
			{
				hash ^= buffer[i];
				hash *= 1099511628211ULL;
			}
		}
		char szRevision[17];
		std::snprintf( szRevision, sizeof( szRevision ), "%016llx", hash );
		return szRevision;
	}
	// A full path with backslashes, as the storage layer takes them; lowercase
	// below the cache root, which is ours to name.
	std::string GetCachePath( const std::string &szTerrainName, const std::string &szRevision )
	{
		std::string szMod;
		for ( const char c : GetSingleton<IUserProfile>()->GetMOD() )
		{
			const unsigned char u = static_cast<unsigned char>( c );
			if ( c == '\\' || c == '/' )
				continue;
			szMod += ( isalnum( u ) || c == '-' || c == '_' || c == '.' ) ? c : '_';
		}
		if ( szMod.empty() || szMod == "." || szMod == ".." )
			szMod = "base";
		std::string szPath = NPlatform::Paths::CacheRoot() + "\\minimaps\\" + szMod + "\\" + szTerrainName + "_" + szRevision + GetUltraDDSImageExtention();
		for ( int i = 0; i < szPath.size(); ++i )
		{
			if ( szPath[i] == '/' )
				szPath[i] = '\\';
		}
		const size_t nRelative = NPlatform::Paths::CacheRoot().size();
		std::string szRelative = szPath.substr( nRelative );
		NStr::ToLower( szRelative );
		return szPath.substr( 0, nRelative ) + szRelative;
	}
	bool GetFileStats( const std::string &szPath, SStorageElementStats *pStats )
	{
		const size_t nSlash = szPath.rfind( '\\' );
		if ( nSlash == std::string::npos )
			return false;
		CPtr<IDataStorage> pDirectory = OpenStorage( szPath.substr( 0, nSlash + 1 ).c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
		return pDirectory && pDirectory->GetStreamStats( szPath.substr( nSlash + 1 ).c_str(), pStats );
	}
	std::string HostPath( const std::string &szPath )
	{
		std::string szHostPath = szPath;
#if !defined(_WIN32)
		for ( int i = 0; i < szHostPath.size(); ++i )
		{
			if ( szHostPath[i] == '\\' )
				szHostPath[i] = '/';
		}
#endif
		return szHostPath;
	}
	// Removes this map's pictures of other revisions, and the one an earlier
	// build kept without a revision, so an often edited map does not pile up
	// megabyte files. Only "<map>_<16 hex digits>_u.dds" and "<map>_u.dds".
	void RemoveOtherRevisions( const std::string &szCachePath )
	{
		const std::filesystem::path path( HostPath( szCachePath ) );
		const std::string szKeep = path.filename().string();
		const std::string szExtension = GetUltraDDSImageExtention();
		const size_t nRevisionAndExtension = 16 + szExtension.size();
		if ( szKeep.size() <= nRevisionAndExtension + 1 )
			return;
		const std::string szPrefix = szKeep.substr( 0, szKeep.size() - nRevisionAndExtension );
		std::error_code error;
		std::filesystem::remove( path.parent_path() / ( szPrefix.substr( 0, szPrefix.size() - 1 ) + szExtension ), error );
		error.clear();
		std::vector<std::filesystem::path> others;
		for ( std::filesystem::directory_iterator it( path.parent_path(), error ), end; !error && it != end; it.increment( error ) )
		{
			const std::string szName = it->path().filename().string();
			if ( szName == szKeep || szName.size() != szKeep.size() || szName.compare( 0, szPrefix.size(), szPrefix ) != 0 ||
				szName.compare( szName.size() - szExtension.size(), szExtension.size(), szExtension ) != 0 )
				continue;
			if ( szName.substr( szPrefix.size(), 16 ).find_first_not_of( "0123456789abcdef" ) == std::string::npos )
				others.push_back( it->path() );
		}
		for ( int i = 0; i < others.size(); ++i )
		{
			error.clear();
			std::filesystem::remove( others[i], error );
		}
	}
	// A cached picture is used only when it is whole: a 512x512 32-bit DDS with
	// all of its pixel data. A truncated or damaged one would otherwise stand in
	// for the shipped picture it replaces and fail to load.
	bool IsValidCachedImage( const std::string &szPath )
	{
		IImageProcessor *pImageProcessor = GetSingleton<IImageProcessor>();
		CPtr<IDataStream> pStream = OpenFileStream( szPath, STREAM_ACCESS_READ );
		if ( !pImageProcessor || !pStream )
			return false;
		const int IMAGE_SIZE = 0x200;
		const int DDS_HEADER_BYTES = 128;
		if ( pStream->GetSize() < DDS_HEADER_BYTES + IMAGE_SIZE * IMAGE_SIZE * 4 )
			return false;
		CPtr<IDDSImage> pDDSImage = pImageProcessor->LoadDDSImage( pStream );
		return pDDSImage && pDDSImage->GetGFXFormat() == GFXPF_ARGB8888 &&
			pDDSImage->GetSizeX( 0 ) == IMAGE_SIZE && pDDSImage->GetSizeY( 0 ) == IMAGE_SIZE;
	}
	// A DDS file outside the data storage as a texture. The texture manager
	// only finds names inside it, so the cached picture is decoded here.
	CPtr<IGFXTexture> LoadTextureFromFile( const std::string &szPath )
	{
		IImageProcessor *pImageProcessor = GetSingleton<IImageProcessor>();
		IGFX *pGFX = GetSingleton<IGFX>();
		if ( !pImageProcessor || !pGFX )
			return 0;
		CPtr<IDataStream> pStream = OpenFileStream( szPath, STREAM_ACCESS_READ );
		if ( !pStream )
			return 0;
		CPtr<IDDSImage> pDDSImage = pImageProcessor->LoadDDSImage( pStream );
		CPtr<IImage> pImage = pDDSImage ? pImageProcessor->Decompress( pDDSImage ) : 0;
		if ( !pImage || pImage->GetSizeX() <= 0 || pImage->GetSizeY() <= 0 )
			return 0;
		CPtr<IGFXTexture> pTexture = pGFX->CreateTexture( pImage->GetSizeX(), pImage->GetSizeY(), 1, GFXPF_ARGB8888, GFXD_STATIC );
		if ( !pTexture )
			return 0;
		{
			CTextureLock<SGFXColor8888> textureLock( pTexture, 0 );
			if ( textureLock.GetSizeY() != pImage->GetSizeY() || textureLock.GetSizeX() != pImage->GetSizeX() )
				return 0;
			const SColor *pPixels = pImage->GetLFB();
			for ( int nY = 0; nY < textureLock.GetSizeY(); ++nY )
				memcpy( textureLock[nY], pPixels + nY * pImage->GetSizeX(), pImage->GetSizeX() * sizeof( SColor ) );
		}
		return pTexture;
	}
}

bool CMinimapCreation::IsShippedImageStale( const std::string &szTerrainName, const std::string &szImageName )
{
	SMapSource source;
	if ( !GetMapSource( szTerrainName, &source ) )
		return false;
	SStorageElementStats statsShipped;
	Zero( statsShipped );
	GetSingleton<IDataStorage>()->GetStreamStats( ( szImageName + GetDDSImageExtention( COMPRESSION_DXT ) ).c_str(), &statsShipped );
	return source.stats.mtime > statsShipped.mtime;
}

std::string CMinimapCreation::GetCachedMapImage( const std::string &szTerrainName )
{
	SMapSource source;
	if ( !GetMapSource( szTerrainName, &source ) )
		return std::string();
	const std::string szRevision = GetSourceRevision( source.szStreamName );
	if ( szRevision.empty() )
		return std::string();
	const std::string szCachePath = GetCachePath( szTerrainName, szRevision );
	SStorageElementStats statsCache;
	Zero( statsCache );
	if ( GetFileStats( szCachePath, &statsCache ) )
	{
		if ( IsValidCachedImage( szCachePath ) )
			return szCachePath;
		// Rebuilt rather than trusted; while that fails, callers fall back to
		// the shipped picture.
		std::error_code error;
		std::filesystem::remove( HostPath( szCachePath ), error );
		if ( getenv( "BK_UI_TRACE" ) )
			fprintf( stderr, "BK_UI_TRACE: minimap cache \"%s\" unreadable, rebuilding\n", szCachePath.c_str() );
	}

	NHPTimer::STime timeStart = 0;
	NHPTimer::GetTime( &timeStart );
	bool bCreated = false;
	try
	{
		CMapInfo mapInfo;
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( source.szStreamName.c_str(), STREAM_ACCESS_READ );
		if ( pStream )
		{
			if ( source.bXML )
			{
				CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
				saver.AddTypedSuper( &mapInfo );
			}
			else
			{
				CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
				CSaverAccessor saver = pSaver;
				saver.Add( 1, &mapInfo );
			}
			if ( mapInfo.IsValid() )
			{
				mapInfo.UnpackFrameIndices();
				CRMImageCreateParameterList imageCreateParameterList;
				imageCreateParameterList.push_back( SRMImageCreateParameter( szCachePath, CTPoint<int>( 0x200, 0x200 ), true, false, 0.0f, 0.0f, 0.0f, true ) );
				bCreated = mapInfo.CreateMiniMapImage( imageCreateParameterList ) && IsValidCachedImage( szCachePath );
			}
		}
	}
	catch ( ... )
	{
		bCreated = false;
	}
	if ( bCreated )
		RemoveOtherRevisions( szCachePath );
	if ( getenv( "BK_UI_TRACE" ) )
		fprintf( stderr, "BK_UI_TRACE: minimap cache \"%s\" -> \"%s\" generated=%d in %.0f ms\n",
			source.szStreamName.c_str(), szCachePath.c_str(), bCreated ? 1 : 0, NHPTimer::GetTimePassed( &timeStart ) * 1000.0 );
	return bCreated ? szCachePath : std::string();
}

CPtr<IGFXTexture> CMinimapCreation::GetMapImageTexture( const std::string &szTerrainName, const std::string &szImageName )
{
	const bool bStale = IsShippedImageStale( szTerrainName, szImageName );
	CPtr<IGFXTexture> pTexture;
	std::string szSource = szImageName;
	if ( bStale )
	{
		const std::string szCachePath = GetCachedMapImage( szTerrainName );
		if ( !szCachePath.empty() )
		{
			pTexture = LoadTextureFromFile( szCachePath );
			if ( pTexture )
				szSource = szCachePath;
		}
	}
	if ( !pTexture )
		pTexture = GetSingleton<ITextureManager>()->GetTexture( szImageName.c_str() );
	if ( getenv( "BK_UI_TRACE" ) )
		fprintf( stderr, "BK_UI_TRACE: map image \"%s\" for \"%s\" stale=%d texture=%d\n",
			szSource.c_str(), szTerrainName.c_str(), bStale ? 1 : 0, pTexture ? 1 : 0 );
	return pTexture;
}
