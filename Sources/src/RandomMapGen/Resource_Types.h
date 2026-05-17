#if !defined(__Resource__Types__)
#define __Resource__Types__

#include "..\GFX\GFXTypes.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class Type>
bool LoadDataResource( const std::string &rszResourceFileName, const std::string &rszBinaryExtention, bool bBinary, int nChunkNumber, const std::string &rszChunkLabel, Type &rResource )
{
	try
	{
		IDataStorage *pDataStorage = GetSingleton<IDataStorage>();
		NI_ASSERT_TF( pDataStorage != 0,
									NStr::Format( "LoadDataResource, GetSingleton<IDataStorage>() == 0" ), 
									return false );
		if ( bBinary )
		{
			CPtr<IDataStream> pStreamBinary = pDataStorage->OpenStream( ( rszResourceFileName + rszBinaryExtention ).c_str(), STREAM_ACCESS_READ );
			if ( pStreamBinary == 0 )
			{
				return false;
			}
			CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStreamBinary, IStructureSaver::READ );
			CSaverAccessor saver = pSaver;
			saver.Add( nChunkNumber, &rResource );
		}
		else
		{
			CPtr<IDataStream> pStreamXML = pDataStorage->OpenStream( ( rszResourceFileName + ".xml" ).c_str(), STREAM_ACCESS_READ );
			if ( pStreamXML == 0 )
			{
				return false;
			}
			CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStreamXML, IDataTree::READ );
			CTreeAccessor saver = pSaver;
			saver.Add( rszChunkLabel.c_str(), &rResource );
		}
		return true;
	}
	catch ( ... )
	{
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class Type>
bool LoadTypedSuperDataResource( const std::string &rszResourceFileName, const std::string &rszBinaryExtention, bool bBinary, int nChunkNumber, Type &rResource )
{
	try
	{
		IDataStorage *pDataStorage = GetSingleton<IDataStorage>();
		NI_ASSERT_TF( pDataStorage != 0,
									NStr::Format( "LoadTypedSuperDataResource, GetSingleton<IDataStorage>() == 0" ), 
									return false );
		if ( bBinary )
		{
			CPtr<IDataStream> pStreamBinary = pDataStorage->OpenStream( ( rszResourceFileName + rszBinaryExtention ).c_str(), STREAM_ACCESS_READ );
			if ( pStreamBinary == 0 )
			{
				return false;
			}
			CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStreamBinary, IStructureSaver::READ );
			CSaverAccessor saver = pSaver;
			saver.Add( nChunkNumber, &rResource );
		}
		else
		{
			CPtr<IDataStream> pStreamXML = pDataStorage->OpenStream( ( rszResourceFileName + ".xml" ).c_str(), STREAM_ACCESS_READ );
			if ( pStreamXML == 0 )
			{
				return false;
			}
			CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStreamXML, IDataTree::READ );
			CTreeAccessor saver = pSaver;
			saver.AddTypedSuper( &rResource );
		}
		return true;
	}
	catch ( ... )
	{
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class Type>
bool SaveDataResource( const std::string &rszResourceFileName, const std::string &rszBinaryExtention, bool bBinary, int nChunkNumber, const std::string &rszChunkLabel, Type &rResource )
{
	try
	{
		IDataStorage *pDataStorage = GetSingleton<IDataStorage>();
		NI_ASSERT_TF( pDataStorage != 0,
									NStr::Format( "SaveDataResource, GetSingleton<IDataStorage>() == 0" ), 
									return false );
		if ( bBinary )
		{
			CPtr<IDataStream> pStreamBinary = CreateFileStream( ( pDataStorage->GetName() + rszResourceFileName + rszBinaryExtention ).c_str(), STREAM_ACCESS_WRITE );
			if ( pStreamBinary == 0 )
			{
				return false;
			}
			CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStreamBinary, IStructureSaver::WRITE );
			CSaverAccessor saver = pSaver;
			saver.Add( nChunkNumber, &rResource );
		}
		else
		{
			CPtr<IDataStream> pStreamXML = CreateFileStream( ( pDataStorage->GetName() + rszResourceFileName + ".xml" ).c_str(), STREAM_ACCESS_WRITE );
			if ( pStreamXML == 0 )
			{
				return false;
			}
			CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStreamXML, IDataTree::WRITE );
			CTreeAccessor saver = pSaver;
			saver.Add( rszChunkLabel.c_str(), &rResource );
		}
		return true;
	}
	catch ( ... )
	{
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class Type>
bool SaveTypedSuperDataResource( const std::string &rszResourceFileName, const std::string &rszBinaryExtention, bool bBinary, int nChunkNumber, Type &rResource )
{
	try
	{
		IDataStorage *pDataStorage = GetSingleton<IDataStorage>();
		NI_ASSERT_TF( pDataStorage != 0,
									NStr::Format( "SaveTypedSuperDataResource, GetSingleton<IDataStorage>() == 0" ), 
									return false );
		if ( bBinary )
		{
			CPtr<IDataStream> pStreamBinary = CreateFileStream( ( pDataStorage->GetName() + rszResourceFileName + rszBinaryExtention ).c_str(), STREAM_ACCESS_WRITE );
			if ( pStreamBinary == 0 )
			{
				return false;
			}
			CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStreamBinary, IStructureSaver::WRITE );
			CSaverAccessor saver = pSaver;
			saver.Add( nChunkNumber, &rResource );
		}
		else
		{
			CPtr<IDataStream> pStreamXML = CreateFileStream( ( pDataStorage->GetName() + rszResourceFileName + ".xml" ).c_str(), STREAM_ACCESS_WRITE );
			if ( pStreamXML == 0 )
			{
				return false;
			}
			CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStreamXML, IDataTree::WRITE );
			CTreeAccessor saver = pSaver;
			saver.AddTypedSuper( &rResource );
		}
		return true;
	}
	catch ( ... )
	{
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class Type>
bool LoadLatestDataResource( const std::string &rszResourceFileName, const std::string &rszBinaryExtention, int nChunkNumber, const std::string &rszChunkLabel, Type &rResource )
{
	try
	{
		IDataStorage *pDataStorage = GetSingleton<IDataStorage>();
		NI_ASSERT_TF( pDataStorage != 0,
									NStr::Format( "LoadLatestDataResource, GetSingleton<IDataStorage>() == 0" ), 
									return false );
		const std::string szResourceFileName = rszResourceFileName.substr( 0, rszResourceFileName.rfind( '.' ) );

		SStorageElementStats statsBinary, statsXML;
		{
			// get stats from Binary and XML files
			Zero( statsBinary );
			pDataStorage->GetStreamStats( ( szResourceFileName + rszBinaryExtention ).c_str(), &statsBinary );
			Zero( statsXML );
			pDataStorage->GetStreamStats( ( szResourceFileName + ".xml" ).c_str(), &statsXML );
		}		
		if ( statsBinary.mtime >= statsXML.mtime ) 
		{
			CPtr<IDataStream> pStreamBinary = pDataStorage->OpenStream( ( szResourceFileName + rszBinaryExtention ).c_str(), STREAM_ACCESS_READ );
			if ( pStreamBinary == 0 )
			{
				return false;
			}
			CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStreamBinary, IStructureSaver::READ );
			CSaverAccessor saver = pSaver;
			saver.Add( nChunkNumber, &rResource );
		}
		else
		{
			CPtr<IDataStream> pStreamXML = pDataStorage->OpenStream( ( szResourceFileName + ".xml" ).c_str(), STREAM_ACCESS_READ );
			if ( pStreamXML == 0 )
			{
				return false;
			}
			CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStreamXML, IDataTree::READ );
			CTreeAccessor saver = pSaver;
			saver.Add( rszChunkLabel.c_str(), &rResource );
		}
		return true;
	}
	catch ( ... )
	{
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class Type>
bool LoadTypedSuperLatestDataResource( const std::string &rszResourceFileName, const std::string &rszBinaryExtention, int nChunkNumber, Type &rResource, std::string *pszFullFileName = 0 )
{
	try
	{
		IDataStorage *pDataStorage = GetSingleton<IDataStorage>();
		NI_ASSERT_TF( pDataStorage != 0,
									NStr::Format( "LoadTypedSuperLatestDataResource, GetSingleton<IDataStorage>() == 0" ), 
									return false );
		const std::string szResourceFileName = rszResourceFileName.substr( 0, rszResourceFileName.rfind( '.' ) );

		SStorageElementStats statsBinary, statsXML;
		{
			// get stats from Binary and XML files
			Zero( statsBinary );
			pDataStorage->GetStreamStats( ( szResourceFileName + rszBinaryExtention ).c_str(), &statsBinary );
			Zero( statsXML );
			pDataStorage->GetStreamStats( ( szResourceFileName + ".xml" ).c_str(), &statsXML );
		}		
		if ( statsBinary.mtime >= statsXML.mtime ) 
		{
			if ( pszFullFileName )
			{
				( *pszFullFileName ) = ( szResourceFileName + rszBinaryExtention );
			}
			CPtr<IDataStream> pStreamBinary = pDataStorage->OpenStream( ( szResourceFileName + rszBinaryExtention ).c_str(), STREAM_ACCESS_READ );
			if ( pStreamBinary == 0 )
			{
				return false;
			}
			CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStreamBinary, IStructureSaver::READ );
			CSaverAccessor saver = pSaver;
			saver.Add( nChunkNumber, &rResource );
		}
		else
		{
			if ( pszFullFileName )
			{
				( *pszFullFileName ) = ( szResourceFileName + ".xml" );
			}
			CPtr<IDataStream> pStreamXML = pDataStorage->OpenStream( ( szResourceFileName + ".xml" ).c_str(), STREAM_ACCESS_READ );
			if ( pStreamXML == 0 )
			{
				return false;
			}
			CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStreamXML, IDataTree::READ );
			CTreeAccessor saver = pSaver;
			saver.AddTypedSuper( &rResource );
		}
		return true;
	}
	catch ( ... )
	{
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern bool SaveImageToTGAImageResource( interface IImage *pImage, const std::string &rszTGAImageResourceFileName );
//������� ������ ��� ���� ( minimap )
extern bool SaveImageToDDSImageResource( interface IImage *pImage, const std::string &rszDDSImageResourceFileName,
																				 EGFXPixelFormat nCompressedFormat = GFXPF_DXT1,
																				 EGFXPixelFormat nLowFormat = GFXPF_ARGB0565,
																				 EGFXPixelFormat nHighFormat = GFXPF_ARGB8888 );

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//������������ ������ ��� ��������� �������� ��������.� �. ��������� ImageProcessor
extern interface IImage* LoadImageFromTGAImageResource( const std::string &rszTGAImageResourceFileName );
extern interface IImage* LoadImageFromDDSImageResource( const std::string &rszDDSImageResourceFileName );

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SEnumFilesInDataStorageParameter
{
	std::list<std::string> fileNames;
	std::string szPath;
	std::string szExtention;

	int nPathLength;
	int nExtentionLength;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::unordered_map<std::string, std::set<std::string> > TEnumFolders;
struct SEnumFolderStructureParameter
{
	int nIgnoreFolderCount;
	TEnumFolders folders;


	bool IsFolderRelative( const std::string &rszFolder, const std::string &rszRelativeFolder );
	void SetRelativeFolder( const std::string &rszFolder, const std::string &rszRelativeFolder );

	static bool IsFolderRelative( const TEnumFolders &rFolders, const std::string &rszFolder, const std::string &rszRelativeFolder );
	static void SetRelativeFolder( TEnumFolders *pFolders, const std::string &rszFolder, const std::string &rszRelativeFolder );
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void EnumFilesInDataStorage( std::vector<SEnumFilesInDataStorageParameter> *pParameters, IDataStorage *pStorage, SEnumFolderStructureParameter *pEnumFolderStructureParameter = 0 );

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ExecuteProcess( const std::string &szCommand, const std::string &szCmdLine, const std::string &szDirectory, bool bWait = true );

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // #if !defined(__Resource__Types__)
