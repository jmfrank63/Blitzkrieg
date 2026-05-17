#ifndef __OPTIONENTRYWRAPPER_H__
#define __OPTIONENTRYWRAPPER_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
#include "UIOptions.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI representation of options
class CUIOption : public IUISetOptionsToUI, public IUIGetOptionsFromUI, public IRefCount
{
	OBJECT_COMPLETE_METHODS( CUIOption );
	//
	CPtr<IUIDialog> pDialog;
	CPtr<IOption> pOption;
	CPtr<IUIStatic> pOptionName;
	CPtr<IUIDialog> pSubDialog;
	
	//for selection
	std::vector<SOptionDropListValue> szSelections;
	int nCurSelection;
	int nStartingSelection;

	//initial parametres ( for canceling options changes )
	int nInitialSliderPos;							// for remember slider position
	int nInitialSelection;							// for remember initial selection
	std::wstring szInitialText;					// initial text entry value
	int nInitialNumericEntry;						// initial numeric entry value

	void SetSlider( const int nInitialSliderPos );
		// store current value to option.
	void SaveOption();
	void ChangeSelection( const int nCurSelection );
public:
	CUIOption() {  }
	// init textes and dialog according to current option value and type
	CUIOption( IUIStatic *_pOptionName, IUIDialog *_pDialog, IOption *_pOption );

	//saves options an set current values as initial;
	void Apply();
	
	//restores option as it was during creation.
	void CancelChanges();

	void OnClicked( const bool bLeft );
	void OnSelected();
	void PositionChanged();
	bool IsOptionValid() const { return pOption.IsValid(); }

	//interface IUISetOptionsToUI 
	//usese tempBuffer
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

	//interface IUIGetOptionsFromUI
	virtual int STDCALL GetSelectionOption() const;
	virtual int STDCALL GetSliderOption() const ;
	virtual const wchar_t * STDCALL GetTextOption () const ;
	virtual const wchar_t * STDCALL GetTextGameSpyOption() const ;
	virtual const int STDCALL GetTextNumericOption() const;
};
typedef std::list<SOptionDesc> OptionDescs;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

	void InitList( const bool bDefault );
public:
	COptionsListWrapper() {  }
	COptionsListWrapper( const DWORD _dwFlags, IUIListControl * _pList, const int _nIDToStartFrom, IOptionSystem * pSystem = 0, const bool bDisableChange = false );
	COptionsListWrapper( IUIListControl * _pList, OptionDescs & optionDescs, const int _nIDToStartFrom, IOptionSystem * pSystem = 0, const bool bDisableChange = false );
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );

	void Apply();
	void ToDefault();
	void CancelChanges();
	void ShowWindow( int nCmdShow ) { pList->ShowWindow( nCmdShow ); }
	void DisableChange();
	bool IsChangesEnabled() const { return bDisableChange; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __OPTIONENTRYWRAPPER_H__
