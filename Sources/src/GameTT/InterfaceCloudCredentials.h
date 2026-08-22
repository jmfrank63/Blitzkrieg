#ifndef __INTERFACECLOUDCREDENTIALS_H__
#define __INTERFACECLOUDCREDENTIALS_H__
#pragma ONCE
#include "../Common/InterfaceScreenBase.h"
#include "../Input/InputHelper.h"
#include "iMission.h"
// The cloud credentials dialog (P07-M01), modeled on the player-profile
// dialog: an edit-box screen reached from the settings Cloud tab. Values go
// through NCloudSync, never the option system - the option store truncates
// long strings, and the secret must never pass through it at all.
class CInterfaceCloudCredentials : public CInterfaceScreenBase
{
	OBJECT_NORMAL_METHODS( CInterfaceCloudCredentials );
	NInput::CCommandRegistrator msgs;

	CPtr<IUIButton> pButtonOK;
	CPtr<IUIButton> pButtonCancel;
	bool bFinished;

	// "s3" or "webdav"; which rows are visible and how they are labelled.
	std::string szProtocol;
	// The secret's real value lives here; the edit box shows only mask
	// characters. Empty plus bStoredSecret means "keep what is stored".
	std::wstring szSecretReal;
	bool bStoredSecret;
	bool bSecretTouched;
	// The stored document is in the generic schema this legacy dialog cannot
	// represent; saving from these fields would overwrite the real
	// configuration. Interim guard until the generic form replaces the
	// dialog (P02-M03).
	bool bGenericStored;
	// Poll handle of the running connection test, -1 when idle.
	int nTestHandle;

	void ApplyProtocol();
	void PopulateFromCredentials();
	void RefreshDiscoveryLine();
	void SetRow( int nRow, const char *pszLabelKey, const std::wstring &szValue, bool bVisible );
	std::wstring GetEdit( int nID );
	void SetEdit( int nID, const std::wstring &szText );
	void SetStatus( const char *pszTextKey, const std::wstring &szSuffix );
	void OnSecretEdited();
	bool SaveCredentials();
	void BeginConnectionTest();

	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual bool STDCALL StepLocal( bool bAppActive );
	virtual ~CInterfaceCloudCredentials() {}

protected:
	CInterfaceCloudCredentials() : CInterfaceScreenBase( "Current" ) {  }
public:
	virtual void STDCALL OnGetFocus( bool bFocus );
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
	virtual void STDCALL Done();
};
class CICCloudCredentials : public CInterfaceCommandBase<IInterfaceBase, MISSION_INTERFACE_CLOUD_CREDENTIALS>
{
	OBJECT_NORMAL_METHODS( CICCloudCredentials );

	virtual void PostCreate( IMainLoop *pML, IInterfaceBase *pInterface ) { pML->PushInterface( pInterface ); }
	CICCloudCredentials() {  }
};
#endif // __INTERFACECLOUDCREDENTIALS_H__
