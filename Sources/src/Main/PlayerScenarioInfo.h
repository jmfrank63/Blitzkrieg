#ifndef __PLAYERSCENARIOINFO_H__
#define __PLAYERSCENARIOINFO_H__
#pragma ONCE
#include "ScenarioTracker.h"
#include "PlayerSkill.h"
#include "ScenarioStatistics.h"
class CScenarioUnit : public CTRefCount<IScenarioUnit>
{
	OBJECT_SERVICE_METHODS( CScenarioUnit );
	DECLARE_SERIALIZE;
	std::string szRPGStats;								// unit's RPG stats
	std::vector<int> values;							// unit's values (exp, kills, etc.)
	std::vector<int> currValues;					// current mission values
	std::vector<int> valueDiffs;					// last mission diffs
	bool bKilled;													// unit was killed in mission
	CPtr<IText> pName;										// personal localized name
	std::string szNameFileName;						// file name with the localized name
	int nScenarioID;											// scenario ID
public:
	CScenarioUnit();
	void STDCALL SetValue( const int nType, const int nValue );
	void STDCALL AddValue( const int nType, const int nValue );
	int STDCALL GetValue( const int nType ) const;
	int STDCALL GetValueDiff( const int nType ) const;
	virtual void STDCALL Kill() { bKilled = true; }
	interface IText* STDCALL GetName() const;
	void STDCALL ChangeRPGStats( const std::string &szStatsName );
	const std::string& STDCALL GetRPGStats() const;
	virtual int STDCALL GetScenarioID() const { return nScenarioID; }
	bool IsKilled() const { return bKilled; }
	void Reincarnate( const bool bLowerLevel );
	void BeginMission();
	void AcceptMission();
	void ClearMission();
	void SetPersonalName( const std::string &szName );
	const std::string& GetPersonalNameFileName() const { return szNameFileName; }
	void SetRPGStats( const std::string &szNewRPGStats ) { szRPGStats = szNewRPGStats; }
	void Init( const int nID );
	void SetExpToNextLevel();
};
class CPlayerScenarioInfo : public CTRefCount<IPlayerScenarioInfo>
{
	OBJECT_SERVICE_METHODS( CPlayerScenarioInfo );
	DECLARE_SERIALIZE;
	std::wstring wszName;									// player name
	CPtr<IText> pNameObject;							// player name object
	std::string szSide;										// side (USSR, German, French, Poland, Italy, etc.)
	std::string szGeneralSide;						// general side (USSR, German, Allies)
	CPtr<IText> pSideName;								// localized side name
	int nDiplomacySide;										// diplomacy side [0..2]
	DWORD dwColor;												// color
	std::vector<SPlayerSkill> skills;			// skills.
	SPlayerRank	rank;											// current rank
	float fExperience;										// players experience
	bool bGainLevel;											// is player gained level?
	typedef std::vector< CObj<CScenarioUnit> > CUnitsList;
	CUnitsList units;											// all player's units
	std::vector<int> newUnits;						// new added units (after mission start)
	std::vector<std::string> medalSlots;	// slots for medals
	std::vector<int> newMedals;						// new came medals
	std::string szUpgrade;								// single upgrade after mission
	std::vector<std::string> depotUpgrades;// depot (endless) upgrades
	std::vector<std::string> depotNewUpgrades;	// new depot upggrades
	CObj<CCampaignStatistics> pCampaignStats;	// current campaign statistics for this player (main storage for all statistics)
	CPtr<CChapterStatistics> pChapterStats;		// current chapter statistics for this player (shortcur from campaign stats)
	CPtr<CMissionStatistics> pMissionStats;		// current mission statistics for this player (shortcut from chapter stats)
public:	
	CPlayerScenarioInfo();
	void Init();
	void STDCALL SetName( const std::wstring &wszName );
	const std::wstring& STDCALL GetName() const;
	IText* STDCALL GetNameObject() const;
	void STDCALL SetSide( const std::string &szSideName );
	const std::string& STDCALL GetSide() const;
	const std::string& STDCALL GetGeneralSide() const;
	interface IText* STDCALL GetSideName() const;
	void STDCALL SetDiplomacySide( const int nDiplomacySide );
	const int STDCALL GetDiplomacySide() const;
	void STDCALL SetColor( const DWORD dwColor );
	DWORD STDCALL GetColor() const;
	const struct SPlayerSkill& STDCALL GetSkill( const int nSkill ) const;
	void STDCALL SetSkill( const int nSkill, const float fVal );
	const struct SPlayerRank& STDCALL GetRankInfo() const;
	void STDCALL ClearLevelGain();
	bool STDCALL IsGainLevel() const;
	bool STDCALL SetExperience( const double fExperience );
	int STDCALL GetNumUnits() const;
	IScenarioUnit* STDCALL GetUnit( const int nIndex ) const;
	int STDCALL GetNumNewUnits() const;
	IScenarioUnit* STDCALL GetNewUnit( const int nIndex ) const;
	const std::string& STDCALL GetMedalInSlot( const int nSlot ) const;
	bool STDCALL HasMedal( const std::string &szName ) const;
	int STDCALL GetNumNewMedals() const;
	const std::string& STDCALL GetNewMedal( const int nIndex ) const;
	const std::string& STDCALL GetUpgrade() const;
	int STDCALL GetNumDepotUpgrades() const;
	const std::string& STDCALL GetDepotUpgrade( const int nIndex ) const;
	void STDCALL OrderDepotUpgrade( const int nUpgradeIndex );
	int STDCALL GetNumNewDepotUpgrades() const;
	const std::string& STDCALL GetNewDepotUpgrade( const int nIndex ) const;
	virtual void STDCALL ClearNewDepotUpgrade();
	ICampaignStatistics* STDCALL GetCampaignStats() const;
	IChapterStatistics* STDCALL GetChapterStats() const;
	IMissionStatistics* STDCALL GetMissionStats() const;
	void StartCampaign( CCampaignStatistics *pStats );
	void StartChapter( CChapterStatistics *pStats );
	void StartMission( CMissionStatistics *pStats );
	void FinishMission( const EMissionFinishStatus eStatus );
	CScenarioUnit* AddNewSlot( const std::string &szRPGStats );
	void SetUpgrade( const std::string &szUpgradeRPGStats );
	void AddDepotUpgrade( const std::string &szRPGStats );
	void RemoveDepotUpgrade( const std::string &szRPGStats );
	void AddMedal( const std::string &szMedal, const int nSlot );
};
#endif // __PLAYERSCENARIOINFO_H__
