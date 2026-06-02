#include "StdAfx.h"
#include "MODCollector.h"
#include "..\Misc\FileUtils.h"
void CMODCollector::Collect()
{
	if ( availableMODs.empty() )
	{
		if ( IDataStorage *pDataStorage = GetSingleton<IDataStorage>() )
		{
			std::string szMODPath = pDataStorage->GetName();
			szMODPath = szMODPath.substr( 0, szMODPath.rfind( "data\\" ) );
			szMODPath += std::string( "mods\\" );
			NStr::ToLower( szMODPath );
				
			for ( NFile::CFileIterator _NFileIterator( NStr::Format( _T( "%s\\*" ), szMODPath.c_str() ) ); !_NFileIterator.IsEnd(); ++_NFileIterator )
			{
				if ( _NFileIterator.IsDirectory() && !_NFileIterator.IsDots() )
				{
					CMODNode MODNode;
					MODNode.szMODFolder = _NFileIterator.GetFilePath() + std::string( "\\" );
					
					if ( CPtr<IDataStorage> pMOD = OpenStorage( ( MODNode.szMODFolder + std::string( "data\\*.pak" ) ).c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON) )
					{
						if ( CPtr<IDataStream> pStream = pMOD->OpenStream( "mod.xml", STREAM_ACCESS_READ ) )
						{
							{
								CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
								saver.Add( "MODName", &( MODNode.szMODName ) );
								saver.Add( "MODVersion", &( MODNode.szMODVersion ) );
							}
							if ( ( !MODNode.szMODVersion.empty() ) && ( !MODNode.szMODVersion.empty() ) )
							{
								const std::string szKEY = GetKey( MODNode.szMODName, MODNode.szMODVersion );
								if ( availableMODs.find( szKEY ) == availableMODs.end() )
								{
									availableMODs[szKEY] = MODNode;
								}
								else
								{
									NStr::DebugTrace( "CMODCollector::Collect(), AMBIGIOUS MOD! szKEY: %s, folder: %s\n", szKEY.c_str(), MODNode.szMODFolder.c_str() );
								}
							}
						}
					}
				}
			}
		}
	}
}

const std::string CMODCollector::GetKey( const std::string &rszMODName, const std::string &rszMODVersion )
{ 
	if ( !rszMODName.empty() )
	{
		if ( !rszMODVersion.empty() )
		{
			return ( rszMODName + std::string( " " ) + rszMODVersion );
		}
		else
		{
			return rszMODName;
		}
	}
	else
	{
		return "";
	}
}
