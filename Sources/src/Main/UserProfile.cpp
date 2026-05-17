#include "StdAfx.h"

#include "UserProfile.h"

#include "..\Input\Input.h"
#include "..\StreamIO\OptionSystem.h"
#include "..\Misc\Checker.h"
#include "..\GameTT\MultiplayerCommandManager.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** help screen rtacker part
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUserProfile::IsHelpCalled( const int nInterfaceTypeID, const int nHelpNumber ) const
{
	CHelpscenes::const_iterator it = helpscreens.find( nInterfaceTypeID );
	return  it != helpscreens.end() && (it->second & (1 << nHelpNumber));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUserProfile::HelpCalled( const int nInterfaceTypeID, const int nHelpNumber )
{
	CHelpscenes::iterator it = helpscreens.find( nInterfaceTypeID );
	if ( it == helpscreens.end() )
	{
		helpscreens[nInterfaceTypeID] = 0;
		it = helpscreens.find( nInterfaceTypeID );
	}
	it->second |= 1<<nHelpNumber;
	SetChanged();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** cutscenes availability
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUserProfile::AddCutScene( const std::string &szCutSceneName )
{
	if ( std::find(cutscenes.begin(), cutscenes.end(), szCutSceneName) == cutscenes.end() ) 
	{
		cutscenes.push_back( szCutSceneName );
		SetChanged();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUserProfile::GetNumCutScenes() const
{
	return cutscenes.size();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const std::string& CUserProfile::GetCutScene( const int nIndex ) const
{
	CheckRange( cutscenes, nIndex );
	return cutscenes[nIndex];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** variables
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUserProfile::AddVar( const char *pszValueName, const int nValue )
{
	CVariables::iterator pos = variables.find( pszValueName );
	if ( ((pos != variables.end()) && (pos->second != nValue)) || (pos == variables.end()) ) 
	{
		variables[pszValueName] = nValue;
		SetChanged();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUserProfile::GetVar( const char *pszValueName, const int nDefValue ) const
{
	CVariables::const_iterator it = variables.find( pszValueName );
	if ( variables.end() == it )
		return nDefValue;
	return it->second;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUserProfile::RemoveVar( const char *pszValueName )
{
	SetChanged();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** templates usage statistics
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUserProfile::AddUsedTemplate( const std::string &szTemplate, int nTemplateWeight, const std::string &szGraph, int nGraphWeight, int nAngle, int nAngleWeight )
{
	if ( templates.find( szTemplate ) == templates.end() )
		templates[szTemplate].nCount = 0;

	STemplateUsage &usage = templates[szTemplate];
	usage.nCount += nTemplateWeight;
	if ( usage.graphs.find(szGraph) == usage.graphs.end() )
		usage.graphs[szGraph] = 0;
	usage.graphs[szGraph] += nGraphWeight;
	
	if ( (nAngle >= 0) && (nAngle < 4) && (templateAngles.size() > nAngle) )
		templateAngles[nAngle] += nAngleWeight;

	SetChanged();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUserProfile::GetUsedTemplates( const std::string &szTemplate )
{
	CTemplateUsageMap::const_iterator pos = templates.find( szTemplate );
	return pos != templates.end() ? pos->second.nCount : 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUserProfile::GetUsedTemplateGraphs( const std::string &szTemplate, const std::string &szGraph )
{
	CTemplateUsageMap::const_iterator posTemplate = templates.find( szTemplate );
	if ( posTemplate != templates.end() )
	{
		std::unordered_map<std::string, int>::const_iterator posGraph = posTemplate->second.graphs.find( szGraph );
		if ( posGraph != posTemplate->second.graphs.end() )
			return posGraph->second;
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUserProfile::GetUsedAngles( const int nAngle ) 
{
	templateAngles.resize( 4, 0 );
	return (nAngle < 0) || (nAngle > 3) ? -1 : templateAngles[nAngle];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** user relations
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUserProfile::SetChatRelation( const wchar_t *pwszNick, const EPlayerRelation nRelation )
{
	CChatRelations::iterator pos = chatRelations.find( pwszNick );
	if ( (pos == chatRelations.end()) || (pos->second != nRelation) ) 
	{
		chatRelations[pwszNick] = nRelation;
		SetChanged();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const EPlayerRelation CUserProfile::GetChatRelation( const wchar_t *pwszNick )
{
	CChatRelations::iterator pos = chatRelations.find( pwszNick );
	return pos == chatRelations.end() ? EPR_NORMAL : pos->second;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** MOD support
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUserProfile::SetMOD( const std::string &_szMOD )
{
	if ( szMOD != _szMOD )
	{
		szMOD = _szMOD;
		SetChanged();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const std::string& CUserProfile::GetMOD() const
{
	return szMOD;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** loads counters, based on GUID for each mission
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUserProfile::RegisterLoad( const GUID &guid )
{
	loadCounters[guid] += 1;
	SetChanged();
}
int CUserProfile::GetLoadCounter( const GUID &guid ) const
{
	CLoadsMap::const_iterator pos = loadCounters.find( guid );
	return pos == loadCounters.end() ? 0 : pos->second;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** serialization & repairing
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUserProfile::IsChanged() const 
{ 
	return GetSingleton<IOptionSystem>()->IsChanged() || GetSingleton<IInput>()->IsChanged() || bChanged; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUserProfile::SerializeConfig( IDataTree *pSS )
{
	// serialize external singletons
	GetSingleton<IOptionSystem>()->SerializeConfig( pSS );
	GetSingleton<IInput>()->SerializeConfig( pSS );
	// serialize internal data
	CTreeAccessor saver = pSS;
	saver.Add( "HelpScreens", &helpscreens );
	saver.Add( "CutScenes", &cutscenes );
	saver.Add( "TemplatesUsage", &templates );
	saver.Add( "AnglesUsage", &templateAngles );
	saver.Add( "ChatRelations", &chatRelations );
	saver.Add( "MOD", &szMOD );
	saver.Add( "Variables", &variables );
	if ( saver.IsReading() ) 
	{
		templateAngles.resize( 4, 0 );
		std::vector<SLoadCounter> loads;
		saver.Add( "LoadCounters", &loads );
		loadCounters.clear();
		for ( std::vector<SLoadCounter>::const_iterator it = loads.begin(); it != loads.end(); ++it )
			loadCounters.insert( CLoadsMap::value_type(it->guid, it->nCounter) );
	}
	else
	{
		std::vector<SLoadCounter> loads;
		loads.reserve( loadCounters.size() );
		for ( CLoadsMap::const_iterator it = loadCounters.begin(); it != loadCounters.end(); ++it )
			loads.push_back( SLoadCounter(it->first, it->second) );
		saver.Add( "LoadCounters", &loads );
	}
	//
	bChanged = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUserProfile::Repair( IDataTree *pSS, const bool bToDefault )
{
	// repair external singletons
	GetSingleton<IOptionSystem>()->Repair( pSS, bToDefault );
	GetSingleton<IInput>()->Repair( pSS, bToDefault );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
