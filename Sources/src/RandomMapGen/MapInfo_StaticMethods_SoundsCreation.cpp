
#include "StdAfx.h"

#include "MapInfo_Types.h"
#include "../Formats/fmtTerrain.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool CMapInfo::AddSounds( const SLoadMapInfo &rLoadMapInfo, TMapSoundInfoList *pSoundsList, DWORD dwSoundTypeBits )
{
	CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>();
	if ( ( !pODB ) || ( !pSoundsList ) )
	{
		return false;
	}

	int nRiverPointsPeriod = 8;					//период между звуками на реке
	int nDelimiter = 16;								//размер в VIS тайлах масива лесов( в одном VIS тайле может стоять до 4х деревьев )
	int nMinWoodsCount = 4;							//nDelimiter * nDelimiter * 4;
	int nCirclePeriod = 1;							//период ( в размерности nDelimiter ) постановки циклических звуков для лесов
	int nAmbientPeriod = 2;							//период ( в размерности nDelimiter ) постановки циклических ембиентных звуков для лесов
	CVec3 vCircleShiftPoint = VNULL3;		//сдвиг относительно центра постановки 		
	CVec3 vAmbientShiftPoint = VNULL3;	//сдвиг относительно центра постановки
	bool bExactCirclePoint = true;			//в центр масс звуков
	bool bExactAmbientPoint = true;			//в центр масс звуков
	float bCircleRatio = 1.0f;					//вероятность постановки
	float bAmbientRatio = 0.5f;					//вероятность постановки
	
	if ( ( dwSoundTypeBits & SOUND_TYPE_BITS_RIVERS ) == SOUND_TYPE_BITS_RIVERS )
	{
		int nRiverIndex = 0;
		for ( TVSOList::const_iterator riverIterator = rLoadMapInfo.terrain.rivers.begin(); riverIterator != rLoadMapInfo.terrain.rivers.end(); ++riverIterator )
		{
			if ( !( riverIterator->szAmbientSound.empty() ) )
			{
				NStr::DebugTrace( "CMapInfo::AddSounds() river: %d, sound: %s", nRiverIndex, riverIterator->szAmbientSound.c_str() );
				int nRiverPointsCount = nRiverPointsPeriod;
				int nSoundsAdded = 0;
				for ( int nPointIndex = 0; nPointIndex < riverIterator->points.size(); ++nPointIndex )
				{
					if ( nRiverPointsCount >= nRiverPointsPeriod )
					{
						CMapSoundInfo soundInfo;
						soundInfo.szName = riverIterator->szAmbientSound;
						soundInfo.vPos = riverIterator->points[nPointIndex].vPos;
						pSoundsList->push_back( soundInfo );
						++nSoundsAdded;
						nRiverPointsCount = 1;
					}
					else
					{
						++nRiverPointsCount;
					}
				}
				NStr::DebugTrace( ", %d sounds added\n", nSoundsAdded );
			}
			++nRiverIndex;
		}
	}

	if ( ( ( dwSoundTypeBits & SOUND_TYPE_BITS_BUILDINGS ) == SOUND_TYPE_BITS_BUILDINGS ) ||
			 ( ( dwSoundTypeBits & SOUND_TYPE_BITS_FORESTS ) == SOUND_TYPE_BITS_FORESTS ) )
	{
		CArray2D<int> woods( rLoadMapInfo.terrain.tiles.GetSizeX() * 2, rLoadMapInfo.terrain.tiles.GetSizeX() * 2 );
		CArray2D<int> forests( woods.GetSizeX() / nDelimiter,
													 woods.GetSizeY() / nDelimiter );
		woods.SetZero();
		forests.SetZero();

		for ( int nObjectIndex = 0; nObjectIndex < rLoadMapInfo.objects.size(); ++nObjectIndex )
		{
			const SGDBObjectDesc *pGDBObjectDesc = pODB->GetDesc( rLoadMapInfo.objects[nObjectIndex].szName.c_str() );
			if ( pGDBObjectDesc )
			{
				if ( ( dwSoundTypeBits & SOUND_TYPE_BITS_BUILDINGS ) == SOUND_TYPE_BITS_BUILDINGS )
				{
					if ( pGDBObjectDesc->eGameType == SGVOGT_BUILDING )
					{
						const SBuildingRPGStats* pBuildingRPGStats = NGDB::GetRPGStats<SBuildingRPGStats>( rLoadMapInfo.objects[nObjectIndex].szName.c_str() );
						if ( pBuildingRPGStats && !( pBuildingRPGStats->szAmbientSound.empty() ) )
						{
							CMapSoundInfo soundInfo;
							soundInfo.szName = pBuildingRPGStats->szAmbientSound;
							NStr::DebugTrace( "CMapInfo::AddSounds() building: %s, sound: %s\n",  rLoadMapInfo.objects[nObjectIndex].szName.c_str(), pBuildingRPGStats->szAmbientSound.c_str() );
							soundInfo.vPos = rLoadMapInfo.objects[nObjectIndex].vPos;
							AI2Vis( &( soundInfo.vPos ) );
							pSoundsList->push_back( soundInfo );
						}
					}
				}
				
				if ( ( dwSoundTypeBits & SOUND_TYPE_BITS_FORESTS ) == SOUND_TYPE_BITS_FORESTS )
				{
					std::vector<std::string> splitNames;
					bool isWood = false;

					NStr::SplitString( pGDBObjectDesc->szPath, splitNames, '\\');
					for ( int splitNameIndex = 0; splitNameIndex < splitNames.size(); ++splitNameIndex )
					{
						std::string strName = splitNames[splitNameIndex];
						NStr::ToLower( strName );
						if ( strName.find( "flora" ) != std::string::npos )
						{
							isWood = true;
							break;
						}
					}
					if ( isWood )
					{
						int nXIndex = rLoadMapInfo.objects[nObjectIndex].vPos.x / SAIConsts::TILE_SIZE;
						int nYIndex = rLoadMapInfo.objects[nObjectIndex].vPos.y / SAIConsts::TILE_SIZE;
						if ( ( nXIndex >= 0 ) &&
								 ( nYIndex >= 0 ) &&
								 ( nXIndex < woods.GetSizeX() ) &&
								 ( nYIndex < woods.GetSizeY() ) )
						{
							woods[nYIndex][nXIndex] += 1;
							forests[nYIndex / nDelimiter][nXIndex / nDelimiter] += 1;
						}
					}
				}
			}
		}
		if ( ( dwSoundTypeBits & SOUND_TYPE_BITS_FORESTS ) == SOUND_TYPE_BITS_FORESTS )
		{
			if ( !rLoadMapInfo.szForestCircleSounds.empty() )
			{
				NStr::DebugTrace( "CMapInfo::AddSounds() circle forest, sound: %s\n", rLoadMapInfo.szForestCircleSounds.c_str() );

				for ( int nXIndex = 0; nXIndex < forests.GetSizeX(); nXIndex += nCirclePeriod )
				{
					for ( int nYIndex = 0; nYIndex < forests.GetSizeY(); nYIndex += nCirclePeriod )
					{
						int nWoodsCount = 0;

						for ( int nForestXIndex = 0; nForestXIndex < nCirclePeriod; ++nForestXIndex )
						{
							for ( int nForestYIndex = 0; nForestYIndex < nCirclePeriod; ++nForestYIndex )
							{
								nWoodsCount += forests[nYIndex + nForestYIndex][nXIndex + nForestXIndex];
							}
						}
						
						if ( ( nWoodsCount >= nMinWoodsCount ) && ( ( rand() * 1.0f / ( RAND_MAX + 1 ) ) <= bCircleRatio ) )
						{
							CMapSoundInfo soundInfo;
							soundInfo.szName = rLoadMapInfo.szForestCircleSounds;
							soundInfo.vPos = VNULL3;
							
							if ( bExactCirclePoint )
							{
								nWoodsCount = 0;

								for ( int nWoodsXIndex = ( nXIndex * nDelimiter ); nWoodsXIndex < ( ( nXIndex + nCirclePeriod ) * nDelimiter ); ++nWoodsXIndex )
								{
									for ( int nWoodsYIndex = ( nYIndex * nDelimiter ); nWoodsYIndex < ( ( nYIndex + nCirclePeriod ) * nDelimiter ); ++nWoodsYIndex )
									{
										if ( woods[nWoodsYIndex][nWoodsXIndex] > 0 )
										{
											soundInfo.vPos.x += ( nWoodsXIndex + 0.5f ) * SAIConsts::TILE_SIZE;
											soundInfo.vPos.y += ( nWoodsYIndex + 0.5f ) * SAIConsts::TILE_SIZE;
											++nWoodsCount;
										}
									}
								}
								soundInfo.vPos.x /= nWoodsCount;
								soundInfo.vPos.y /= nWoodsCount;
							}
							else
							{
								soundInfo.vPos.x = ( nXIndex + 0.5f ) * nDelimiter * SAIConsts::TILE_SIZE;
								soundInfo.vPos.y = ( nYIndex + 0.5f ) * nDelimiter * SAIConsts::TILE_SIZE;
							}
							soundInfo.vPos += vCircleShiftPoint;
							AI2Vis( &( soundInfo.vPos ) );

							pSoundsList->push_back( soundInfo );
						}
					}
				}
			}
			if ( !rLoadMapInfo.szForestAmbientSounds.empty() )
			{
				NStr::DebugTrace( "CMapInfo::AddSounds() ambient forest, sound: %s\n", rLoadMapInfo.szForestAmbientSounds.c_str() );

				for ( int nXIndex = 0; nXIndex < forests.GetSizeX(); nXIndex += nAmbientPeriod )
				{
					for ( int nYIndex = 0; nYIndex < forests.GetSizeY(); nYIndex += nAmbientPeriod )
					{
						int nWoodsCount = 0;

						for ( int nForestXIndex = 0; nForestXIndex < nAmbientPeriod; ++nForestXIndex )
						{
							for ( int nForestYIndex = 0; nForestYIndex < nAmbientPeriod; ++nForestYIndex )
							{
								nWoodsCount += forests[nYIndex + nForestYIndex][nXIndex + nForestXIndex];
							}
						}
						
						if ( ( nWoodsCount >= nMinWoodsCount ) && ( ( rand() * 1.0f / ( RAND_MAX + 1 ) ) <= bAmbientRatio ) )
						{
							CMapSoundInfo soundInfo;
							soundInfo.szName = rLoadMapInfo.szForestAmbientSounds;
							soundInfo.vPos = VNULL3;
							
							if ( bExactAmbientPoint )
							{
								nWoodsCount = 0;

								for ( int nWoodsXIndex = ( nXIndex * nDelimiter ); nWoodsXIndex < ( ( nXIndex + nAmbientPeriod ) * nDelimiter ); ++nWoodsXIndex )
								{
									for ( int nWoodsYIndex = ( nYIndex * nDelimiter ); nWoodsYIndex < ( ( nYIndex + nAmbientPeriod ) * nDelimiter ); ++nWoodsYIndex )
									{
										if ( woods[nWoodsYIndex][nWoodsXIndex] > 0 )
										{
											soundInfo.vPos.x += ( nWoodsXIndex + 0.5f ) * SAIConsts::TILE_SIZE;
											soundInfo.vPos.y += ( nWoodsYIndex + 0.5f ) * SAIConsts::TILE_SIZE;
											++nWoodsCount;
										}
									}
								}
								soundInfo.vPos.x /= nWoodsCount;
								soundInfo.vPos.y /= nWoodsCount;
							}
							else
							{
								soundInfo.vPos.x = ( nXIndex + 0.5f ) * nDelimiter * SAIConsts::TILE_SIZE;
								soundInfo.vPos.y = ( nYIndex + 0.5f ) * nDelimiter * SAIConsts::TILE_SIZE;
							}
							soundInfo.vPos += vAmbientShiftPoint;
							AI2Vis( &( soundInfo.vPos ) );

							pSoundsList->push_back( soundInfo );
						}
					}
				}
				for ( int nXIndex = 0; nXIndex < forests.GetSizeX(); nXIndex += nAmbientPeriod )
				{
					for ( int nYIndex = 0; nYIndex < forests.GetSizeY(); nYIndex += nAmbientPeriod )
					{
						if ( forests[nYIndex][nXIndex] >= nMinWoodsCount )
						{
							CMapSoundInfo soundInfo;
							soundInfo.szName = rLoadMapInfo.szForestAmbientSounds;
							soundInfo.vPos.x = ( nXIndex + ( nAmbientPeriod / 2.0f ) ) * nDelimiter * SAIConsts::TILE_SIZE;
							soundInfo.vPos.y = ( nYIndex + ( nAmbientPeriod / 2.0f ) ) * nDelimiter * SAIConsts::TILE_SIZE;
							soundInfo.vPos.z = 0;
							soundInfo.vPos += vAmbientShiftPoint;
							AI2Vis( &( soundInfo.vPos ) );
							pSoundsList->push_back( soundInfo );
						}
					}
				}
			}
		}
	}
	
	return true;
}
