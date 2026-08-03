#include "StdAfx.h"

#include "MinimapCreation.h"

#include "../Image/Image.h"
#include "../RandomMapGen/IB_Types.h"
#include "../RandomMapGen/MiniMap_Types.h"
#include "../RandomMapGen/MapInfo_Types.h"
void CMinimapCreation::Create1Minimap( const std::string &szTerrainName, const std::string &szMiniMapName )
{
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	SStorageElementStats statsXML, statsBZM;
	Zero( statsXML );
	pStorage->GetStreamStats( ( szTerrainName + ".xml" ).c_str(), &statsXML );
	Zero( statsBZM );
	pStorage->GetStreamStats( ( szTerrainName + ".bzm" ).c_str(), &statsBZM );

	int nMapStreamType = 0;
	if ( statsXML.mtime > statsBZM.mtime ) 
		nMapStreamType = 1;
	else
		nMapStreamType = 2;

	bool bNeedCreate = true;
	{
		SStorageElementStats miniMapStats;
		Zero( miniMapStats );
		pStorage->GetStreamStats( ( szMiniMapName + GetDDSImageExtention( COMPRESSION_DXT ) ).c_str(), &miniMapStats );

		if ( nMapStreamType == 1 )
		{
			if ( statsXML.mtime < miniMapStats.mtime )
				bNeedCreate = false;	
		}
		else if ( nMapStreamType == 2 )
		{
			if ( statsBZM.mtime < miniMapStats.mtime )
				bNeedCreate = false;
		}
		else
		{
			NI_ASSERT_T( false, NStr::Format( "Can't open \"%s\" map", szTerrainName.c_str() ) );
		}
	}
	
	if ( bNeedCreate )
	{
		CMapInfo mapInfo;
		if ( nMapStreamType == 1 )
		{
			CPtr<IDataStream> pStreamXML = GetSingleton<IDataStorage>()->OpenStream( ( szTerrainName + ".xml" ).c_str(), STREAM_ACCESS_READ );
			CTreeAccessor saver = CreateDataTreeSaver( pStreamXML, IDataTree::READ );
			saver.AddTypedSuper( &mapInfo );
		}
		else if ( nMapStreamType == 2 )
		{
			CPtr<IDataStream> pStreamBZM = GetSingleton<IDataStorage>()->OpenStream( ( szTerrainName + ".bzm" ).c_str(), STREAM_ACCESS_READ );
			CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStreamBZM, IStructureSaver::READ );
			CSaverAccessor saver = pSaver;
			saver.Add( 1, &mapInfo );
		}
		else
		{
			NI_ASSERT_T( false, NStr::Format( "Can't open \"%s\" map", szTerrainName.c_str() ) );
		}
		mapInfo.UnpackFrameIndices();
		CRMImageCreateParameterList imageCreateParameterList;
		imageCreateParameterList.push_back( SRMImageCreateParameter( szMiniMapName, CTPoint<int>( 0x200, 0x200 ), true, false, SRMImageCreateParameter::INTERMISSION_IMAGE_BRIGHTNESS, SRMImageCreateParameter::INTERMISSION_IMAGE_CONSTRAST, SRMImageCreateParameter::INTERMISSION_IMAGE_GAMMA ) ); 
		mapInfo.CreateMiniMapImage( imageCreateParameterList );
	}
}
