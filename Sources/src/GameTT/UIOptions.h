#ifndef __UIOPTIONS_H__
#define __UIOPTIONS_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
#include "..\StreamIO\OptionSystem.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ui side
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IUISetOptionsToUI 
{
	//usese tempBuffer
	virtual void STDCALL SetSelectionOption( const std::vector<SOptionDropListValue> &szSelections, const int nDefault ) = 0;
	virtual void STDCALL SetSliderOption( const int nMin, const int nMax, const int nDefault ) = 0;
	virtual void STDCALL SetTextOption( const wchar_t *pszEntry ) = 0;
	virtual void STDCALL SetTextGameSpyOption( const wchar_t *pszEntry ) = 0;
	virtual void STDCALL SetTextNumericOption( const int nEnntry ) = 0;

	virtual void STDCALL ResetSelection() = 0;
	virtual void STDCALL ResetSlider() = 0;
	virtual void STDCALL ResetTextEntry() = 0;
	virtual void STDCALL ResetNumericEntry() = 0;
	virtual void STDCALL ResetTextGameSpyEntry() = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IUIGetOptionsFromUI
{
	virtual int STDCALL GetSelectionOption()  const = 0;
	virtual int STDCALL GetSliderOption() const = 0;
	virtual const wchar_t * STDCALL GetTextOption () const = 0;
	virtual const int STDCALL GetTextNumericOption() const = 0;
	virtual const wchar_t * STDCALL GetTextGameSpyOption() const = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EOptionsType
{
	EOT_SELECTION						= 0,
	EOT_SLIDER							= 1,
	EOT_TEXTENTRY						= 2,
	EOT_NUMERICENTRY				= 3,
	EOT_GAMESPY_TEXTENTRY		= 4,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// other side
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// for manipulate with options. options container returns array of this interfaces.
interface IOption : public IRefCount
{
	virtual void STDCALL Set( interface IUISetOptionsToUI *pSet ) = 0;
	virtual void STDCALL Get( interface IUIGetOptionsFromUI *pGet ) = 0;
	virtual EOptionsType STDCALL GetType() const = 0;
	virtual const char * STDCALL GetName() const = 0;
	virtual void STDCALL Apply()  = 0;
	virtual void STDCALL CancelChanges( interface IUISetOptionsToUI *pSet ) = 0;
	virtual bool STDCALL IsInstant() const = 0;
	virtual void STDCALL SetOptionSystem( IOptionSystem * pSystem ) = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// for getting options.
interface IOptionContainer
{
	//usese tempBuffer
	virtual void GetOptions( interface IOption **ppOptions, int *pnCount ) = 0;
	virtual void Apply() = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class COption : public IOption
{
	std::string szName;
	bool bInstant;
	CPtr<IOptionSystem> pOptionSystem;
protected:
	IOptionSystem * GetOptionSystem()
	{
		if ( pOptionSystem.IsValid() ) 
			return pOptionSystem;
		return GetSingleton<IOptionSystem>();
	}
	COption() {  }
	COption( const char *pszName, const bool _bInstant ) : szName( pszName ), bInstant( _bInstant ) {  }
public:
	virtual void STDCALL SetOptionSystem( IOptionSystem * pSystem ) { pOptionSystem = pSystem; }
	virtual const char * STDCALL GetName() const { return szName.c_str(); }
	virtual bool STDCALL IsInstant() const { return bInstant; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
class COptionSelection : public COption
{
	OBJECT_COMPLETE_METHODS( COptionSelection );
	std::vector<SOptionDropListValue> selections;			// list of options
	int nSelection;															// current selection
public:
	COptionSelection() :  nSelection( 0 ) {  }
	COptionSelection( const char *pszName, const bool _bInstant, const std::vector<SOptionDropListValue> &_selections, const char *_pszDefault )
		: COption( pszName, _bInstant ), selections( _selections ) 
	{  
		const std::string szDefault = _pszDefault;
		nSelection = selections.size() - 1;
		for ( ; nSelection > 0; --nSelection  )
			if ( selections[nSelection].szProgName == szDefault )
				break;
		
	}

	virtual void STDCALL Set( interface IUISetOptionsToUI *pSet )
		{	pSet->SetSelectionOption( selections, nSelection ); }
	virtual void STDCALL Get( interface IUIGetOptionsFromUI *pGet )
		{ nSelection = pGet->GetSelectionOption(); }
	virtual EOptionsType STDCALL GetType() const { return EOT_SELECTION; }
	virtual void STDCALL CancelChanges( interface IUISetOptionsToUI *pSet )
		{ pSet->ResetSelection(); }
	virtual void STDCALL Apply()
	{
		GetOptionSystem()->Set( GetName(), selections[nSelection].szProgName.c_str() );
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class COptionSlider : public COption
{
	OBJECT_COMPLETE_METHODS( COptionSlider );
	int nMin, nMax, nCur;
public:
	COptionSlider() {  }
	COptionSlider( const char *pszName, const bool _bInstant, const int _nMin, const int _nMax, const int _nCur )
		: COption( pszName, _bInstant ), nMin( _nMin ), nMax( _nMax ), nCur( _nCur )  {  }
	virtual void STDCALL Set( interface IUISetOptionsToUI *pSet )
		{ pSet->SetSliderOption( nMin, nMax, nCur ); }
	virtual void STDCALL Get( interface IUIGetOptionsFromUI *pGet )
		{ nCur = pGet->GetSliderOption(); }
	virtual EOptionsType STDCALL GetType() const { return EOT_SLIDER; }
	virtual void STDCALL CancelChanges( interface IUISetOptionsToUI *pSet )
		{ pSet->ResetSlider(); }
	virtual void STDCALL Apply() 
	{
		GetOptionSystem()->Set( GetName(), variant_t(long(nCur)) );
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// local player name
class COptionTextEntry : public COption
{
	OBJECT_COMPLETE_METHODS( COptionTextEntry );
	std::wstring szText;
public:
	COptionTextEntry() {  }
	COptionTextEntry( const char *pszName, const bool _bInstant, const wchar_t *_pszText)	: COption( pszName, _bInstant ), szText( _pszText ) {  }
	virtual void STDCALL Set( interface IUISetOptionsToUI *pSet )
		{ pSet->SetTextOption( szText.c_str() ); }
	virtual void STDCALL Get( interface IUIGetOptionsFromUI *pGet )
		{ szText = pGet->GetTextOption(); }
	virtual EOptionsType STDCALL GetType() const { return EOT_TEXTENTRY; }
	virtual void STDCALL CancelChanges( interface IUISetOptionsToUI *pSet )
		{ pSet->ResetTextEntry(); }
	virtual void STDCALL Apply() 
	{
		GetOptionSystem()->Set( GetName(), szText.c_str() );
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class COptionTextEntryGameSpyCharacters : public COption
{
	OBJECT_COMPLETE_METHODS( COptionTextEntryGameSpyCharacters );
	std::wstring szText;
public:
	COptionTextEntryGameSpyCharacters() {  }
	COptionTextEntryGameSpyCharacters( const char *pszName, const bool _bInstant, const wchar_t *_pszText )	: COption( pszName, _bInstant ), szText( _pszText ) {  }
	virtual void STDCALL Set( interface IUISetOptionsToUI *pSet )
		{ pSet->SetTextGameSpyOption( szText.c_str() ); }
	virtual void STDCALL Get( interface IUIGetOptionsFromUI *pGet )
		{ szText = pGet->GetTextGameSpyOption(); }
	virtual EOptionsType STDCALL GetType() const { return EOT_GAMESPY_TEXTENTRY; }
	virtual void STDCALL CancelChanges( interface IUISetOptionsToUI *pSet )
		{ pSet->ResetTextGameSpyEntry(); }
	virtual void STDCALL Apply() 
	{
		GetOptionSystem()->Set( GetName(), szText.c_str() );
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class COptionNumericEntry : public COption
{
	OBJECT_COMPLETE_METHODS( COptionNumericEntry );
	int nEntry;
public:
	COptionNumericEntry() {  }
	COptionNumericEntry( const char *pszName, const bool _bInstant, const int _nEntry )	: COption( pszName, _bInstant), nEntry( _nEntry ) {  }
	virtual void STDCALL Set( interface IUISetOptionsToUI *pSet )
		{ pSet->SetTextNumericOption( nEntry ); }
	virtual void STDCALL Get( interface IUIGetOptionsFromUI *pGet )
		{ nEntry = pGet->GetTextNumericOption(); }
	virtual EOptionsType STDCALL GetType() const { return EOT_NUMERICENTRY; }
	virtual void STDCALL CancelChanges( interface IUISetOptionsToUI *pSet )
		{ pSet->ResetNumericEntry(); }
			
	virtual void STDCALL Apply() 
	{
		GetOptionSystem()->Set( GetName(), variant_t(long(nEntry)) );
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __UIOPTIONS_H__
