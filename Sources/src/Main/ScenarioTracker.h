#ifndef __SCENARIOTRACKER2_H__
#define __SCENARIOTRACKER2_H__
#pragma ONCE
#include "iMainClassIDs.h"
enum EPlayerRelation : unsigned int;
#define NUM_MEDAL_SLOTS 6
enum EMissionFinishStatus
{
	MISSION_FINISH_UNKNOWN	= -1,
	MISSION_FINISH_WIN			= 0,
	MISSION_FINISH_LOSE			= 1,
	MISSION_FINISH_ABORT		= 2,
	MISSION_FINISH_RESTART	= 3,

	MISSION_FINISH_FORCE_DWORD = 0x7fffffff
};
enum ECampaignType
{
	CAMPAIGN_TYPE_UNKNOWN					= -1,
	CAMPAIGN_TYPE_SINGLE					= 0,
	CAMPAIGN_TYPE_MULTIPLAYER			= 1,
	CAMPAIGN_TYPE_CUSTOM_CAMPAIGN	= 2,
	CAMPAIGN_TYPE_CUSTOM_CHAPTER	= 3,
	CAMPAIGN_TYPE_CUSTOM_MISSION	= 4,
	CAMPAIGN_TYPE_TUTORIAL				= 5,

	CAMPAIGN_FORCE_DWORD = 0x7fffffff
};
enum EPlayerSkillType
{
  EPST_TACTICS                          = 0,
  EPST_LOGISTICS                        = 1,
  EPST_CAREFULNESS                      = 2,
  EPST_STAFF                            = 3,
  EPST_ARTOFWAR                         = 4,
  EPST_DUTY                             = 5,
  _EPST_COUNT                           = 6,
};
enum EStatsComplexity
{
	STATS_COMPLEXITY_TOTAL		= 0,
	STATS_COMPLEXITY_CHAPTER	= 1,
	STATS_COMPLEXITY_MISSION	= 2,

	STATS_COMPLEXITY_FORCE_DWORD = 0x7fffffff
};
interface IUserProfile : public IRefCount
{
	enum { tidTypeID = MAIN_USER_PROFILE };
	
	virtual bool STDCALL IsHelpCalled( const int nInterfaceTypeID, const int nHelpNumber ) const = 0;
	virtual void STDCALL HelpCalled( const int nInterfaceTypeID, const int nHelpNumber ) = 0;
	virtual void STDCALL AddCutScene( const std::string &szCutSceneName ) = 0;
	virtual int STDCALL GetNumCutScenes() const = 0;
	virtual const std::string& STDCALL GetCutScene( const int nIndex ) const = 0;
	virtual void STDCALL AddUsedTemplate( const std::string &rszTemplate, int nTemplateWeight, const std::string &rszGraph, int nGraphWeight, int nAngle, int nAngleWeight ) = 0;
	virtual int STDCALL GetUsedTemplates( const std::string &rszTemplate ) = 0;
	virtual int STDCALL GetUsedTemplateGraphs( const std::string &rszTemplate, const std::string &rszGraph ) = 0;
	virtual int STDCALL GetUsedAngles( const int nAngle ) = 0;
	virtual void STDCALL SetChatRelation( const wchar_t *pwszNick, const enum EPlayerRelation eRelation ) = 0;
	virtual const enum EPlayerRelation STDCALL GetChatRelation( const wchar_t *pwszNick ) = 0;
	virtual void STDCALL SetMOD( const std::string &szMOD ) = 0;
	virtual const std::string& STDCALL GetMOD() const = 0;
	virtual void STDCALL RegisterLoad( const GUID &guid ) = 0;
	virtual int STDCALL GetLoadCounter( const GUID &guid ) const = 0;

	virtual void STDCALL AddVar( const char *pszValueName, const int nValue ) = 0;
	virtual int STDCALL GetVar( const char *pszValueName, const int nDefValue ) const = 0;
	virtual void STDCALL RemoveVar( const char *pszValueName ) = 0;

	virtual bool STDCALL IsChanged() const = 0;
	virtual void STDCALL SerializeConfig( IDataTree *pSS ) = 0;
	virtual void STDCALL Repair( IDataTree *pSS, const bool bToDefault ) = 0;
};
interface IScenarioUnit : public IRefCount
{
	virtual void STDCALL SetValue( const int nType, const int nValue ) = 0;
	virtual void STDCALL AddValue( const int nType, const int nValue ) = 0;
	virtual int STDCALL GetValue( const int nType ) const = 0;
	virtual int STDCALL GetValueDiff( const int nType ) const = 0;
	virtual void STDCALL Kill() = 0;
	virtual interface IText* STDCALL GetName() const = 0;
	virtual void STDCALL ChangeRPGStats( const std::string &szStatsName ) = 0;
	virtual const std::string& STDCALL GetRPGStats() const = 0;
	virtual int STDCALL GetScenarioID() const = 0;
};
interface IPlayerScenarioInfo : public IRefCount
{
	virtual void STDCALL SetName( const std::wstring &wszName ) = 0;
	virtual const std::wstring& STDCALL GetName() const = 0;
	virtual IText* STDCALL GetNameObject() const = 0;
	virtual void STDCALL SetSide( const std::string &szSideName ) = 0;
	virtual const std::string& STDCALL GetSide() const = 0;
	virtual const std::string& STDCALL GetGeneralSide() const = 0;
	virtual interface IText* STDCALL GetSideName() const = 0;
	virtual void STDCALL SetDiplomacySide( const int nDiplomacySide ) = 0;
	virtual const int STDCALL GetDiplomacySide() const = 0;
	virtual void STDCALL SetColor( const DWORD dwColor ) = 0;
	virtual DWORD STDCALL GetColor() const = 0;
	virtual const struct SPlayerSkill& STDCALL GetSkill( const int nSkill ) const = 0;
	virtual void STDCALL SetSkill( const int nSkill, const float fVal ) = 0;
	virtual const struct SPlayerRank& STDCALL GetRankInfo() const = 0;
	virtual void STDCALL ClearLevelGain() = 0;
	virtual bool STDCALL IsGainLevel() const = 0;
	virtual bool STDCALL SetExperience( const double fExperience ) = 0;
	virtual int STDCALL GetNumUnits() const = 0;
	virtual IScenarioUnit* STDCALL GetUnit( const int nIndex ) const = 0;
	virtual int STDCALL GetNumNewUnits() const = 0;
	virtual IScenarioUnit* STDCALL GetNewUnit( const int nIndex ) const = 0;
	virtual const std::string& STDCALL GetMedalInSlot( const int nSlot ) const = 0;
	virtual bool STDCALL HasMedal( const std::string &szName ) const = 0;
	virtual int STDCALL GetNumNewMedals() const = 0;
	virtual const std::string& STDCALL GetNewMedal( const int nIndex ) const = 0;
	virtual const std::string& STDCALL GetUpgrade() const = 0;
	virtual int STDCALL GetNumDepotUpgrades() const = 0;
	virtual const std::string& STDCALL GetDepotUpgrade( const int nIndex ) const = 0;
	virtual void STDCALL OrderDepotUpgrade( const int nUpgradeIndex ) = 0;
	virtual int STDCALL GetNumNewDepotUpgrades() const = 0;
	virtual const std::string& STDCALL GetNewDepotUpgrade( const int nIndex ) const = 0;
	virtual void STDCALL ClearNewDepotUpgrade() = 0;
	virtual interface ICampaignStatistics* STDCALL GetCampaignStats() const = 0;
	virtual interface IChapterStatistics* STDCALL GetChapterStats() const = 0;
	virtual interface IMissionStatistics* STDCALL GetMissionStats() const = 0;
};
interface IPlayerScenarioInfoIterator : public IRefCount
{
	virtual void STDCALL Next() = 0;
	virtual bool STDCALL IsEnd() const = 0;
	virtual IPlayerScenarioInfo* STDCALL Get() const = 0;
	virtual int STDCALL GetID() const = 0;
};
interface IScenarioStatistics : public IRefCount
{
	virtual const std::string& STDCALL GetName() const = 0;
	virtual int STDCALL GetValue( const int nType ) const = 0;
	virtual int STDCALL GetNumKIA() const = 0;
	virtual const std::string& STDCALL GetKIAName( const int nIndex ) const = 0;
	virtual const std::string& STDCALL GetKIANewName( const int nIndex ) const = 0;
	virtual const std::string& STDCALL GetKIAStats( const int nIndex ) const = 0;
};
interface IMissionStatistics : public IScenarioStatistics
{
	virtual void STDCALL AddValue( const int nType, const int nValue ) = 0;
	virtual void STDCALL SetValue( const int nType, const int nValue ) = 0;
	virtual EMissionFinishStatus STDCALL GetFinishStatus() const = 0;
};
interface IChapterStatistics : public IScenarioStatistics
{
	virtual int STDCALL GetNumMissions() const = 0;
	virtual IMissionStatistics* STDCALL GetMission( const int nIndex ) const = 0;
};
interface ICampaignStatistics : public IScenarioStatistics
{
	virtual ECampaignType STDCALL GetType() const = 0;
	virtual int STDCALL GetNumChapters() const = 0;
	virtual IChapterStatistics* STDCALL GetChapter( const int nIndex ) const = 0;
};
interface IScenarioTracker : public IRefCount
{
	enum { tidTypeID = MAIN_SCENARIO_TRACKER };
	virtual bool STDCALL Init( ISingleton *pSingleton ) = 0;
	virtual IPlayerScenarioInfo* STDCALL AddPlayer( const int nPlayerID ) = 0;
	virtual bool STDCALL RemovePlayer( const int nPlayerID ) = 0;
	virtual IPlayerScenarioInfo* STDCALL GetPlayer( const int nPlayerID ) const = 0;
	virtual void STDCALL SetUserPlayer( const int nPlayerID ) = 0;
	virtual IPlayerScenarioInfo* STDCALL GetUserPlayer() const = 0;
	virtual int STDCALL GetUserPlayerID() const = 0;
	virtual IPlayerScenarioInfoIterator* STDCALL CreatePlayerScenarioInfoIterator() const = 0;
	virtual void STDCALL StartCampaign( const std::string &szCampaignName, const ECampaignType eType ) = 0;
	virtual bool STDCALL StartChapter( const std::string &szChapterName ) = 0;
	virtual void STDCALL StartMission( const std::string &szMissionName ) = 0;
	virtual void STDCALL FinishMission( const EMissionFinishStatus eStatus ) = 0;
	virtual const GUID& STDCALL GetCurrMissionGUID() const = 0;
	virtual void STDCALL UpdateMinimumDifficulty() = 0;
	virtual const std::string & STDCALL GetMinimumDifficulty() const = 0;
	virtual int STDCALL GetNumRandomTemplates() const = 0;
	virtual const std::string& STDCALL GetTemplateName( const int nIndex ) const = 0;

	virtual void STDCALL ClearRandomBonuses( int nDifficulty ) = 0;
	virtual bool STDCALL AddRandomBonus( int nDifficulty, const std::string &rszRandomBonus ) = 0;
	virtual std::string STDCALL GetRandomBonus( int nDifficulty ) = 0;

	virtual IScenarioTracker* STDCALL Duplicate() const = 0;
	virtual int STDCALL operator&( IDataTree &ss ) = 0;
};
#endif // __SCENARIOTRACKER2_H__
