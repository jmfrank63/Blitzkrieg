#include "StdAfx.h"
#include "StrProc.h"
#include "FileUtils.h"
#include "DataStorage.h"
#include "ZipFile.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const std::string CDataStorage::BITMAP_EXTENTION	= ".bmp";
const std::string CDataStorage::TEXT_EXTENTION = ".txt";
const std::string CDataStorage::SOUND_EXTENTION = ".wav";
const std::string CDataStorage::CONFIGURATION_EXTENTION = ".ini";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDataStorage::Load( const std::string &rszFileName, CDC *pDC )
{
	CZipFile zipFile;
	if ( zipFile.Init( rszFileName ) )
	{
		int nFilesCount = zipFile.GetNumFiles();
		for ( int nFileIndex = 0; nFileIndex < nFilesCount; ++nFileIndex )
		{
			const int nFileSize = zipFile.GetFileLen( nFileIndex );
			std::vector<BYTE> buffer( nFileSize );
			zipFile.ReadFile( nFileIndex, &( buffer[0] ) );

			std::string szFileName;
			zipFile.GetFileName( nFileIndex, &szFileName );
			NStr::ToLower( szFileName );
			if ( !szFileName.empty() )
			{
				if ( szFileName.rfind( BITMAP_EXTENTION ) == ( szFileName.size() - BITMAP_EXTENTION.size() ) )
				{
					bitmaps[szFileName].Load( buffer, pDC );
				}
				else if ( szFileName.rfind( SOUND_EXTENTION ) == ( szFileName.size() - SOUND_EXTENTION.size() ) )
				{
					sounds[szFileName].Load( buffer );
				}
				else if ( szFileName.rfind( TEXT_EXTENTION ) == ( szFileName.size() - TEXT_EXTENTION.size() ) )
				{
					texts[szFileName].Load( buffer );
				}
				else if ( szFileName.rfind( CONFIGURATION_EXTENTION ) == ( szFileName.size() - CONFIGURATION_EXTENTION.size() ) )
				{
					configurations[szFileName].Load( buffer );
				}
			}
		}
		return true;
	}
	return false;
}

void CDataStorage::SetCodePage( int nCodePage )
{
	for ( std::unordered_map<std::string, CARText>::iterator textIterator = texts.begin(); textIterator != texts.end(); ++textIterator )
	{
		textIterator->second.SetCodePage( nCodePage );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CARBitmap* CDataStorage::GetBitmap( const std::string& rszBitmapName )
{
	std::string szBitmapName = rszBitmapName;
	NStr::ToLower( szBitmapName );
	if ( bitmaps.find( szBitmapName ) == bitmaps.end() )
	{
		return 0;
	}
	const CARBitmap &rBitmap = bitmaps[szBitmapName];
	return &( rBitmap );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CARSound* CDataStorage::GetSound( const std::string& rszSoundName )
{
	std::string szSoundName = rszSoundName;
	NStr::ToLower( szSoundName );
	if ( sounds.find( szSoundName ) == sounds.end() )
	{
		return 0;
	}
	const CARSound &rSound = sounds[szSoundName];
	return &( rSound );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CARText* CDataStorage::GetText( const std::string& rszTextName )
{
	std::string szTextName = rszTextName;
	NStr::ToLower( szTextName );
	if ( texts.find( szTextName ) == texts.end() )
	{
		return 0;
	}
	const CARText &rText = texts[szTextName];
	return &( rText );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CARConfiguration* CDataStorage::GetConfiguration( const std::string& rszConfigurationName )
{
	std::string szConfigurationName = rszConfigurationName;
	NStr::ToLower( szConfigurationName );
	if ( configurations.find( szConfigurationName ) == configurations.end() )
	{
		return 0;
	}
	const CARConfiguration &rConfiguration = configurations[szConfigurationName];
	return &( rConfiguration );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
