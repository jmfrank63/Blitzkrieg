#ifndef __INTERFACESTARTDIALOG_H__
#define __INTERFACESTARTDIALOG_H__
#pragma ONCE
#include "../Common/InterfaceScreenBase.h"
#include "../Input/InputHelper.h"
#include "iMission.h"
class COptionsListWrapper;
class CInterfacePlayerProfile : public CInterfaceScreenBase
{
	OBJECT_NORMAL_METHODS( CInterfacePlayerProfile );
	NInput::CCommandRegistrator msgs;
	CPtr<IUIButton> pButtonOK;
	CPtr<IUIButton> pButtonCancel;
	CPtr<IUIEditBox> pEdit;
	bool bFinished;												// interface is closed

	CPtr<COptionsListWrapper> pOptions;
	bool bEnableCancel;

	CPtr<IUIListControl> pProfileList;
	std::vector<std::string> profileNames;// row user data indexes into this
	std::string szPendingDelete;					// profile awaiting the confirm box answer

	void SwitchToProfile( const std::string &szNewProfile, const std::wstring &szTypedName );
	void FillProfileList( const std::string &szSelect );
	void SetEditBoxText( const std::wstring &szText );
	void DeleteProfile( const std::string &szName );
	void ReportProfileError( const char *pszTextKey, const std::string &szDetail );

	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfacePlayerProfile() {}
	virtual bool STDCALL StepLocal( bool bAppActive );
	
protected:
	CInterfacePlayerProfile() : CInterfaceScreenBase( "Current" ) {  }
public:
	virtual void STDCALL OnGetFocus( bool bFocus );
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
};
class CICPlayerProfile : public CInterfaceCommandBase<IInterfaceBase, MISSION_INTERFACE_PLAYER_PROFILE>
{
	OBJECT_NORMAL_METHODS( CICPlayerProfile );

	virtual void PostCreate( IMainLoop *pML, IInterfaceBase *pInterface ) { pML->PushInterface( pInterface ); }
	CICPlayerProfile() {  }
};
#endif // __INTERFACESTARTDIALOG_H__
