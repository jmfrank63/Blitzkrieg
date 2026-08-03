#ifndef __SCENARIOTRACKER2INTERNAL_H__
#define __SCENARIOTRACKER2INTERNAL_H__
#pragma ONCE
#include "ScenarioTracker.h"
#include "PlayerScenarioInfo.h"
enum EUnitRPGClass : unsigned int;
typedef std::pair< int, CObj<CPlayerScenarioInfo> > CPlayerScenarioInfoPair;
typedef std::list<CPlayerScenarioInfoPair> CPlayersList;
class CPlayerScenarioInfoIterator : public CTRefCount<IPlayerScenarioInfoIterator>
{
	const CPlayersList &players;
	CPlayersList::const_iterator itCurrPlayer;
public:
	CPlayerScenarioInfoIterator( const CPlayersList &players );
	void STDCALL Next();
	bool STDCALL IsEnd() const;
	IPlayerScenarioInfo* STDCALL Get() const;
	int STDCALL GetID() const;
};
class CScenarioTracker2 : public CTRefCount<IScenarioTracker>
{
	OBJECT_SERVICE_METHODS( CScenarioTracker2 );
	DECLARE_SERIALIZE;
	struct SNameUsageStats
	{
		std::string szName;									// name file name
		NTimer::STime timeLastUsage;				// ����� ���������� ������������� ����� �����
		int nUsedCounter;										// ������� ��� ��� ��� ������������
		int nUsage;													// ������� ��� ���������� ������
		SNameUsageStats() 
			: timeLastUsage( 0 ), nUsedCounter( 0 ), nUsage( 0 ) {  }
		SNameUsageStats& operator=( const SNameUsageStats &stats ) 
		{
			szName = stats.szName;
			timeLastUsage = stats.timeLastUsage;
			nUsedCounter = stats.nUsedCounter;
			nUsage = stats.nUsage;
			return *this;
		}
		bool operator<( const SNameUsageStats &stats ) const
		{
			if ( timeLastUsage == stats.timeLastUsage ) 
				return nUsedCounter == stats.nUsedCounter ? nUsage < stats.nUsage : nUsedCounter < stats.nUsedCounter;
			else
				return timeLastUsage < stats.timeLastUsage;
		}
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
	struct SOpponentDesc
	{
		struct SRPGClassDesc
		{
			std::string szClassName;						// RPG class name
			enum EUnitRPGClass eRPGClass;				// RPG class constant
			std::vector<std::string> names;			// files with personal names
			int operator&( IDataTree &ss );
		};
		std::string szSide;										// side: USSR, German, Allies, etc.
		std::vector<SRPGClassDesc> classes;		// RPG classes descriptions
		int operator&( IDataTree &ss );
	};
	CPlayersList players;									// all players in the scenario tracker
	CPtr<CPlayerScenarioInfo> pUserPlayer;// user player
	int nUserPlayerID;										// user player ID
	std::string szMinimumDifficulty;
	mutable std::vector<SOpponentDesc> opponents;	// opponents descriptions
	typedef std::unordered_map<std::string, SNameUsageStats> CNamesUsageMap;
	CNamesUsageMap personalNamesUsage;		// personal names usage
	class Script *pChapterScript;					// chapter script
	std::string szChapterScriptFileName;	// current script file name
	std::string szCurrCampaign;						// currently started campaign
	std::string szCurrChapter;						// current chapter name
	std::string szCurrMission;						// current mission name
	ECampaignType eCampaignType;					// campaign's type
	std::vector<std::string> templateMissions;	// all available template missions
	std::vector<std::list<std::string> > randomBonuses;
	GUID guidMission;											//
	std::string GetBestPersonalName( const std::string &szRPGStats, const std::string &szSide ) const;
	void AssignBestPersonalName( CScenarioUnit *pUnit, const std::string &szSide );
	bool LoadChapterScript( const std::string &szScriptFileName );
	void ProcessScriptChanges( const bool bPostMission );
	void LoadOpponents() const;
	void InitMinimumDifficulty();
	void AssignPlayerColors();
public:
	CScenarioTracker2();
	bool STDCALL Init( ISingleton *pSingleton );
	IPlayerScenarioInfo* STDCALL AddPlayer( const int nPlayerID );
	bool STDCALL RemovePlayer( const int nPlayerID );
	IPlayerScenarioInfo* STDCALL GetPlayer( const int nPlayerID ) const;
	void STDCALL SetUserPlayer( const int nPlayerID );
	IPlayerScenarioInfo* STDCALL GetUserPlayer() const;
	int STDCALL GetUserPlayerID() const;
	IPlayerScenarioInfoIterator* STDCALL CreatePlayerScenarioInfoIterator() const;
	void STDCALL StartCampaign( const std::string &szCampaignName, const ECampaignType eType );
	bool STDCALL StartChapter( const std::string &szChapterName );
	void STDCALL StartMission( const std::string &szMissionName );
	void STDCALL FinishMission( const EMissionFinishStatus eStatus );
	const GUID& STDCALL GetCurrMissionGUID() const { return guidMission; }
	virtual void STDCALL UpdateMinimumDifficulty();
	const std::string & STDCALL GetMinimumDifficulty() const;
	int STDCALL GetNumRandomTemplates() const;
	const std::string& STDCALL GetTemplateName( const int nIndex ) const;
	
	virtual void STDCALL ClearRandomBonuses( int nDifficulty );
	virtual bool STDCALL AddRandomBonus( int nDifficulty, const std::string &rszRandomBonus );
	virtual std::string STDCALL GetRandomBonus( int nDifficulty );

	IScenarioTracker* STDCALL Duplicate() const;
	int STDCALL operator&( IDataTree &ss );
};
#endif // __SCENARIOTRACKER2INTERNAL_H__
