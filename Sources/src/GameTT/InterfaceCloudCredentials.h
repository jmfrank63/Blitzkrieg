#ifndef __INTERFACECLOUDCREDENTIALS_H__
#define __INTERFACECLOUDCREDENTIALS_H__
#pragma ONCE
#include "../Common/InterfaceScreenBase.h"
#include "../Input/InputHelper.h"
#include "iMission.h"
// The cloud credentials dialog: an edit-box screen reached from the settings
// Cloud tab that renders whatever the catalogue's form model says — no
// per-backend field set anywhere. Seven label+edit row slots are a window
// over the form's field list (wheel or the arrow buttons scroll; nothing
// truncates), a cycle button beside a row steps through the catalogue's
// examples, and the backend chooser walks the filtered destination list.
// Values go through NCloudSync, never the option system - the option store
// truncates long strings, and secrets must never pass through it at all.
class CInterfaceCloudCredentials : public CInterfaceScreenBase
{
	OBJECT_NORMAL_METHODS( CInterfaceCloudCredentials );
	NInput::CCommandRegistrator msgs;

	CPtr<IUIButton> pButtonOK;
	CPtr<IUIButton> pButtonCancel;
	CPtr<IInputSlider> pWheelScroll;		// own view of the mouse wheel for row scrolling
	bool bFinished;

	// One rendered field: the model's data plus the dialog-held value state.
	// The value lives here, not in the edit boxes - the boxes are a window
	// over this list, rebound on every scroll.
	struct SField
	{
		std::string szName;					// Option.Name; empty for the two special rows
		int nRole;									// 0 option, 1 remote root, 2 rclone override
		std::wstring szLabel;
		std::wstring szHelp;
		std::string szWidget;				// text | masked | droplist_closed | droplist_editable
		bool bRequired;
		bool bAdvanced;
		bool bIsPassword;
		std::string szPlaceholder;	// the catalogue default; never persisted as a value
		std::vector<std::string> exampleValues;
		std::vector<std::wstring> exampleHelp;
		std::wstring szValue;				// the real value, masked fields included
		bool bStoredSecret;					// a secret is stored; empty value means keep it
		bool bTouched;							// masked only: typed this session
		SField() : nRole( 0 ), bRequired( false ), bAdvanced( false ), bIsPassword( false ), bStoredSecret( false ), bTouched( false ) {}
		bool IsMasked() const { return szWidget == "masked"; }
	};
	std::vector<SField> fields;			// model order: basic, then advanced, then rclone
	std::vector<int> visibleRows;		// indexes into fields under the advanced filter
	int nScroll;
	bool bShowAdvanced;

	std::string szBackend;					// the chosen backend
	std::vector<std::string> destinations;
	bool bCatalogueReady;

	// What the stored credentials document said, parsed once on open. The
	// prefill source when the chosen backend is the stored one.
	std::string szStoredBackend;
	std::string szStoredRoot;
	std::string szStoredRclone;
	std::vector<std::pair<std::string, std::string> > storedOptions;
	std::vector<std::string> storedSecretNames;
	bool bLoadFailed;								// present but unreadable: refuse to overwrite

	// Poll handles: the connection test and the catalogue fetch, -1 when idle.
	int nTestHandle;
	int nCatalogueHandle;

	void LoadStored();
	void BeginCatalogue();
	void OnCatalogueReady();
	void ShowCatalogueMissing( const std::wstring &szReason );
	void RebuildForm( bool bPreserveTyped );
	void LayoutRows();
	void RefreshDiscoveryLine();
	void CycleExample( int nSlot );
	void OnRowEdited( int nSlot );
	SField *FieldAtSlot( int nSlot );
	SField *FieldNamed( const char *pszName );
	std::wstring GetEdit( int nID );
	void SetEdit( int nID, const std::wstring &szText );
	void SetStatus( const char *pszTextKey, const std::wstring &szSuffix );
	void OnSecretEdited( SField *pField, int nEditID );
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
