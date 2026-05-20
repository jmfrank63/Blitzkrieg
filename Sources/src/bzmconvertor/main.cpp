#include "StdAfx.h"

#include "..\RandomMapGen\MapInfo_Types.h"
#include "..\Misc\FileUtils.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NParams
{
	static std::string szMapName;
	static std::string szStartDir;
	static std::string szDestDir;
	static bool bConvertFences = false;
	static bool bConvertBZM = true;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ProcessCommandLine( int argc, char *argv[] )
{
	std::vector<std::string> strings;
	strings.reserve( argc - 1 );
	for ( int i = 1; i < argc; ++i )
		strings.push_back( argv[i] );
	//
	for ( std::vector<std::string>::const_iterator it = strings.begin(); it != strings.end(); ++it )
	{
		if ( (it->size() > 4) && (it->find(".xml") == it->size() - 4) )
		{
			NParams::szMapName = *it;
			NParams::szMapName = NParams::szMapName.substr( 0, NParams::szMapName.rfind('.') );
		}
		else if ( *it == "-fences" )
			NParams::bConvertFences = true;
		else if ( *it == "-nobzm" ) 
			NParams::bConvertBZM = false;
		else
		{
			if ( NParams::szStartDir.empty() ) 
			{
				NParams::szStartDir = *it;
				if ( NParams::szStartDir.empty() ) 
					NParams::szStartDir = ".\\";
				else if ( NParams::szStartDir[NParams::szStartDir.size() - 1] != '\\' ) 
					NParams::szStartDir += "\\";
			}
			else
			{
				NParams::szDestDir = *it;
				if ( NParams::szDestDir.empty() ) 
					NParams::szDestDir = ".\\";
				else if ( NParams::szDestDir[NParams::szDestDir.size() - 1] != '\\' ) 
					NParams::szDestDir += "\\";
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TRPGStats>
void ConvertFrameIndex( IObjectsDB *pGDB, SMapObjectInfo *pInfo, const TRPGStats *pStats, const char *pszType )
{
	if ( pStats == 0 ) 
		return;
	const int nType = pStats->GetTypeFromIndex( pInfo->nFrameIndex );
	if ( nType == -1 ) 
	{
		NI_ASSERT_T( nType != -1, NStr::Format("Unknown type for %s \"%s\". Was frame index %d", pszType, pInfo->szName.c_str(), pInfo->nFrameIndex) );
	}
	else
		pInfo->nFrameIndex = nType;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ConvertFences( CMapInfo *pMapInfo )
{
	IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
	//
	for ( std::vector<SMapObjectInfo>::iterator it = pMapInfo->objects.begin(); it != pMapInfo->objects.end(); ++it )
	{
		const SGDBObjectDesc *pDesc = pGDB->GetDesc( it->szName.c_str() );
		NI_ASSERT_TF( pDesc != 0, NStr::Format("Can't find descriptor for \"%s\"", it->szName.c_str()), continue );
		switch ( pDesc->eGameType ) 
		{
			case SGVOGT_FENCE:
				ConvertFrameIndex( pGDB, &(*it), NGDB::GetRPGStats<SFenceRPGStats>(pGDB, pDesc), "fence" );
				break;
			case SGVOGT_ENTRENCHMENT:
				ConvertFrameIndex( pGDB, &(*it), NGDB::GetRPGStats<SEntrenchmentRPGStats>(pGDB, pDesc), "entrenchment" );
				break;
			case SGVOGT_BRIDGE:
				ConvertFrameIndex( pGDB, &(*it), NGDB::GetRPGStats<SBridgeRPGStats>(pGDB, pDesc), "bridge" );
				break;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CProcessMap
{
	bool bConvertFences;
	bool bSaveAsBZM;
public:
	CProcessMap( bool _bConvertFences, bool _bSaveAsBZM )
		: bConvertFences( _bConvertFences ), bSaveAsBZM( _bSaveAsBZM ) {  }
	//
	int Convert( const std::string &szMapName, const std::string &szResultName ) const
	{
		printf( "Processing map \"%s\"...\n", szMapName.c_str() );
		//
		CMapInfo mapinfo;
		if ( CPtr<IDataStream> pStream = OpenFileStream(szMapName.c_str(), STREAM_ACCESS_READ) )
		{
			CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
			saver.AddTypedSuper( &mapinfo );
		}
		else
			return 0xDEAD;
		//
		if ( NParams::bConvertFences ) 
			ConvertFences( &mapinfo );
		// write result
		if ( NParams::bConvertBZM ) 
		{
			if ( CPtr<IDataStream> pStream = CreateFileStream(szResultName.c_str(), STREAM_ACCESS_WRITE) )
			{
				CPtr<IStructureSaver> pSS = CreateStructureSaver( pStream, IStructureSaver::WRITE );
				CSaverAccessor saver = pSS;
				saver.Add( 1, &mapinfo );
			}
			else
				return 0xDEAD;
		}
		else if ( NParams::bConvertFences ) 
		{
			if ( CPtr<IDataStream> pStream = CreateFileStream(szResultName.c_str(), STREAM_ACCESS_WRITE) )
			{
				CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::WRITE );
				saver.AddTypedSuper( &mapinfo );
			}
			else
				return 0xDEAD;
		}
		printf( "Done processing map \"%s\"...\n", szMapName.c_str() );
		return 0;
	}
	//
	int operator()( NFile::CFileIterator &file ) const
	{
		if ( file.IsDirectory() ) 
			return 0;

		std::string szMapName = file.GetFilePath();
		szMapName = szMapName.substr( 0, szMapName.rfind('.') );
		std::string szDestMapName = szMapName;
		const char *pszResultExt = bSaveAsBZM ? ".bzm" : ".xml";
		if ( !NParams::szStartDir.empty() && !NParams::szDestDir.empty() ) 
		{
			szDestMapName = szDestMapName.substr( NParams::szStartDir.size(), std::string::npos );
			szDestMapName = NParams::szDestDir + szDestMapName;
		}
		return Convert( szMapName + ".xml", szDestMapName + pszResultExt );
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main( int argc, char *argv[] )
{
	ProcessCommandLine( argc, argv );
	if ( NParams::szMapName.empty() && NParams::szStartDir.empty() ) 
		return 0xDEAD;
	// open main resource system and register as '0'
	{
		CPtr<IDataStorage> pStorage = OpenStorage( ".\\data\\*.pak", STREAM_ACCESS_READ, STORAGE_TYPE_COMMON );
		RegisterSingleton( IDataStorage::tidTypeID, pStorage );
	}
	// CRAP{ load game database
	{
		CPtr<IObjectsDB> pODB = CreateObjectsDB();
		pODB->LoadDB();
		RegisterSingleton( IObjectsDB::tidTypeID, pODB );
		//
		GetSLS()->SetGDB( pODB );
	}
	//
	CProcessMap processor( NParams::bConvertFences, NParams::bConvertBZM );
	if ( NParams::szMapName.empty() ) 
		NFile::EnumerateFiles( NParams::szStartDir.c_str(), "*.xml", processor, true );
	else
	{
		std::string szMapName = NParams::szMapName.substr( 0, NParams::szMapName.rfind('.') );
		std::string szDestMapName = szMapName;
		const char *pszResultExt = NParams::bConvertBZM ? ".bzm" : ".xml";
		if ( !NParams::szStartDir.empty() && !NParams::szDestDir.empty() ) 
		{
			szDestMapName = szDestMapName.substr( NParams::szStartDir.size() + 1, std::string::npos );
			szDestMapName = NParams::szDestDir + szDestMapName;
		}
		processor.Convert( szMapName + ".xml", szDestMapName + pszResultExt );
	}
	//
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
