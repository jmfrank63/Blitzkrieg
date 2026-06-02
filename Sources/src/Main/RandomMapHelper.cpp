#include "StdAfx.h"

#include "RandomMapHelper.h"
#include "GameStats.h"
#include "iMain.h"
#include "..\StreamIO\StreamIOTypes.h"
#include "..\StreamIO\ProgressHook.h"
#include "..\RandomMapGen\MapInfo_Types.h"
#include "..\RandomMapGen\Resource_Types.h"
#include "..\Misc\FileUtils.h"
void STDCALL StoreRandomMap( const std::string &szMissionName, NSaveLoad::SRandomHeader *pRndHdr, CPtr<IRandomGenSeed> *ppSeed )
{
	const SMissionStats *pMission = NGDB::GetGameStats<SMissionStats>( szMissionName.c_str(), IObjectsDB::MISSION );
	CPtr<IDataStream> pRGStream = GetSingleton<IDataStorage>()->OpenStream( ("maps\\" + pMission->szFinalMap + ".seed").c_str(), STREAM_ACCESS_READ );
	NI_ASSERT_T( pRGStream != 0, NStr::Format("Can't open random seed file for random map \"%s\"", pMission->szFinalMap.c_str()) );
	if ( pRGStream == 0 ) 
		return;
	SStorageElementStats stats;
	pRGStream->GetStats( &stats );
	pRndHdr->dwRandomDateTime = stats.mtime;
	pRndHdr->szChapterUnitsTableFileName = GetGlobalVar( "Chapter.Units.Table.FileName", "" );
	pRndHdr->nLevel = GetGlobalVar( "Chapter.Units.Table.Level", 0 );
	pRndHdr->szGraphName = GetGlobalVar( "Chapter.Units.Table.GraphName", "" );
	pRndHdr->nAngle = GetGlobalVar( "Chapter.Units.Table.Angle", 0 );

	*ppSeed = CreateObject<IRandomGenSeed>( STREAMIO_RANDOM_GEN_SEED );
	(*ppSeed)->Restore( pRGStream );
};
void STDCALL RestoreRandomMap( const std::string &szMissionName, const NSaveLoad::SRandomHeader &rndhdr, IRandomGenSeed *pSeed )
{
	if ( const SMissionStats *pMission = NGDB::GetGameStats<SMissionStats>(szMissionName.c_str(), IObjectsDB::MISSION) ) 
	{
		SStorageElementStats stats;
		GetSingleton<IDataStorage>()->GetStreamStats( ("maps\\" + pMission->szFinalMap + ".seed").c_str(), &stats );
		if ( stats.mtime != rndhdr.dwRandomDateTime ) 
		{
			GetSingleton<IRandomGen>()->SetSeed( pSeed );
			CPtr<IMovieProgressHook> pProgress = CreateObject<IMovieProgressHook>( MAIN_PROGRESS_INDICATOR );
			pProgress->Init( IMovieProgressHook::PT_MAPGEN );
			pProgress->SetNumSteps( RMGC_CREATE_RANDOM_MAP_STEP_COUNT );
			
			int nFoundGraphIndex = -1;
			int nFoundAngle = rndhdr.nAngle;
			{
				SRMTemplate randomMapTemplate;
				bool bResult = LoadDataResource( pMission->szTemplateMap, "", false, 0, RMGC_TEMPLATE_XML_NAME, randomMapTemplate );
				NI_ASSERT_T( bResult,
										 NStr::Format( "RestoreRandomMap, Can't load SRMTemplate from %s", pMission->szTemplateMap.c_str() ) );
				for ( int nGraphIndex = 0; nGraphIndex < randomMapTemplate.graphs.size(); ++nGraphIndex )
				{
					if ( randomMapTemplate.graphs[nGraphIndex] == rndhdr.szGraphName )
					{
						nFoundGraphIndex = nGraphIndex;
						break;
					}
				}
			}			
			if ( nFoundGraphIndex < 0 )
			{
				NI_ASSERT_T( nFoundGraphIndex >= 0,
										 NStr::Format( "Old save on random map. Please, press <Continue>. RestoreRandomMap, Can't find graph <%s> in SRMTemplate from <%s>.", rndhdr.szGraphName.c_str(), pMission->szTemplateMap.c_str() ) );
				nFoundGraphIndex = -1;
				nFoundAngle = -1;
			}
			const bool bRes = CMapInfo::CreateRandomMap( const_cast<SMissionStats*>( pMission ), rndhdr.szChapterUnitsTableFileName, rndhdr.nLevel, nFoundGraphIndex, nFoundAngle, true, true, 0, pProgress );
			pProgress->Stop();
			NI_ASSERT_T( bRes != false, NStr::Format("Can not generate random map \"%s\"", szMissionName.c_str()) );

			IDataStorage *pStorage = GetSingleton<IDataStorage>();
			const std::string szFullMissionName = pStorage->GetName() + szMissionName + ".xml";
			CPtr<IDataStream> pStream = CreateFileStream( szFullMissionName.c_str(), STREAM_ACCESS_WRITE );
			CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::WRITE );
			saver.Add( "RPG", const_cast<SMissionStats*>(pMission) );
			const std::string szFullFileName = GetSingleton<IDataStorage>()->GetName() + std::string("maps\\") + pMission->szFinalMap + ".seed";
			{
				SWin32Time w32time = rndhdr.dwRandomDateTime;
				FILETIME localfiletime, filetime;
				DosDateTimeToFileTime( w32time.GetDate(), w32time.GetTime(), &localfiletime );
				LocalFileTimeToFileTime( &localfiletime, &filetime );
				NFile::CFile::SetFileTime( szFullFileName.c_str(), &filetime, &filetime, &filetime );
			}
		}
	}
	else
	{
		NI_ASSERT_T( false, NStr::Format("Can't find mission stats (\"%s\") for random mission to re-create map", szMissionName.c_str()) );
	}
}
