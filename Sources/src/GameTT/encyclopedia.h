#ifndef __IM_ENCYCLOPEDIA_H__
#define __IM_ENCYCLOPEDIA_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
struct SMedalStats;
struct SUnitBaseRPGStats;
class CInterfaceEncyclopedia : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfaceEncyclopedia );
	NInput::CCommandRegistrator commandMsgs;
	bool bFinished;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceEncyclopedia();
	CInterfaceEncyclopedia() : CInterfaceInterMission( "InterMission" ), bFinished( false ) {}

	void LoadMedalInfo( const SMedalStats *pMedalStats, std::string *pszTextureFileName, std::wstring *pszTitle, std::wstring *pDesc );
	void LoadUnitInfo( const SUnitBaseRPGStats *pUnitStats, std::string *pszTextureFileName, std::wstring *pszTitle, std::wstring *pDesc, std::wstring *pStatistics );
public:
	virtual bool STDCALL Init();
	virtual void STDCALL Done();
	void Create( int nType, const char *pszName );
};
class CICEncyclopedia : public CInterfaceCommandBase<CInterfaceEncyclopedia, MISSION_INTERFACE_ENCYCLOPEDIA>
{
	OBJECT_NORMAL_METHODS( CICEncyclopedia );
	
	int nType;
	std::string szName;

	virtual void PreCreate( IMainLoop *pML ) {}
	virtual void PostCreate( IMainLoop *pML, CInterfaceEncyclopedia *pEI );
	CICEncyclopedia() {  }
public:
	virtual void STDCALL Configure( const char *pszConfig );
};
#endif		//__IM_ENCYCLOPEDIA_H__
