#include "StdAfx.h"

#include "ScenarioStatistics.h"

#include "../Misc/Checker.h"
#include "ScenarioTrackerTypes.h"
CMissionStatistics::CMissionStatistics() 
: values( STMT_NUM_ELEMENTS ), eStatus( MISSION_FINISH_UNKNOWN )
{  
}
const std::string& CMissionStatistics::GetName() const
{
	return szName;
}
int CMissionStatistics::GetValue( const int nType ) const
{
	CheckRange( values, nType );
	return values[nType];
}
void CMissionStatistics::AddValue( const int nType, const int nValue )
{
	CheckRange( values, nType );
	values[nType] += nValue;
}
void CMissionStatistics::SetValue( const int nType, const int nValue )
{
	CheckRange( values, nType );
	values[nType] = nValue;
}
EMissionFinishStatus CMissionStatistics::GetFinishStatus() const
{
	return eStatus;
}
const std::string& CMissionStatistics::GetKIAName( const int nIndex ) const
{
	CheckRange( kiaUnits, nIndex );
	return kiaUnits[nIndex].szOldName;
}
const std::string& CMissionStatistics::GetKIANewName( const int nIndex ) const
{
	CheckRange( kiaUnits, nIndex );
	return kiaUnits[nIndex].szNewName;
}
const std::string& CMissionStatistics::GetKIAStats( const int nIndex ) const
{
	CheckRange( kiaUnits, nIndex );
	return kiaUnits[nIndex].szRPGStats;
}
int CMissionStatistics::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szName );
	saver.Add( 2, &values );
	saver.Add( 3, &eStatus );
	saver.Add( 4, &kiaUnits );
	if ( saver.IsReading() ) 
	{
		if ( values.size() != STMT_NUM_ELEMENTS ) 
			values.resize( STMT_NUM_ELEMENTS );
		if ( values[STMT_OBJECTIVES_RECIEVED] < values[STMT_OBJECTIVES_COMPLETED] ) 
			values[STMT_OBJECTIVES_RECIEVED] = values[STMT_OBJECTIVES_COMPLETED];
	}
	return 0;
}
const std::string& CChapterStatistics::GetName() const
{
	return szName;
}
int CChapterStatistics::GetValue( const int nType ) const
{
	int nValue = 0;
	for ( CMissionStatisticsList::const_iterator it = missions.begin(); it != missions.end(); ++it )
	{
		if ( (*it)->GetFinishStatus() == MISSION_FINISH_WIN ) 
			nValue += (*it)->GetValue( nType );
	}
	return nValue;
}
int CChapterStatistics::GetNumMissions() const
{
	return missions.size();
}
IMissionStatistics* CChapterStatistics::GetMission( const int nIndex ) const
{
	CheckRange( missions, nIndex );
	return missions[nIndex];
}
int CChapterStatistics::GetNumKIA() const 
{ 
	int nAmount = 0;
	for ( CMissionStatisticsList::const_iterator it = missions.begin(); it != missions.end(); ++it )
	{
		if ( (*it)->GetFinishStatus() == MISSION_FINISH_WIN ) 
			nAmount += (*it)->GetNumKIA();
	}
	return nAmount; 
}
static const std::string szDummy;
const std::string& CChapterStatistics::GetKIAName( const int nIndex ) const
{
	int nAmount = 0;
	for ( CMissionStatisticsList::const_iterator it = missions.begin(); it != missions.end(); ++it )
	{
		if ( (*it)->GetFinishStatus() == MISSION_FINISH_WIN ) 
		{
			const int nNumKIA = (*it)->GetNumKIA();
			if ( (nAmount <= nIndex) && (nAmount + nNumKIA > nIndex) )
				return (*it)->GetKIAName( nIndex - nAmount );
			nAmount += (*it)->GetNumKIA();
		}
	}
	NI_ASSERT_T( false, "Invalid index for KIA name in chapter" );
	return szDummy;
}
const std::string& CChapterStatistics::GetKIANewName( const int nIndex ) const
{
	int nAmount = 0;
	for ( CMissionStatisticsList::const_iterator it = missions.begin(); it != missions.end(); ++it )
	{
		if ( (*it)->GetFinishStatus() == MISSION_FINISH_WIN ) 
		{
			const int nNumKIA = (*it)->GetNumKIA();
			if ( (nAmount <= nIndex) && (nAmount + nNumKIA > nIndex) )
				return (*it)->GetKIANewName( nIndex - nAmount );
			nAmount += (*it)->GetNumKIA();
		}
	}
	NI_ASSERT_T( false, "Invalid index for KIA new name in chapter" );
	return szDummy;
}
const std::string& CChapterStatistics::GetKIAStats( const int nIndex ) const
{
	int nAmount = 0;
	for ( CMissionStatisticsList::const_iterator it = missions.begin(); it != missions.end(); ++it )
	{
		if ( (*it)->GetFinishStatus() == MISSION_FINISH_WIN ) 
		{
			const int nNumKIA = (*it)->GetNumKIA();
			if ( (nAmount <= nIndex) && (nAmount + nNumKIA > nIndex) )
				return (*it)->GetKIAStats( nIndex - nAmount );
			nAmount += (*it)->GetNumKIA();
		}
	}
	NI_ASSERT_T( false, "Invalid index for KIA stats in chapter" );
	return szDummy;
}
int CChapterStatistics::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szName );
	saver.Add( 2, &missions );
	return 0;
}
const std::string& CCampaignStatistics::GetName() const
{
	return szName;
}
ECampaignType CCampaignStatistics::GetType() const
{
	return eType;
}
int CCampaignStatistics::GetValue( const int nType ) const
{
	int nValue = 0;
	for ( CChapterStatisticsList::const_iterator it = chapters.begin(); it != chapters.end(); ++it )
		nValue += (*it)->GetValue( nType );
	return nValue;
}
int CCampaignStatistics::GetNumChapters() const
{
	return chapters.size();
}
IChapterStatistics* CCampaignStatistics::GetChapter( const int nIndex ) const
{
	CheckRange( chapters, nIndex );
	return chapters[nIndex];
}
void CCampaignStatistics::SetName( const std::string &_szName, const ECampaignType _eType )
{
	szName = _szName;
	eType = _eType;
}
int CCampaignStatistics::GetNumKIA() const 
{ 
	int nAmount = 0;
	for ( CChapterStatisticsList::const_iterator it = chapters.begin(); it != chapters.end(); ++it )
		nAmount += (*it)->GetNumKIA();
	return nAmount; 
}
const std::string& CCampaignStatistics::GetKIAName( const int nIndex ) const
{
	int nAmount = 0;
	for ( CChapterStatisticsList::const_iterator it = chapters.begin(); it != chapters.end(); ++it )
	{
		const int nNumKIA = (*it)->GetNumKIA();
		if ( (nAmount <= nIndex) && (nAmount + nNumKIA > nIndex) )
			return (*it)->GetKIAName( nIndex - nAmount );
		nAmount += (*it)->GetNumKIA();
	}
	NI_ASSERT_T( false, "Invalid index for KIA name in campaign" );
	return szDummy;
}
const std::string& CCampaignStatistics::GetKIANewName( const int nIndex ) const
{
	int nAmount = 0;
	for ( CChapterStatisticsList::const_iterator it = chapters.begin(); it != chapters.end(); ++it )
	{
		const int nNumKIA = (*it)->GetNumKIA();
		if ( (nAmount <= nIndex) && (nAmount + nNumKIA > nIndex) )
			return (*it)->GetKIANewName( nIndex - nAmount );
		nAmount += (*it)->GetNumKIA();
	}
	NI_ASSERT_T( false, "Invalid index for KIA name in campaign" );
	return szDummy;
}
const std::string& CCampaignStatistics::GetKIAStats( const int nIndex ) const
{
	int nAmount = 0;
	for ( CChapterStatisticsList::const_iterator it = chapters.begin(); it != chapters.end(); ++it )
	{
		const int nNumKIA = (*it)->GetNumKIA();
		if ( (nAmount <= nIndex) && (nAmount + nNumKIA > nIndex) )
			return (*it)->GetKIAStats( nIndex - nAmount );
		nAmount += (*it)->GetNumKIA();
	}
	NI_ASSERT_T( false, "Invalid index for KIA stats in campaign" );
	return szDummy;
}
int CCampaignStatistics::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szName );
	saver.Add( 2, &chapters );
	saver.Add( 3, &eType );
	return 0;
}
