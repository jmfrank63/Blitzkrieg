#ifndef __SCENARIOTRACKER2INTERNAL_H__
#define __SCENARIOTRACKER2INTERNAL_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "ScenarioTracker.h"
#include "PlayerScenarioInfo.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::pair< int, CObj<CPlayerScenarioInfo> > CPlayerScenarioInfoPair;
typedef std::list<CPlayerScenarioInfoPair> CPlayersList;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** player scenario info iterator
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlayerScenarioInfoIterator : public CTRefCount<IPlayerScenarioInfoIterator>
{
	const CPlayersList &players;
	CPlayersList::const_iterator itCurrPlayer;
public:
	CPlayerScenarioInfoIterator( const CPlayersList &players );
	// move to next player scenario info
	void STDCALL Next();
	// check, if we've reached end?
	bool STDCALL IsEnd() const;
	// get current iterator's player scenario info
	IPlayerScenarioInfo* STDCALL Get() const;
	// get current iterator's player ID
	int STDCALL GetID() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** scenario tracker
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioTracker2 : public CTRefCount<IScenarioTracker>
{
	OBJECT_SERVICE_METHODS( CScenarioTracker2 );
	DECLARE_SERIALIZE;
	// names usage statistics
	struct SNameUsageStats
	{
		std::string szName;									// name file name
		NTimer::STime timeLastUsage;				// время последнего использования этого имени
		int nUsedCounter;										// сколько раз это имя использовали
		int nUsage;													// сколько его используют сейчас
		//
		SNameUsageStats() 
			: timeLastUsage( 0 ), nUsedCounter( 0 ), nUsage( 0 ) {  }
		//
		SNameUsageStats& operator=( const SNameUsageStats &stats ) 
		{
			szName = stats.szName;
			timeLastUsage = stats.timeLastUsage;
			nUsedCounter = stats.nUsedCounter;
			nUsage = stats.nUsage;
			return *this;
		}
		//
		bool operator<( const SNameUsageStats &stats ) const
		{
			if ( timeLastUsage == stats.timeLastUsage ) 
				return nUsedCounter == stats.nUsedCounter ? nUsage < stats.nUsage : nUsedCounter < stats.nUsedCounter;
			else
				return timeLastUsage < stats.timeLastUsage;
		}
		//
		int operator&( IStructureSaver &ss )
		{
			CSaverAccessor saver = &ss;
			saver.Add( 1, &szName );
			saver.Add( 2, &timeLastUsage );
			saver.Add( 3, &nUsedCounter );
			saver.Add( 4, &nUsage );
			return 0;
		}
	};
	// oponent description
	struct SOpponentDesc
	{
		struct SRPGClassDesc
		{
			std::string szClassName;						// RPG class name
			enum EUnitRPGClass eRPGClass;				// RPG class constant
			std::vector<std::string> names;			// files with personal names
			//
			int operator&( IDataTree &ss );
		};
		//
		std::string szSide;										// side: USSR, German, Allies, etc.
		std::vector<SRPGClassDesc> classes;		// RPG classes descriptions
		//
		int operator&( IDataTree &ss );
	};
	// player
	CPlayersList players;									// all players in the scenario tracker
	CPtr<CPlayerScenarioInfo> pUserPlayer;// user player
	int nUserPlayerID;										// user player ID
	std::string szMinimumDifficulty;
	//
	mutable std::vector<SOpponentDesc> opponents;	// opponents descriptions
	typedef std::unordered_map<std::string, SNameUsageStats> CNamesUsageMap;
	CNamesUsageMap personalNamesUsage;		// personal names usage
	// current chapter script
	class Script *pChapterScript;					// chapter script
	std::string szChapterScriptFileName;	// current script file name
	// campaign, chapter, mission
	std::string szCurrCampaign;						// currently started campaign
	std::string szCurrChapter;						// current chapter name
	std::string szCurrMission;						// current mission name
	ECampaignType eCampaignType;					// campaign's type
	std::vector<std::string> templateMissions;	// all available template missions
	std::vector<std::list<std::string> > randomBonuses;
	// current mission GUID
	GUID guidMission;											//
	//
	std::string GetBestPersonalName( const std::string &szRPGStats, const std::string &szSide ) const;
	void AssignBestPersonalName( CScenarioUnit *pUnit, const std::string &szSide );
	bool LoadChapterScript( const std::string &szScriptFileName );
	void ProcessScriptChanges( const bool bPostMission );
	void LoadOpponents() const;
	void InitMinimumDifficulty();
public:
	CScenarioTracker2();
	//
	bool STDCALL Init( ISingleton *pSingleton );
	//
	// players
	//
	// add new player with 'nPlayerID'. ASSERT, if such player already exist
	IPlayerScenarioInfo* STDCALL AddPlayer( const int nPlayerID );
	// remove player with 'nPlayerID'
	bool STDCALL RemovePlayer( const int nPlayerID );
	// get player with 'nPlayerID'.
	IPlayerScenarioInfo* STDCALL GetPlayer( const int nPlayerID ) const;
	// set user player
	void STDCALL SetUserPlayer( const int nPlayerID );
	// get user player interface
	IPlayerScenarioInfo* STDCALL GetUserPlayer() const;
	int STDCALL GetUserPlayerID() const;
	// iterate through all players
	IPlayerScenarioInfoIterator* STDCALL CreatePlayerScenarioInfoIterator() const;
	//
	// campaign, chapters, missions
	//
	// start new campaign (for all players)
	void STDCALL StartCampaign( const std::string &szCampaignName, const ECampaignType eType );
	// start new chapter (and finish previous one)
	bool STDCALL StartChapter( const std::string &szChapterName );
	// start new mission
	void STDCALL StartMission( const std::string &szMissionName );
	// finish mission
	void STDCALL FinishMission( const EMissionFinishStatus eStatus );
	// current mission GUID
	const GUID& STDCALL GetCurrMissionGUID() const { return guidMission; }
	// 
	// minimum difficulty tracking
	//
	virtual void STDCALL UpdateMinimumDifficulty();
	const std::string & STDCALL GetMinimumDifficulty() const;
	//
	// campaign random mission templates
	//
	// number of available random mission templates
	int STDCALL GetNumRandomTemplates() const;
	// get template by number
	const std::string& STDCALL GetTemplateName( const int nIndex ) const;
	
	//удаляет все рандомные бонуса
	virtual void STDCALL ClearRandomBonuses( int nDifficulty );
	//добавляет рандомный бонус
	virtual bool STDCALL AddRandomBonus( int nDifficulty, const std::string &rszRandomBonus );
	//возвращает рандомный бонус, удаляя его из списка
	virtual std::string STDCALL GetRandomBonus( int nDifficulty );

	//
	IScenarioTracker* STDCALL Duplicate() const;
	int STDCALL operator&( IDataTree &ss );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SCENARIOTRACKER2INTERNAL_H__
