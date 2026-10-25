#ifndef __OPTIONENTRYWRAPPER_H__
#define __OPTIONENTRYWRAPPER_H__
#pragma ONCE
#include <map>
#include "InterMission.h"
#include "iMission.h"
#include "UIOptions.h"
class CUIOption : public IUISetOptionsToUI, public IUIGetOptionsFromUI, public IRefCount
{
	OBJECT_COMPLETE_METHODS( CUIOption );
	CPtr<IUIDialog> pDialog;
	CPtr<IOption> pOption;
	CPtr<IUIStatic> pOptionName;
	CPtr<IUIDialog> pSubDialog;
	
	std::vector<SOptionDropListValue> szSelections;
	int nCurSelection;
	int nStartingSelection;

	int nInitialSliderPos;							// for remember slider position
	int nInitialSelection;							// for remember initial selection
	std::wstring szInitialText;					// initial text entry value
	int nInitialNumericEntry;						// initial numeric entry value

	void SetSlider( const int nInitialSliderPos );
	void SetNameTexts();
	void SaveOption();
	void ChangeSelection( const int nCurSelection );
public:
	CUIOption() {  }
	CUIOption( IUIStatic *_pOptionName, IUIDialog *_pDialog, IOption *_pOption );

	void Apply();

	void CancelChanges();

	void OnClicked( const bool bLeft );
	void OnSelected();
	void PositionChanged();
	bool ChangeByKey( const bool bNext );
	bool IsOptionValid() const { return pOption.IsValid(); }
	// Sets the row's name, tooltip and shown choice from the text manager
	// again: they are copies, which a language switch leaves stale.
	void RefreshTexts();

	virtual void STDCALL SetSelectionOption( const std::vector<SOptionDropListValue> &szSelections, const int nDefault );
	virtual void STDCALL SetSliderOption( const int nMin, const int nMax, const int nDefault );
	virtual void STDCALL SetTextOption( const wchar_t *pszEntry );
	virtual void STDCALL SetTextNumericOption( const int nEntry );
	virtual void STDCALL SetTextGameSpyOption( const wchar_t *pszEntry );

	virtual void STDCALL ResetSelection();
	virtual void STDCALL ResetSlider();
	virtual void STDCALL ResetTextEntry();
	virtual void STDCALL ResetTextGameSpyEntry();
	virtual void STDCALL ResetNumericEntry();

	virtual int STDCALL GetSelectionOption() const;
	virtual int STDCALL GetSliderOption() const ;
	virtual const wchar_t * STDCALL GetTextOption () const ;
	virtual const wchar_t * STDCALL GetTextGameSpyOption() const ;
	virtual const int STDCALL GetTextNumericOption() const;
private:
	// Backing store for the two accessors that return a raw const wchar_t*.
	// The text arrives from the UI as UTF-16 and has to be widened, so the
	// result needs an owner that outlives the call.
	mutable std::wstring szWideTextCache;
	mutable std::wstring szWideGameSpyCache;
};
typedef std::list<SOptionDesc> OptionDescs;
// Values a screen supplies for a droplist row in place of the option
// system's fill - for rows whose list the option bridge cannot build itself.
// The cloud Provider row is the case: its values come from the cloud facade,
// which the streamio library does not link. Keyed by option name.
typedef std::map< std::string, std::vector<SOptionDropListValue> > OptionDropOverrides;
class COptionsListWrapper : public IRefCount
{
	OBJECT_COMPLETE_METHODS( COptionsListWrapper );

	std::vector< CPtr<CUIOption> > options;
	OptionDescs optionsDescs;
	CPtr<IUIListControl> pList;
	int nIDToStartFrom;										// the ids of options windows will start from this value.
	DWORD dwFlags;
	
	CPtr<IOptionSystem> pSetOptionSystem;		// where to set options ( custom option system )
	bool bDisableChange;
	OptionDropOverrides dropOverrides;

	void InitList( const bool bDefault );
public:
	COptionsListWrapper() {  }
	COptionsListWrapper( const DWORD _dwFlags, IUIListControl * _pList, const int _nIDToStartFrom, IOptionSystem * pSystem = 0, const bool bDisableChange = false );
	COptionsListWrapper( IUIListControl * _pList, OptionDescs & optionDescs, const int _nIDToStartFrom, IOptionSystem * pSystem = 0, const bool bDisableChange = false );
	COptionsListWrapper( IUIListControl * _pList, OptionDescs & optionDescs, const int _nIDToStartFrom, const OptionDropOverrides &overrides, IOptionSystem * pSystem = 0, const bool bDisableChange = false );
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );

	bool ChangeSelectedOption( const bool bNext );

	void Apply();
	void ToDefault();
	void CancelChanges();
	void ShowWindow( int nCmdShow ) { pList->ShowWindow( nCmdShow ); }
	void DisableChange();
	void RefreshTexts();
	bool IsChangesEnabled() const { return bDisableChange; }
};
// The language option's choices: Default (the shipped texts), then every
// folder under mods\ that carries data\texts.pak, as ELK exports a language.
// Scanned on each call; the settings screen asks once per opening.
std::vector<SOptionDropListValue> ScanLanguages();
// A language's name for the settings row: the mod's MODName from its
// data\mod.xml, or the folder name without one. Empty for Default, which
// has a text of its own.
std::string LanguageDisplayName( const std::string &szFolder );
// Whether the mod's data\mod.xml marks it <MODType>language</MODType>: such a
// mod belongs to Options > Language and stays out of the gameplay mods list.
bool IsLanguageOnlyMod( const std::string &szFolder );
// mods\ under the game's folder, in whatever case it is spelled on disk (the
// installs make it "mods"; a case-sensitive file system has no "Mods"), with
// a trailing backslash; empty when there is none. BK_MODS_ROOT, when set, is
// the folder instead (StreamIO/ModsPaths.h).
std::string ModsFolder();
#endif // __OPTIONENTRYWRAPPER_H__
