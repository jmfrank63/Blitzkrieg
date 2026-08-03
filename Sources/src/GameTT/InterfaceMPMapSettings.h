#ifndef __INTERFACEMPMAPSETTINGS_H__
#define __INTERFACEMPMAPSETTINGS_H__
#pragma ONCE
#include "../Common/InterfaceScreenBase.h"
#include "../Input/InputHelper.h"
#include "iMission.h"
#include "MapSettingsWrapper.h"
class CInterfaceMPMapSettings : public CInterfaceScreenBase
{
	OBJECT_NORMAL_METHODS( CInterfaceMPMapSettings );
	NInput::CCommandRegistrator msgs;
	CPtr<IUIButton> pButtonOK;
	CPtr<IUIButton> pButtonCancel;
	bool bFinished;												// interface is closed
	bool bDisableChanges;
	
	CPtr<CMapSettingsWrapper> pMapSettingsWrapper;
	CPtr<COptionsListWrapper> pOptions;

	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceMPMapSettings() {}
	virtual bool STDCALL StepLocal( bool bAppActive );
protected:
	CInterfaceMPMapSettings() : CInterfaceScreenBase( "Current" ), bDisableChanges( false ) {  }
public:
	virtual bool STDCALL Init();
	void Create( const bool bDisableChanges, const bool bStagingRoom );
};
class CICMPMapSettings : public CInterfaceCommandBase<CInterfaceMPMapSettings, MISSION_INTERFACE_MP_MAP_SETTINGS>
{
	OBJECT_NORMAL_METHODS( CICMPMapSettings );

	bool bDisableChange;
	bool bStagingRoom;

	virtual void PostCreate( IMainLoop *pML, CInterfaceMPMapSettings *pInterface ) 
	{ 
		pInterface->Create( bDisableChange, bStagingRoom );
		pML->PushInterface( pInterface ); 
	}
	CICMPMapSettings() : bDisableChange( false ), bStagingRoom( false ) {  }

public:
	virtual void STDCALL Configure( const char *pszConfig )
	{
		if ( !pszConfig ) return;
		const std::string szConfig = pszConfig;
		std::vector<std::string> strings;
		NStr::SplitString( szConfig, strings, ';' );

		if ( strings.size() == 2 )
		{
			bDisableChange = NStr::ToInt( strings[0] );
			bStagingRoom = NStr::ToInt( strings[1] );
		}
	}
};

#endif // __INTERFACEMPMAPSETTINGS_H__
