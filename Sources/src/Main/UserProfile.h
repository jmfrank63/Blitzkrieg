#ifndef __USERPROFILE_H__
#define __USERPROFILE_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "ScenarioTracker.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct STemplateUsage
{
	int nCount;
	std::unordered_map<std::string, int> graphs;
	//
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "Count", &nCount );
		saver.Add( "Graphs", &graphs );
		return 0;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGUIDHashFunc
{
	unsigned long operator()( const GUID &guid ) const
	{
		const unsigned long uCode = guid.Data1 + guid.Data2 + guid.Data3 + 
																guid.Data4[0] + guid.Data4[1] + guid.Data4[2] + guid.Data4[3] + 
																guid.Data4[4] + guid.Data4[5] + guid.Data4[6] + guid.Data4[7];
		return uCode;
	}
};
struct SGUIDEqual
{
	bool operator()( const GUID &g1, const GUID &g2 ) const
	{
		return memcmp( &g1, &g2, sizeof(g1) ) == 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SLoadCounter
{
	GUID guid;														// mission GUID
	int nCounter;													// loads counter for this mission
	//
	SLoadCounter() 
		: nCounter( 0 ) {  }
	SLoadCounter( const GUID &_guid, const int _nCounter )
		: guid( _guid ), nCounter( _nCounter ) {  }
	//
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.AddRawData( "GUID", &guid, sizeof(guid) );
		saver.Add( "Counter", &nCounter );
		return 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CUserProfile : public CTRefCount<IUserProfile>
{
	OBJECT_SERVICE_METHODS( CUserProfile );

	// user variables
	typedef std::unordered_map<std::string, int> CVariables;
	CVariables variables;
	// help screens tracking
	//std::list<int> helpscreens;					// called help screens
	typedef std::unordered_map<int/*interface ID*/,WORD/*help mask*/> CHelpscenes;
	CHelpscenes helpscreens;
	// cutscenes
	std::vector<std::string> cutscenes;		// available cutscenes
	// templates usage statistics
	typedef std::unordered_map<std::string, STemplateUsage> CTemplateUsageMap;
	CTemplateUsageMap templates;					// templates usage statistics
	std::vector<int> templateAngles;			// angles usage statistics
	// chat relations
	typedef std::unordered_map<std::wstring, enum EPlayerRelation> CChatRelations;
	CChatRelations chatRelations;
	// MOD support
	std::string szMOD;										// MOD name
	// load count
	typedef std::unordered_map<GUID, int, SGUIDHashFunc, SGUIDEqual> CLoadsMap;
	CLoadsMap loadCounters;
	//
	bool bChanged;												// was user profile changed?
	//
	void SetChanged() { bChanged = true; }
public:	
	CUserProfile() : bChanged( false ) {  templateAngles.resize( 4, 0 ); }
	// 
	// help screens tracker part
	//
	bool STDCALL IsHelpCalled( const int nInterfaceTypeID, const int nHelpNumber ) const;
	void STDCALL HelpCalled( const int nInterfaceTypeID, const int nHelpNumber );
	//
	// cutscenes availability
	//
	void STDCALL AddCutScene( const std::string &szCutSceneName );
	int STDCALL GetNumCutScenes() const;
	const std::string& STDCALL GetCutScene( const int nIndex ) const;
	//
	// templates usage statistics
	//
	void STDCALL AddUsedTemplate( const std::string &rszTemplate, int nTemplateWeight, const std::string &rszGraph, int nGraphWeight, int nAngle, int nAngleWeight );
	int STDCALL GetUsedTemplates( const std::string &rszTemplate );
	int STDCALL GetUsedTemplateGraphs( const std::string &rszTemplate, const std::string &rszGraph );
	//0...3
	int STDCALL GetUsedAngles( const int nAngle );
	//
	// user relationships - for chat
	//
	void STDCALL SetChatRelation( const wchar_t *pwszNick, const enum EPlayerRelation nRelation );
	const enum EPlayerRelation STDCALL GetChatRelation( const wchar_t *pwszNick );
	//
	// current MOD
	//
	void STDCALL SetMOD( const std::string &_szMOD );
	const std::string& STDCALL GetMOD() const;
	//
	// loads counters, based on GUID for each mission
	//
	void STDCALL RegisterLoad( const GUID &guid );
	int STDCALL GetLoadCounter( const GUID &guid ) const;

	//
	// user values (stored in profile)
	//
	virtual void STDCALL AddVar( const char *pszValueName, const int nValue );
	virtual int STDCALL GetVar( const char *pszValueName, const int nDefValue ) const;
	virtual void STDCALL RemoveVar( const char *pszValueName );
	//
	// serialization & repairing
	//
	bool STDCALL IsChanged() const;
	void STDCALL SerializeConfig( IDataTree *pSS );
	void STDCALL Repair( IDataTree *pSS, const bool bToDefault );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __USERPROFILE_H__
