#ifndef __INTERFACESWITCHMODETO_H__
#define __INTERFACESWITCHMODETO_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
#include "..\Misc\FileUtils.h"
class CInterfaceSwitchModeTo : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfaceSwitchModeTo );
public:
	typedef std::pair<std::string,std::string> CModName;
	typedef std::vector< CModName > CModNames;
	struct SEnumDirs
	{
		CModNames *pModDirs;
		void operator()( const class NFile::CFileIterator &fileIt ) const;
	};
private:
	NInput::CCommandRegistrator commandMsgs;
	bool bModExists;											// mode exists and switch is possible
	std::string szDirName;						// desired mod name

	int nCommandOnOk;
	std::string szCommandParams;
	bool bSilentSwitch;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceSwitchModeTo();
	CInterfaceSwitchModeTo() : CInterfaceInterMission( "InterMission" ) {  }
public:
	virtual bool STDCALL Init();
	bool Create(  const std::string &szModName,
								const std::string &szModVer,
								const int nCommandID,
								const std::string &szComandParams,
								const bool bSilentSwitch );
	void OnOk();

};
class CICSwitchModeTo : public CInterfaceCommandBase<CInterfaceSwitchModeTo, MISSION_INTERFACE_SWITCH_MODE_TO>
{
	OBJECT_NORMAL_METHODS( CICSwitchModeTo );
	std::string szNewModName;						// new mode to switch to
	std::string szNewModVersion;						// new mode to switch to

	int nCommandOnOk;
	std::string szCommandParams;
	bool bSilentSwitch;

	virtual void PreCreate( IMainLoop *pML ) 
	{
	}
	virtual void PostCreate( IMainLoop *pML, CInterfaceSwitchModeTo *pI ) 
	{ 
		const bool bOk = pI->Create( szNewModName, szNewModVersion, nCommandOnOk, szCommandParams, bSilentSwitch );
		pML->PushInterface( pI ); 
		if ( bOk )
			pI->OnOk();
	}
	CICSwitchModeTo() {}
public:
	virtual void STDCALL Configure( const char *pszConfig )
	{
		if ( pszConfig )
		{
			std::vector<std::string> szStrings;
			NStr::SplitString( pszConfig, szStrings, ';' );
			NI_ASSERT_T( szStrings.size() == 5, "Invalid number of parameters for encyclopedia interface" );
			szNewModName = szStrings[0];
			szNewModVersion = szStrings[1];
			nCommandOnOk = NStr::ToInt( szStrings[2] );
			szCommandParams = szStrings[3];
			bSilentSwitch = NStr::ToInt( szStrings[4] );
		}
	}
};
#endif // __INTERFACESWITCHMODETO_H__
