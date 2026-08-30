#ifndef __INTERFACEOPTIONSSETTINGS_H__
#define __INTERFACEOPTIONSSETTINGS_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
#include "../StreamIO/OptionSystem.h"
#include "OptionEntryWrapper.h"
class CInterfaceOptionsSettings : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfaceOptionsSettings );

	std::vector< CPtr<COptionsListWrapper> > optionsLists;

	int nActive;													// nurrent active division
	int nMaxDivision;											// total number of divisions.
	int nMinDifficulty;
	int nActiveNavButton;									// button the last Tab press parked the cursor on, -1 when none
	int nCloudDivision;										// tab index of the "Cloud" division, -1 when absent

	CPtr<IInputSlider> pWheelScroll;			// own view of the mouse wheel for the whole-screen list scroll

	OptionDescs cloudDescs;								// every Cloud.* descriptor, kept to rebuild the tab's list
	std::string szCloudProvider;						// Cloud.Provider as last built; a change rebuilds the list
	std::vector<std::string> cloudDestinations;	// the catalogue's destination list, empty until fetched
	int nCatalogueHandle;									// the catalogue fetch job, -1 when idle
	static bool IsCloudProviderOff( const std::string &szValue );
	std::string ReadCloudProvider() const;
	void BuildCloudList();
	void BeginCloudCatalogue();
	void LoadCloudDestinations();
	void RefreshCloudButtons();

	NInput::CCommandRegistrator commandMsgs;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual bool STDCALL StepLocal( bool bAppActive );

	virtual ~CInterfaceOptionsSettings() {  }
	CInterfaceOptionsSettings() : CInterfaceInterMission( /*"InterMission"*/"Current" ), nActive( -1 ), nMaxDivision( 0 ), nActiveNavButton( -1 ), nCloudDivision( -1 ), nCatalogueHandle( -1 ) {  }

	virtual void SuspendAILogic( bool bSuspend );
	void OnChangeDivision( const int nDivision );
	void Close();
	virtual bool OpenCurtains();
	void CycleNavButton();
	int GetArmedNavButton();
public:
	virtual void STDCALL Done();
	virtual bool STDCALL Init();
	void Create();

};
class CICOptionsSettings: public CInterfaceCommandBase<CInterfaceOptionsSettings, MISSION_INTERFACE_OPTIONSSETTINGS>
{
	OBJECT_NORMAL_METHODS( CICOptionsSettings );
	
	virtual void PreCreate( IMainLoop *pML ) { }
	virtual void PostCreate( IMainLoop *pML, CInterfaceOptionsSettings *pEI )
	{
		pEI->Create();
		pML->PushInterface( pEI );
	}
	CICOptionsSettings() {  }
public:
	virtual void STDCALL Configure( const char *pszConfig ) 
	{  
	}
};
#endif // __INTERFACEOPTIONSSETTINGS_H__
