#include "StdAfx.h"
#include "IB_Types.h"
#include "Resource_Types.h"
#include "..\Platform\System.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool SaveImageToTGAImageResource( IImage *pImage, const std::string &rszTGAImageResourceFileName )
{
	try
	{
		CPtr<IImageProcessor> pImageProcessor = GetSingleton<IImageProcessor>();
		std::string szTGAImageResourceFileName = rszTGAImageResourceFileName;
		NStr::ToLower( szTGAImageResourceFileName );
		if ( ( szTGAImageResourceFileName.size() < 2 ) || szTGAImageResourceFileName[1] != ':' )
		{
			CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>();
			szTGAImageResourceFileName = pDataStorage->GetName() + szTGAImageResourceFileName;
		}
			
		CPtr<IDataStream> pStream = CreateFileStream( ( szTGAImageResourceFileName +".tga" ).c_str(), STREAM_ACCESS_WRITE );
		if ( !pStream )
		{
			return false;
		}
		pImageProcessor->SaveImageAsTGA( pStream, pImage );
		return true;
	}
	catch ( ... )
	{
		return false;
	}
}

bool SaveImageToDDSImageResource( IImage *pImage, const std::string &rszDDSImageResourceFileName,
																	EGFXPixelFormat nCompressedFormat,
																	EGFXPixelFormat nLowFormat,
																	EGFXPixelFormat nHighFormat )
{
	try
	{
		CPtr<IImageProcessor> pImageProcessor = GetSingleton<IImageProcessor>();
		std::string szDDSImageResourceFileName = rszDDSImageResourceFileName;
		NStr::ToLower( szDDSImageResourceFileName );
		if ( ( szDDSImageResourceFileName.size() < 2 ) || szDDSImageResourceFileName[1] != ':' )
		{
			CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>();
			szDDSImageResourceFileName = pDataStorage->GetName() + szDDSImageResourceFileName;
		}
		
		{
			CPtr<IDDSImage> pDDSImage = pImageProcessor->Compress( pImage, nCompressedFormat );

			CPtr<IDataStream> pDDSStream = CreateFileStream( ( szDDSImageResourceFileName + GetDDSImageExtention( COMPRESSION_DXT ) ).c_str(), STREAM_ACCESS_WRITE );
			if ( !pDDSStream )
			{
				return false;
			}
			pImageProcessor->SaveImageAsDDS( pDDSStream, pDDSImage );
		}
		{
			CPtr<IDDSImage> pDDSImage = pImageProcessor->Compress( pImage, nLowFormat );

			CPtr<IDataStream> pDDSStream = CreateFileStream( ( szDDSImageResourceFileName + GetDDSImageExtention( COMPRESSION_LOW_QUALITY ) ).c_str(), STREAM_ACCESS_WRITE );
			if ( !pDDSStream )
			{
				return false;
			}
			pImageProcessor->SaveImageAsDDS( pDDSStream, pDDSImage );
		}
		{
			CPtr<IDDSImage> pDDSImage = pImageProcessor->Compress( pImage, nHighFormat );

			CPtr<IDataStream> pDDSStream = CreateFileStream( ( szDDSImageResourceFileName + GetDDSImageExtention( COMPRESSION_HIGH_QUALITY ) ).c_str(), STREAM_ACCESS_WRITE );
			if ( !pDDSStream )
			{
				return false;
			}
			pImageProcessor->SaveImageAsDDS( pDDSStream, pDDSImage );
		}
		return true;
	}
	catch ( ... )
	{
		return false;
	}
}

IImage* LoadImageFromTGAImageResource( const std::string &rszTGAImageResourceFileName )
{
	try
	{
		CPtr<IImageProcessor> pImageProcessor = GetSingleton<IImageProcessor>();
		CPtr<IDataStream> pTGAStream = 0;
		CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>();
		if ( ( rszTGAImageResourceFileName.size() < 2 ) || rszTGAImageResourceFileName[1] != ':' )
		{
			if ( pDataStorage )
			{
				pTGAStream = pDataStorage->OpenStream( ( rszTGAImageResourceFileName + ".tga" ).c_str(), STREAM_ACCESS_READ );	
			}
		}
		else
		{
			if ( pDataStorage && ( rszTGAImageResourceFileName.find( pDataStorage->GetName() ) == 0 ) )
			{
				const std::string szShortImageName = rszTGAImageResourceFileName.substr( strlen( pDataStorage->GetName() ) );
				pTGAStream = pDataStorage->OpenStream( ( szShortImageName + ".tga" ).c_str(), STREAM_ACCESS_READ );	
			}
			else
			{
				pTGAStream = OpenFileStream( ( rszTGAImageResourceFileName + ".tga" ).c_str(), STREAM_ACCESS_READ );	
			}
		}
		if ( !pTGAStream )
		{
			return 0;
		}
		return pImageProcessor->LoadImage( pTGAStream );
	}
	catch ( ... )
	{
		return 0;
	}
}

IImage* LoadImageFromDDSImageResource( const std::string &rszDDSImageResourceFileName )
{
	try
	{
		CPtr<IImageProcessor> pImageProcessor = GetSingleton<IImageProcessor>();
		CPtr<IDataStream> pDDSStream = 0;
		CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>();
		if ( ( rszDDSImageResourceFileName.size() < 2 ) || rszDDSImageResourceFileName[1] != ':' )
		{
			if ( pDataStorage )
			{
				pDDSStream = pDataStorage->OpenStream( ( rszDDSImageResourceFileName + GetDDSImageExtention( COMPRESSION_HIGH_QUALITY ) ).c_str(), STREAM_ACCESS_READ );	
			}
		}
		else
		{
			if ( pDataStorage && ( rszDDSImageResourceFileName.find( pDataStorage->GetName() ) == 0 ) )
			{
				const std::string szShortImageName = rszDDSImageResourceFileName.substr( strlen( pDataStorage->GetName() ) );
				pDDSStream = pDataStorage->OpenStream( ( szShortImageName + GetDDSImageExtention( COMPRESSION_HIGH_QUALITY ) ).c_str(), STREAM_ACCESS_READ );	
			}
			else
			{
				pDDSStream = OpenFileStream( ( rszDDSImageResourceFileName + GetDDSImageExtention( COMPRESSION_HIGH_QUALITY ) ).c_str(), STREAM_ACCESS_READ );	
			}
		}
		if ( !pDDSStream )
		{
			return 0;
		}
		CPtr<IDDSImage> pDDSImage = pImageProcessor->LoadDDSImage( pDDSStream );
		return pImageProcessor->Decompress( pDDSImage );
	}
	catch ( ... )
	{
		return 0;
	}
}

void EnumFilesInDataStorage( std::vector<SEnumFilesInDataStorageParameter> *pParameters, IDataStorage *pStorage, SEnumFolderStructureParameter *pEnumFolderStructureParameter )
{
	if ( pParameters || pEnumFolderStructureParameter )
	{
		if ( pParameters )
		{
			for ( int nParameterElement = 0; nParameterElement < pParameters->size(); ++nParameterElement )
			{
				SEnumFilesInDataStorageParameter &rParameter = ( *pParameters )[nParameterElement];
				NStr::ToLower( rParameter.szPath );
				NStr::ToLower( rParameter.szExtention );

				rParameter.nPathLength = rParameter.szPath.size();
				rParameter.nExtentionLength = rParameter.szExtention.size();
			}
		}
		/**
		if ( pEnumFolderStructureParameter )
		{
			pEnumFolderStructureParameter->folders.clear();
		}
		/**/
		if ( pStorage )
		{
			if ( CPtr<IStorageEnumerator> pEnumerator = pStorage->CreateEnumerator() )
			{
				pEnumerator->Reset( "*.*" );
				std::vector<std::string> strings;
				int nCount = 0;
				while (  pEnumerator->Next() )
				{
					const SStorageElementStats *pStats = pEnumerator->GetStats();
					if ( pStats && pStats->pszName )
					{
						++nCount;
						if ( pParameters )
						{
							const int nStatsLength = strlen( pStats->pszName );
							for ( int nParameterElement = 0; nParameterElement < pParameters->size(); ++nParameterElement )
							{
								SEnumFilesInDataStorageParameter &rParameter = ( *pParameters )[nParameterElement];

								if ( ( rParameter.nPathLength < nStatsLength ) &&
										 ( strncmp( pStats->pszName, rParameter.szPath.c_str(), rParameter.nPathLength ) == 0 ) )
								{
									if ( strncmp( pStats->pszName + nStatsLength - rParameter.nExtentionLength, rParameter.szExtention.c_str(), rParameter.nExtentionLength ) == 0 )
									{
										rParameter.fileNames.push_back( pStats->pszName );
									}
								}
							}
						}
						if ( pEnumFolderStructureParameter )
						{
							strings.clear();
							NStr::SplitString( pStats->pszName, strings, '\\' ) ;
							if ( strings.size() > ( pEnumFolderStructureParameter->nIgnoreFolderCount + 1 ) )
							{
								for ( int nStringIndex = 0; nStringIndex < ( strings.size() - pEnumFolderStructureParameter->nIgnoreFolderCount - 1 ); ++nStringIndex )
								{
									if ( !NStr::IsDecNumber( strings[nStringIndex] ) )
									{
										for ( int nRelativeStringIndex = 0; nRelativeStringIndex < ( strings.size() - pEnumFolderStructureParameter->nIgnoreFolderCount - 1 ); ++nRelativeStringIndex )
										{
											if ( !NStr::IsDecNumber( strings[nRelativeStringIndex] ) )
											{
												pEnumFolderStructureParameter->SetRelativeFolder( strings[nStringIndex], strings[nRelativeStringIndex] );
											}
										}
										/**
										for ( int nRelativeStringIndex = ( nStringIndex + 1 ); nRelativeStringIndex < ( strings.size() - pEnumFolderStructureParameter->nIgnoreFolderCount ); ++nRelativeStringIndex )
										{
											if ( !NStr::IsDecNumber( strings[nRelativeStringIndex] ) )
											{
												pEnumFolderStructureParameter->SetRelativeFolder( strings[nStringIndex], strings[nRelativeStringIndex] );
											}
										}
										/**/
									}
								}
							}
						}
					}
				}
				NStr::Format( "Count: %d", nCount );
			}
		}
	}
}

bool ExecuteProcess( const std::string &szCommand, const std::string &szCmdLine, const std::string &szDirectory, bool bWait )
{
	std::vector<std::string> arguments;
	arguments.push_back( szCommand );
	std::string current;
	bool quoted = false;
	for ( std::size_t i = 0; i < szCmdLine.size(); ++i )
	{
		const char character = szCmdLine[i];
		if ( character == '"' ) { quoted = !quoted; continue; }
		if ( !quoted && (character == ' ' || character == '\t') )
		{
			if ( !current.empty() ) { arguments.push_back( current ); current.clear(); }
		}
		else current += character;
	}
	if ( !current.empty() ) arguments.push_back( current );
	int exitCode = -1;
	return NPlatform::RunProcess( arguments, szDirectory, bWait ? &exitCode : nullptr );
}
