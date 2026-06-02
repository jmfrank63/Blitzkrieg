#include "StdAfx.h"

#include "PlayerScenarioInfo.h"

#include "..\Misc\Checker.h"
#include "RPGStats.h"
#include "TextSystem.h"
#include "ScenarioTrackerTypes.h"
CScenarioUnit::CScenarioUnit()
: values( STUT_NUM_ELEMENTS ), currValues( STUT_NUM_ELEMENTS ), valueDiffs( STUT_NUM_ELEMENTS )
{
	bKilled = false;
	nScenarioID = -1;
}
void CScenarioUnit::SetValue( const int nType, const int nValue )
{
	CheckRange( currValues, nType );
	currValues[nType] = nValue;
	if ( nType == STUT_LEVEL ) 
		SetExpToNextLevel();
}
void CScenarioUnit::AddValue( const int nType, const int nValue )
{
	CheckRange( currValues, nType );
	currValues[nType] += nValue;
	if ( nType == STUT_LEVEL ) 
		SetExpToNextLevel();
}
int CScenarioUnit::GetValue( const int nType ) const
{
	CheckRange( currValues, nType );
	return currValues[nType];
}
int CScenarioUnit::GetValueDiff( const int nType ) const
{
	CheckRange( valueDiffs, nType );
	return valueDiffs[nType];
}
interface IText* CScenarioUnit::GetName() const
{
	return pName;
}
void CScenarioUnit::ChangeRPGStats( const std::string &szStatsName )
{
	CheckRPGStats( szStatsName );
	IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
	const SUnitBaseRPGStats *pOldRPG = NGDB::GetRPGStats<SUnitBaseRPGStats>( pGDB, szRPGStats.c_str() );
	const SAIExpLevel *pOldExpLevels = static_cast<const SAIExpLevel*>( pGDB->GetExpLevels(pOldRPG->type) );
	const SUnitBaseRPGStats *pNewRPG = NGDB::GetRPGStats<SUnitBaseRPGStats>( pGDB, szStatsName.c_str() );
	const SAIExpLevel *pNewExpLevels = static_cast<const SAIExpLevel*>( pGDB->GetExpLevels(pNewRPG->type) );
	NI_ASSERT_SLOW_TF( pOldExpLevels->levels.size() == pNewExpLevels->levels.size(), NStr::Format("Number of levels for the old stats \"%s\" and for the new stats \"%s\" are different", szRPGStats.c_str(), szStatsName.c_str()), return );
	const int nCurrLevel = currValues[STUT_LEVEL];
	const int nNextLevel = Clamp( nCurrLevel + 1, 0, int(pOldExpLevels->levels.size() - 1) );
	if ( nNextLevel > nCurrLevel ) 
	{
		const int nCurrLevelExp = pOldExpLevels->levels[nCurrLevel].nExp;
		const int nNextLevelExp = pOldExpLevels->levels[nNextLevel].nExp;
		const int nCurrExp = currValues[STUT_EXP];
		const float fExpPercents = float( nCurrExp - nCurrLevelExp ) / float( nNextLevelExp - nCurrLevelExp );
		const int nNewExp = pNewExpLevels->levels[nCurrLevel].nExp + fExpPercents * ( pNewExpLevels->levels[nNextLevel].nExp - pNewExpLevels->levels[nCurrLevel].nExp );
		values[STUT_EXP] = currValues[STUT_EXP] = nNewExp;
		values[STUT_EXP_NEXT_LEVEL] = currValues[STUT_EXP_NEXT_LEVEL] = pNewExpLevels->levels[nNextLevel].nExp;
		values[STUT_EXP_CURR_LEVEL] = currValues[STUT_EXP_CURR_LEVEL] = pNewExpLevels->levels[nCurrLevel].nExp;
	}
	else
	{
		values[STUT_EXP] = currValues[STUT_EXP] = pNewExpLevels->levels[nCurrLevel].nExp;
		values[STUT_EXP_NEXT_LEVEL] = currValues[STUT_EXP_NEXT_LEVEL] = pNewExpLevels->levels[nNextLevel].nExp;
		values[STUT_EXP_CURR_LEVEL] = currValues[STUT_EXP_CURR_LEVEL] = pNewExpLevels->levels[nCurrLevel].nExp;
	}
	szRPGStats = szStatsName;
}
const std::string& CScenarioUnit::GetRPGStats() const
{
	return szRPGStats;
}
void CScenarioUnit::BeginMission()
{
	currValues = values;
	valueDiffs.assign( valueDiffs.size(), 0 );
}
void CScenarioUnit::AcceptMission()
{
	for ( int i = 0; i < values.size(); ++i )
		valueDiffs[i] = currValues[i] - values[i];
	values = currValues;
}
void CScenarioUnit::ClearMission()
{
	currValues = values;
	valueDiffs.assign( valueDiffs.size(), 0 );
	bKilled = false;
}
void CScenarioUnit::SetExpToNextLevel()
{
	IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
	const SUnitBaseRPGStats *pRPG = NGDB::GetRPGStats<SUnitBaseRPGStats>( pGDB, szRPGStats.c_str() );
	const SAIExpLevel *pExpLevel = static_cast<const SAIExpLevel*>( pGDB->GetExpLevels(pRPG->type) );
	const int nNextLevel = Clamp( currValues[STUT_LEVEL] + 1, 0, int(pExpLevel->levels.size() - 1) );
	currValues[STUT_EXP_NEXT_LEVEL] = pExpLevel->levels[nNextLevel].nExp;
	currValues[STUT_EXP_CURR_LEVEL] = pExpLevel->levels[currValues[STUT_LEVEL]].nExp;
}
void CScenarioUnit::Init( const int nID )
{
	SetExpToNextLevel();
	values[STUT_EXP_CURR_LEVEL] = 0;
	values[STUT_EXP_NEXT_LEVEL] = currValues[STUT_EXP_NEXT_LEVEL];
	nScenarioID = nID;
}
void CScenarioUnit::Reincarnate( const bool bLowerLevel )
{
	bKilled = false;
	if ( bLowerLevel ) 
	{
		IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
		const SUnitBaseRPGStats *pRPG = NGDB::GetRPGStats<SUnitBaseRPGStats>( pGDB, szRPGStats.c_str() );
		const SAIExpLevel *pExpLevel = static_cast<const SAIExpLevel*>( pGDB->GetExpLevels(pRPG->type) );
		const int nLevel = Clamp( currValues[STUT_LEVEL] - 1, 0, int(pExpLevel->levels.size() - 1) );
		currValues[STUT_LEVEL] = nLevel;
		currValues[STUT_EXP] = pExpLevel->levels[nLevel].nExp;
		SetExpToNextLevel();
	}
}
void CScenarioUnit::SetPersonalName( const std::string &szName )
{
	szNameFileName = szName;
	pName = GetSingleton<ITextManager>()->GetDialog( szNameFileName.c_str() );
}
int CScenarioUnit::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szRPGStats );
	saver.Add( 2, &values );
	saver.Add( 3, &currValues );
	saver.Add( 4, &pName );
	saver.Add( 5, &szNameFileName );
	saver.Add( 6, &bKilled );
	saver.Add( 7, &nScenarioID );
	saver.Add( 8, &valueDiffs );
	if ( saver.IsReading() ) 
	{
		if ( values.size() != STUT_NUM_ELEMENTS )
			values.resize( STUT_NUM_ELEMENTS );
		if ( currValues.size() != STUT_NUM_ELEMENTS )
			currValues.resize( STUT_NUM_ELEMENTS );
		if ( valueDiffs.size() != STUT_NUM_ELEMENTS )
			valueDiffs.resize( STUT_NUM_ELEMENTS );
	}
	return 0;
}
struct SRankInfo2
{
	double fExperience;
	std::string szRank;
	std::string szFullText;
	std::string szPicture;
	int operator&( IDataTree &ss )
	{
		CTreeAccessor tree = &ss;
		tree.Add( "RankKey", &szRank );
		tree.Add( "FullText", &szFullText );
		tree.Add( "ExperienceNeeded", &fExperience );
		tree.Add( "Picture", &szPicture );
		return 0;
	}
};
CPlayerScenarioInfo::CPlayerScenarioInfo()
: medalSlots( NUM_MEDAL_SLOTS )
{
	bGainLevel = false;
	dwColor = 0xffffffff;
	fExperience = 0;
	nDiplomacySide = 0;
}
void CPlayerScenarioInfo::Init()
{
	skills.clear();
	skills.push_back( SPlayerSkill( "Textes\\PlayerSkills\\Tactics", 0.0f, 0.0f ) );
	skills.push_back( SPlayerSkill( "Textes\\PlayerSkills\\Logistics", 0.0f, 0.0f ) );
	skills.push_back( SPlayerSkill( "Textes\\PlayerSkills\\Carefulness", 0.0f, 0.0f ) );
	skills.push_back( SPlayerSkill( "Textes\\PlayerSkills\\Staff", 0.0f, 0.0f ) );
	skills.push_back( SPlayerSkill( "Textes\\PlayerSkills\\ArtOfWar", 0.0f, 0.0f ) );
	skills.push_back( SPlayerSkill( "Textes\\PlayerSkills\\Duty", 0.0f, 0.0f ) );
	for ( std::vector<SPlayerSkill>::iterator it = skills.begin(); it != skills.end(); ++it )
		it->NormalizeValues( true );
}
void CPlayerScenarioInfo::SetName( const std::wstring &_wszName )
{
	wszName = _wszName;
	if ( pNameObject == 0 ) 
		pNameObject = CreateObject<IText>( TEXT_STRING );
	pNameObject->SetText( reinterpret_cast<const WORD*>( wszName.c_str() ) );
}
const std::wstring& CPlayerScenarioInfo::GetName() const
{
	return wszName;
}
IText* STDCALL CPlayerScenarioInfo::GetNameObject() const
{
	return pNameObject;
}
struct SGeneralPartyName
{
	std::string szName;
	std::string szGeneralName;
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "PartyName", &szName );
		saver.Add( "GeneralPartyName", &szGeneralName );
		NStr::ToLower( szName );
		NStr::ToLower( szGeneralName );
		return 0;
	}
};
void CPlayerScenarioInfo::SetSide( const std::string &szSideName )
{
	szSide = szSideName;
	NStr::ToLower( szSide );
	szGeneralSide = szSide;
	{
		std::vector<SGeneralPartyName> partyGeneralNames;
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( "partys.xml" , STREAM_ACCESS_READ );
		CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
		tree.Add( "PartyInfo", &partyGeneralNames );
		for ( std::vector<SGeneralPartyName>::const_iterator it = partyGeneralNames.begin(); it != partyGeneralNames.end(); ++it )
		{
			if ( it->szName == szSide ) 
			{
				szGeneralSide = it->szGeneralName;
				break;
			}
		}
	}
	pSideName = GetSingleton<ITextManager>()->GetDialog( ("textes\\opponents\\" + szSide).c_str() );
	if ( pSideName == 0 ) 
	{
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, NStr::Format("Can't find localized party name for \"%s\"", szSide.c_str()),
			                                          0xffff0000, true );
	}
	SetExperience( fExperience );	// reset rank name acording to side name
}
const std::string& CPlayerScenarioInfo::GetSide() const
{
	return szSide;
}
const std::string& CPlayerScenarioInfo::GetGeneralSide() const
{
	return szGeneralSide;
}
interface IText* CPlayerScenarioInfo::GetSideName() const
{
	return pSideName;
}
void CPlayerScenarioInfo::SetDiplomacySide( const int _nDiplomacySide )
{
	nDiplomacySide = _nDiplomacySide;
}
const int CPlayerScenarioInfo::GetDiplomacySide() const
{
	return nDiplomacySide;
}
void CPlayerScenarioInfo::SetColor( const DWORD _dwColor )
{
	dwColor = _dwColor;
}
DWORD CPlayerScenarioInfo::GetColor() const
{
	return dwColor;
}
const struct SPlayerSkill& CPlayerScenarioInfo::GetSkill( const int nSkill ) const
{
	CheckRange( skills, nSkill );
	return skills[nSkill];
}
void CPlayerScenarioInfo::SetSkill( const int nSkill, const float fVal )
{
	CheckRange( skills, nSkill );
	skills[nSkill].fFormerValue = skills[nSkill].fValue;
	skills[nSkill].fValue = Clamp( fVal, 0.0f, 1.0f );
	skills[nSkill].NormalizeValues( false );
}
const struct SPlayerRank& CPlayerScenarioInfo::GetRankInfo() const
{
	return rank;
}
void CPlayerScenarioInfo::ClearLevelGain()
{
	bGainLevel = false;
}
bool CPlayerScenarioInfo::IsGainLevel() const
{
	return bGainLevel;
}
bool CPlayerScenarioInfo::SetExperience( const double _fExperience )
{
	fExperience = _fExperience;
	std::vector<SRankInfo2> rankInfos;

	const std::string szPath = "medals\\" + szGeneralSide + "\\ranks.xml";

	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szPath.c_str(), STREAM_ACCESS_READ );
	if ( pStream == 0 ) 
		return false;
	CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
	tree.Add( "Ranks", &rankInfos );

	NI_ASSERT_T( rankInfos.size() >= 1, NStr::Format("wrong rank size, must have at least 1 base rank, wrong file %s", szPath.c_str() ) );
	
	int nRank = rankInfos.size() - 1;
	for ( int i = 1; i < rankInfos.size(); ++i )
	{
		if ( rankInfos[i].fExperience > fExperience )
		{
			nRank = i - 1;
			break;
		}
	}
	
	nRank = Clamp( nRank, 0, static_cast<int>(rankInfos.size() - 1) ); //current rank

	bGainLevel |= ( nRank != rank.nRankNumber );
	rank.nRankNumber = nRank;
	bGainLevel &= ( fExperience != 0 );	// cannot gain level when exp == 0

	rank.szCurrentRank = rankInfos[nRank].szRank;
	rank.szFullTextKey = rankInfos[nRank].szFullText;
	rank.szRankPicture = rankInfos[nRank].szPicture;
	
	const int nNextRank = Min( nRank + 1, static_cast<int>(rankInfos.size() - 1) );

	if ( IsGainLevel() )
		rank.fFormerValue = 0.0f;
	else 
		rank.fFormerValue = rank.fValue;

	if ( nNextRank == nRank ) // player reached maximum possible level.
		rank.fValue = 1.0f;
	else
	{
		const float fExpDiff = rankInfos[nNextRank].fExperience - rankInfos[nRank].fExperience;
		const float fExpCur = fExperience - rankInfos[nRank].fExperience;
		rank.fValue = static_cast<float>( fExpCur / fExpDiff );
	}
	return IsGainLevel();
}
int CPlayerScenarioInfo::GetNumUnits() const
{
	return units.size();
}
IScenarioUnit* CPlayerScenarioInfo::GetUnit( const int nIndex ) const
{
	CheckRange( units, nIndex );
	return nIndex >= 0 && nIndex < units.size() ? units[nIndex] : 0;
}
int CPlayerScenarioInfo::GetNumNewUnits() const
{
	return newUnits.size();
}
IScenarioUnit* CPlayerScenarioInfo::GetNewUnit( const int nIndex ) const
{
	CheckRange( newUnits, nIndex );
	CheckRange( units, newUnits[nIndex] );
	return units[ newUnits[nIndex] ];
}
const std::string& CPlayerScenarioInfo::GetMedalInSlot( const int nSlot ) const
{
	CheckRange( medalSlots, nSlot );
	return medalSlots[nSlot];
}
bool CPlayerScenarioInfo::HasMedal( const std::string &szName ) const
{
	for ( std::vector<std::string>::const_iterator it = medalSlots.begin(); it != medalSlots.end(); ++it )
	{
		if ( *it == szName ) 
			return true;
	}
	return false;
}
int CPlayerScenarioInfo::GetNumNewMedals() const
{
	return newMedals.size();
}
const std::string& CPlayerScenarioInfo::GetNewMedal( const int nIndex ) const
{
	CheckRange( newMedals, nIndex );
	CheckRange( medalSlots, newMedals[nIndex] );
	return medalSlots[ newMedals[nIndex] ];
}
const std::string& CPlayerScenarioInfo::GetUpgrade() const
{
	return szUpgrade;
}
int CPlayerScenarioInfo::GetNumDepotUpgrades() const
{
	return depotUpgrades.size();
}
int CPlayerScenarioInfo::GetNumNewDepotUpgrades() const
{
	return depotNewUpgrades.size();
}
const std::string& CPlayerScenarioInfo::GetNewDepotUpgrade( const int nIndex ) const
{
	return depotNewUpgrades[nIndex];
}
const std::string& CPlayerScenarioInfo::GetDepotUpgrade( const int nIndex ) const
{
	CheckRange( depotUpgrades, nIndex );
	return depotUpgrades[nIndex];
}
void CPlayerScenarioInfo::OrderDepotUpgrade( const int nIndex )
{
	CheckRange( depotUpgrades, nIndex );
	szUpgrade = depotUpgrades[nIndex];
}
ICampaignStatistics* CPlayerScenarioInfo::GetCampaignStats() const
{
	return pCampaignStats;
}
IChapterStatistics* CPlayerScenarioInfo::GetChapterStats() const
{
	return pChapterStats;
}
IMissionStatistics* CPlayerScenarioInfo::GetMissionStats() const
{
	return pMissionStats;
}
void CPlayerScenarioInfo::StartCampaign( CCampaignStatistics *pStats )
{
	pCampaignStats = pStats;
	pChapterStats = 0;
	pMissionStats = 0;
}
void CPlayerScenarioInfo::StartChapter( CChapterStatistics *pStats )
{
	NI_ASSERT_SLOW_T( pCampaignStats != 0, "Can't start chapter - campaign not started" );
	pCampaignStats->AddChapter( pStats );
	pChapterStats = pStats;
	pMissionStats = 0;
}
void CPlayerScenarioInfo::StartMission( CMissionStatistics *pStats )
{
	NI_ASSERT_SLOW_T( pChapterStats != 0, "Can't start mission - chapter not started" );
	pChapterStats->AddMission( pStats );
	pMissionStats = pStats;
	for ( CUnitsList::iterator it = units.begin(); it != units.end(); ++it )
		(*it)->BeginMission();
	newMedals.clear();
	newUnits.clear();
	ClearLevelGain();
}
void CPlayerScenarioInfo::FinishMission( const EMissionFinishStatus eStatus )
{
	pMissionStats->SetFinishStatus( eStatus );
	if ( eStatus == MISSION_FINISH_WIN ) 
	{
		for ( CUnitsList::iterator it = units.begin(); it != units.end(); ++it )
			(*it)->AcceptMission();
	}
	else
	{
		for ( CUnitsList::iterator it = units.begin(); it != units.end(); ++it )
			(*it)->ClearMission();
	}
}
CScenarioUnit* CPlayerScenarioInfo::AddNewSlot( const std::string &szRPGStats )
{
	CScenarioUnit *pUnit = CreateObject<CScenarioUnit>( MAIN_SCENARIO_UNIT );
	pUnit->SetRPGStats( szRPGStats );
	units.push_back( pUnit );
	pUnit->Init( units.size() - 1 );
	return pUnit;
}
void CPlayerScenarioInfo::SetUpgrade( const std::string &szUpgradeRPGStats )
{
	szUpgrade = szUpgradeRPGStats;
}
void CPlayerScenarioInfo::AddDepotUpgrade( const std::string &szRPGStats )
{
	if ( std::find(depotUpgrades.begin(), depotUpgrades.end(), szRPGStats) == depotUpgrades.end() ) 
	{
		depotUpgrades.push_back( szRPGStats );
		depotNewUpgrades.push_back( szRPGStats );
	}
}
void CPlayerScenarioInfo::ClearNewDepotUpgrade()
{
	depotNewUpgrades.clear();
}
void CPlayerScenarioInfo::RemoveDepotUpgrade( const std::string &szRPGStats )
{
	std::vector<std::string>::iterator pos = std::find( depotUpgrades.begin(), depotUpgrades.end(), szRPGStats );
	if ( pos != depotUpgrades.end() ) 
		depotUpgrades.erase( pos );
}
void CPlayerScenarioInfo::AddMedal( const std::string &szMedal, const int nSlot )
{
	CheckRange( medalSlots, nSlot );
	medalSlots[nSlot] = szMedal;
	if ( std::find(newMedals.begin(), newMedals.end(), nSlot) == newMedals.end() )
		newMedals.push_back( nSlot );
}
int CPlayerScenarioInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &wszName );
	saver.Add( 2, &pNameObject );
	saver.Add( 3, &szSide );
	saver.Add( 4, &szGeneralSide );
	saver.Add( 5, &pSideName );
	saver.Add( 6, &nDiplomacySide );
	saver.Add( 7, &dwColor );
	saver.Add( 8, &skills );
	saver.Add( 9, &rank );
	saver.Add( 10, &fExperience );
	saver.Add( 11, &bGainLevel );
	saver.Add( 12, &units );
	saver.Add( 13, &medalSlots );
	saver.Add( 14, &newMedals );
	saver.Add( 15, &szUpgrade );
	saver.Add( 16, &depotUpgrades );
	saver.Add( 17, &pCampaignStats );
	saver.Add( 18, &pChapterStats );
	saver.Add( 19, &pMissionStats );
	saver.Add( 20, &depotNewUpgrades );
	return 0;
}
