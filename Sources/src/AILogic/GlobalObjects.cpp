#include "StdAfx.h"

#include "GlobalObjects.h"

#include "AIStaticMap.h"
#include "GroupLogic.h"
#include "Units.h"
#include "Updater.h"
#include "StaticObjects.h"
#include "Shell.h"
#include "Diplomacy.h"
#include "AIWarFog.h"
#include "UnitCreation.h"
#include "EntrenchmentCreation.h"
#include "AntiArtilleryManager.h"
#include "Cheats.h"
#include "AILogicInternal.h"
#include "HitsStore.h"
#include "PathFinder.h"
#include "AckManager.h"
#include "SuspendedUpdates.h"
#include "CombatEstimator.h"
#include "Statistics.h"
#include "GeneralInternal.h"
#include "TrainPathFinder.h"
#include "RailroadGraph.h"
#include "Weather.h"
#include "DifficultyLevel.h"
#include "Graveyard.h"
#include "MultiplayerInfo.h"
#include "AAFeedBacks.h"
extern CAAFeedBacks theAAFeedBacks;
extern CSupremeBeing theSupremeBeing;
extern CCombatEstimator theCombatEstimator;
extern CSuspendedUpdates theSuspendedUpdates;
extern CAckManager theAckManager;
extern CStaticMap theStaticMap;
extern CGroupLogic theGroupLogic;
extern CUnits units;
extern CUpdater updater;
extern CStaticObjects theStatObjs;
extern CPtr<IStaticPathFinder> pThePathFinder;
extern CPtr<IStaticPathFinder> pThePlanePathFinder;
extern CPtr<IStaticPathFinder> pTheTrainPathFinder;
extern NTimer::STime curTime;
extern CHitsStore theHitsStore;
extern CDiplomacy theDipl;
extern CGlobalWarFog theWarFog;
extern CUnitCreation theUnitCreation;
extern CAntiArtilleryManager theAAManager;
extern SCheats theCheats;
extern CAILogic *pAILogic;
extern CShellsStore theShellsStore;
extern CStatistics theStatistics;
extern CRailroadGraph theRailRoadGraph;
extern CWeather theWeather;
extern CDifficultyLevel theDifficultyLevel;
extern CGraveyard theGraveyard;
extern CMultiplayerInfo theMPInfo;
void NGlobalObjects::Clear()
{
	pThePathFinder = 0;
	pThePlanePathFinder = 0;
	pTheTrainPathFinder = 0;

	theGraveyard.Clear();
	theStaticMap.Clear();
	theGroupLogic.Clear();
	units.Clear();
	updater.Clear();
	theStatObjs.Clear();
	theHitsStore.Clear();
	theDipl.Clear();
	theWarFog.Clear();
	theUnitCreation.Clear();
	theAAManager.Clear();
	theCheats.Clear();
	theShellsStore.Clear();
	theAckManager.Clear();
	theSuspendedUpdates.Clear();
	theRailRoadGraph.Clear();
	theWeather.Clear();

	theCombatEstimator.Clear();
	theSupremeBeing.Clear();
	theMPInfo.Clear();
	theWeather.Clear();
	theGraveyard.Clear();
	theDifficultyLevel.Clear();

	theAAFeedBacks.Clear();
}
class CGlobalSerializer
{
	DECLARE_SERIALIZE;
};
int CGlobalSerializer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	if ( saver.IsReading() )
		SConsts::Load();

	saver.Add( 1, &pThePathFinder );
	saver.Add( 2, &pThePlanePathFinder );

	saver.Add( 3, &theStaticMap );
	saver.Add( 4, &theGroupLogic );
	saver.Add( 5, &units );
	saver.Add( 6, &updater );
	saver.Add( 7, &theStatObjs );
	saver.Add( 8, &theHitsStore );
	saver.Add( 9, &theDipl );
	saver.Add( 10, &theWarFog );
	saver.Add( 11, &theUnitCreation );
	saver.Add( 14, &theAAManager );
	saver.Add( 15, &theCheats );
	saver.Add( 16, &theShellsStore );
	saver.Add( 17, &theAckManager );
	saver.Add( 18, &theSuspendedUpdates );
	saver.Add( 19, &theCombatEstimator );
	saver.Add( 20, &theStatistics );
	saver.Add( 21, &theSupremeBeing );
	saver.Add( 22, &theRailRoadGraph );
	saver.Add( 23, &pTheTrainPathFinder );
	saver.Add( 24, &theWeather );
	
	saver.Add( 25, &theDifficultyLevel );
	saver.Add( 26, &theGraveyard );
	saver.Add( 27, &theAAFeedBacks );

	return 0;
}
void NGlobalObjects::Serialize( SSChunkID idChunk, IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	CGlobalSerializer globalSerializer;
	saver.Add( idChunk, &globalSerializer );
}
