#include "StdAfx.h"

#include "OptionEntryWrapper.h"
#include "..\UI\UIMessages.h"
#include "UIConsts.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS(COptionsListWrapper);
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EUIElements
{
	E_SUBDIALOG_SELECTIONS								=	3000,
	E_SELECTIONS_ENTRY										= 10003,

	E_SUBDIALOG_SLIDER										= 3001,
	E_SLIDER															= 10003,

	E_SUBDIALOG_TEXTEDIT									= 3002,
	E_EDITBOX															= 10003,
	
	E_SUBDIALOG_TEXTEDIT_NUMERIC					= 3003,
	E_EDITBOX_NUMERIC											= 10003,

	E_SUBDIALOG_TEXTEDIT_GAMESPY					= 3004,
	E_EDITBOX_GAMESPY											= 10003
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//		CUIOption
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CUIOption::CUIOption( IUIStatic *_pOptionName, IUIDialog *_pDialog, IOption *_pOption )
	: pDialog( _pDialog ), pOption( _pOption ), pOptionName( _pOptionName )
{
	ITextManager * pTM = GetSingleton<ITextManager>();
	std::string szKeyOption = "Textes\\Options\\";
	szKeyOption += pOption->GetName();
	const std::string szKeyName = szKeyOption + ".name";
	const std::string szKeyTooltip = szKeyOption + ".tooltip";
	IText * pText = pTM->GetString( szKeyName.c_str() );
	NI_ASSERT_T( pText != 0, NStr::Format("cannot find local name %s for options", szKeyName.c_str() ) );
	if ( pText )
		pOptionName->SetWindowText( 0, pText->GetString() );
	pText = pTM->GetString( szKeyTooltip.c_str() );
	if ( pText )
		pOptionName->SetHelpContext( 0, pText->GetString() );

	pOption->Set( this );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIOption::SaveOption()
{
	pOption->Get( this );
	pOption->Apply();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIOption::Apply()
{
	SaveOption();
	pOption->Get( this );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIOption::CancelChanges()
{
	pOption->CancelChanges( this );
	SaveOption();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIOption::PositionChanged()
{
	if ( EOT_SLIDER == pOption->GetType() && pOption->IsInstant() )
	{
		SaveOption();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIOption::OnSelected()
{
	if ( EOT_GAMESPY_TEXTENTRY == pOption->GetType() )
	{
		IUIEditBox * pStatic = checked_cast<IUIEditBox*>( pSubDialog->GetChildByID( E_EDITBOX_GAMESPY ) );
		pStatic->SetFocus( true );
	}
	else if ( EOT_TEXTENTRY == pOption->GetType() )
	{
		IUIEditBox * pStatic = checked_cast<IUIEditBox*>( pSubDialog->GetChildByID( E_EDITBOX ) );
		pStatic->SetFocus( true );
	}
	else if ( EOT_NUMERICENTRY == pOption->GetType() )
	{
		IUIEditBox * pStatic = checked_cast<IUIEditBox*>( pSubDialog->GetChildByID( E_EDITBOX_NUMERIC ) );
		pStatic->SetFocus( true );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIOption::ChangeSelection( const int nCurSelection )
{
	const std::string szKey = CUIConsts::ConstructOptionKey( pOption->GetName(), szSelections[nCurSelection].szProgName.c_str() );
	const std::string szKeyName = szKey + ".name";
	const std::string szKeyTooltip = szKey + ".tooltip";

	ITextManager * pTM = GetSingleton<ITextManager>();
	IUIStatic * pStatic = checked_cast<IUIStatic*>( pSubDialog->GetChildByID( E_SELECTIONS_ENTRY ) );

	IText * pText = pTM->GetString( szKeyName.c_str() );
	if ( pText )
		pStatic->SetWindowText( 0, pText->GetString() );
	else
	{
#ifndef _FINALRELEASE
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, NStr::Format( "cannot find localized string %s", szKeyName.c_str() ) );
#endif // _FINALRELEASE
		pStatic->SetWindowText( 0, NStr::ToUnicode( szSelections[nCurSelection].szProgName ).c_str() );
	}

	pText = pTM->GetString( szKeyTooltip.c_str() );
	if ( pText )
		pStatic->SetHelpContext( 0, pText->GetString() );
	else
	{
#ifndef _FINALRELEASE
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, NStr::Format( "cannot find localized string %s", szKeyTooltip.c_str() ) );
#endif // _FINALRELEASE
		IText * pHelpContext = pOptionName->GetHelpContext( VNULL2, 0 );
		if ( pHelpContext )
			pStatic->SetHelpContext( 0, pHelpContext->GetString() );
	}
}	
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIOption::OnClicked( const bool bLeft )
{
	if ( EOT_SELECTION == pOption->GetType() ) //set next entry
	{
		if ( bLeft )
			++nCurSelection;
		else 
		{
			nCurSelection = nCurSelection == 0 ? szSelections.size()-1 : nCurSelection - 1;
		}
		nCurSelection %= szSelections.size();
		ChangeSelection( nCurSelection );
		if ( pOption->IsInstant() )
			SaveOption();
	}
	else if ( EOT_TEXTENTRY == pOption->GetType() )
	{
		IUIEditBox * pStatic = checked_cast<IUIEditBox*>( pSubDialog->GetChildByID( E_EDITBOX ) );
		pStatic->SetFocus( true );
	}
	else if ( EOT_NUMERICENTRY == pOption->GetType() )
	{
		IUIEditBox * pStatic = checked_cast<IUIEditBox*>( pSubDialog->GetChildByID( E_EDITBOX_NUMERIC ) );
		pStatic->SetFocus( true );
	}
	else if ( EOT_GAMESPY_TEXTENTRY == pOption->GetType() )
	{
		IUIEditBox * pStatic = checked_cast<IUIEditBox*>( pSubDialog->GetChildByID( E_EDITBOX_GAMESPY ) );
		pStatic->SetFocus( true );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIOption::SetSelectionOption( const std::vector<SOptionDropListValue> &_szSelections, const int _nDefault )
{
	szSelections = _szSelections;
	nCurSelection = nInitialSelection = _nDefault;
	pSubDialog = checked_cast<IUIDialog*>( pDialog->GetChildByID( E_SUBDIALOG_SELECTIONS ) );

	ChangeSelection( nCurSelection );
	pSubDialog->ShowWindow( UI_SW_SHOW );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIOption::ResetSelection()
{
	//IUIStatic * pStatic = checked_cast<IUIStatic*>( pSubDialog->GetChildByID( E_SELECTIONS_ENTRY ) );
	//ITextManager * pTM = GetSingleton<ITextManager>();
	nCurSelection = nInitialSelection;
	ChangeSelection( nCurSelection );
	pSubDialog->ShowWindow( UI_SW_SHOW );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIOption::SetSliderOption( const int  _nMin, const int _nMax, const int _nInitial )
{
	pSubDialog = checked_cast<IUIDialog*>( pDialog->GetChildByID( E_SUBDIALOG_SLIDER ) );
	IUISlider * pSlider = checked_cast<IUISlider*>( pSubDialog->GetChildByID( E_SLIDER ) );
	pSlider->SetMaxValue( _nMax );
	pSlider->SetMinValue( _nMin );

	nInitialSliderPos = _nInitial;
	ResetSlider();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIOption::ResetSlider()
{
	IUISlider * pSlider = checked_cast<IUISlider*>( pSubDialog->GetChildByID( E_SLIDER ) );
	pSlider->SetPosition( nInitialSliderPos );
	pSubDialog->ShowWindow( UI_SW_SHOW );
	
	IText * pHelpContext = pOptionName->GetHelpContext( VNULL2, 0 );
	if ( pHelpContext )
		pSlider->SetHelpContext( 0, pHelpContext->GetString() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIOption::SetTextOption( const wchar_t *pszEntry )
{
	pSubDialog = checked_cast<IUIDialog*>( pDialog->GetChildByID( E_SUBDIALOG_TEXTEDIT ) );
	szInitialText = pszEntry;
	ResetTextEntry();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIOption::SetTextGameSpyOption( const wchar_t *pszEntry )
{
	pSubDialog = checked_cast<IUIDialog*>( pDialog->GetChildByID( E_SUBDIALOG_TEXTEDIT_GAMESPY ) );
	szInitialText = pszEntry;
	ResetTextGameSpyEntry();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIOption::SetTextNumericOption( const int nEntry )
{
	pSubDialog = checked_cast<IUIDialog*>( pDialog->GetChildByID( E_SUBDIALOG_TEXTEDIT_NUMERIC ) );
	nInitialNumericEntry = nEntry;
	ResetNumericEntry();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIOption::ResetTextEntry()
{
	IUIEditBox * pStatic = checked_cast<IUIEditBox*>( pSubDialog->GetChildByID( E_EDITBOX ) );
	pStatic->SetWindowText( 0, szInitialText.c_str() );
	pSubDialog->ShowWindow( UI_SW_SHOW );
	IText * pHelpContext = pOptionName->GetHelpContext( VNULL2, 0 );
	if ( pHelpContext )
		pStatic->SetHelpContext( 0, pHelpContext->GetString() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIOption::ResetTextGameSpyEntry()
{
	IUIEditBox * pStatic = checked_cast<IUIEditBox*>( pSubDialog->GetChildByID( E_EDITBOX_GAMESPY) );
	pStatic->SetWindowText( 0, szInitialText.c_str() );
	pSubDialog->ShowWindow( UI_SW_SHOW );
	
	IText * pHelpContext = pOptionName->GetHelpContext( VNULL2, 0 );
	if ( pHelpContext )
		pStatic->SetHelpContext( 0, pHelpContext->GetString() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIOption::ResetNumericEntry()
{
	IUIEditBox * pStatic = checked_cast<IUIEditBox*>( pSubDialog->GetChildByID( E_EDITBOX_NUMERIC ) );
	pStatic->SetWindowText( 0, NStr::ToUnicode( NStr::Format( "%d", nInitialNumericEntry ) ).c_str() );
	pSubDialog->ShowWindow( UI_SW_SHOW );
	IText * pHelpContext = pOptionName->GetHelpContext( VNULL2, 0 );
	if ( pHelpContext )
		pStatic->SetHelpContext( 0, pHelpContext->GetString() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIOption::GetSelectionOption() const
{
	return nCurSelection;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIOption::GetSliderOption() const
{
	IUISlider * pSlider = checked_cast<IUISlider*>( pSubDialog->GetChildByID( E_SLIDER ) );
	return pSlider->GetPosition();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CUIOption::GetTextNumericOption() const
{
	IUIEditBox * pStatic = checked_cast<IUIEditBox*>( pSubDialog->GetChildByID( E_EDITBOX_NUMERIC ) );
	std::string szEntry = NStr::ToAscii( pStatic->GetWindowText( 0 ) );
	return NStr::ToInt( szEntry );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const wchar_t * CUIOption::GetTextGameSpyOption() const
{
	IUIEditBox * pStatic = checked_cast<IUIEditBox*>( pSubDialog->GetChildByID( E_EDITBOX ) );
	return pStatic->GetWindowText( 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const wchar_t * CUIOption::GetTextOption () const
{
	IUIEditBox * pStatic = checked_cast<IUIEditBox*>( pSubDialog->GetChildByID( E_EDITBOX ) );
	return pStatic->GetWindowText( 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//		COptionsListWrapper
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
COptionsListWrapper::COptionsListWrapper( const DWORD _dwFlags, IUIListControl * _pList, const int _nIDToStartFrom, IOptionSystem * pSystem, const bool _bDisableChange )
	: pList( _pList ), nIDToStartFrom( _nIDToStartFrom ), dwFlags( _dwFlags ), bDisableChange( _bDisableChange ), pSetOptionSystem( pSystem )
{
	for ( CPtr<IOptionSystemIterator> pIter = pSetOptionSystem->CreateIterator( dwFlags ); !pIter->IsEnd(); pIter->Next() )
	{
		const SOptionDesc *pDesc = pIter->GetDesc();
		optionsDescs.push_back( *pDesc );
	}
	InitList( false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
COptionsListWrapper::COptionsListWrapper( IUIListControl * _pList, OptionDescs & _optionDescs, const int _nIDToStartFrom, IOptionSystem * pSystem, const bool _bDisableChange )
: pList( _pList ), optionsDescs( _optionDescs ), nIDToStartFrom( _nIDToStartFrom ), bDisableChange( _bDisableChange ), pSetOptionSystem( pSystem )
{
	InitList( false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void COptionsListWrapper::InitList( const bool bDefault )
{
	std::vector< CPtr<IOption> > optionsToGive;
	IOptionSystem * pSystem = pSetOptionSystem ? pSetOptionSystem : GetSingleton<IOptionSystem>();

	for ( OptionDescs::const_iterator it = optionsDescs.begin(); it != optionsDescs.end(); ++it )
	{
		const SOptionDesc * pDesc = &(*it);
		
		IOption * pOption = 0;

		switch( pDesc->nEditorType )
		{
		case EOET_GAMESPY_TEXT_ENTRY:
			{
				NI_ASSERT_T( pDesc->nDataType == VT_BSTR, "EOET_TEXT_ENTRY is allowed only for VT_BSTR type" );
				variant_t val ;
				if ( !bDefault && pSystem->Get( pDesc->szName, &val ) )
					pOption = new COptionTextEntryGameSpyCharacters( pDesc->szName.c_str(), pDesc->bInstantApply, (wchar_t*)(_bstr_t)val );
				else
					pOption = new COptionTextEntryGameSpyCharacters( pDesc->szName.c_str(), pDesc->bInstantApply, (wchar_t*)(_bstr_t)pDesc->defaultValue );
			}

			break;
		case EOET_TEXT_ENTRY:
			{
				NI_ASSERT_T( pDesc->nDataType == VT_BSTR, "EOET_TEXT_ENTRY is allowed only for VT_BSTR type" );
				variant_t val ;
				if ( !bDefault && pSystem->Get( pDesc->szName, &val ) )
					pOption = new COptionTextEntry( pDesc->szName.c_str(), pDesc->bInstantApply, (wchar_t*)(_bstr_t)val );
				else
					pOption = new COptionTextEntry( pDesc->szName.c_str(), pDesc->bInstantApply, (wchar_t*)(_bstr_t)pDesc->defaultValue );
			}

			break;
		case EOET_NUMERIC_ENTRY:
			{
				NI_ASSERT_T( pDesc->nDataType == VT_I4, "EOET_NUMERIC_ENTRY is allowed only for VT_I4 type" );
				variant_t val ;
				if ( !bDefault && pSystem->Get( pDesc->szName, &val ) )
					pOption = new COptionNumericEntry( pDesc->szName.c_str(), pDesc->bInstantApply, long(val) );
				else
					pOption = new COptionNumericEntry( pDesc->szName.c_str(), pDesc->bInstantApply, long(pDesc->defaultValue) );
			}

			break;
		case EOET_SLIDER:
			{
				NI_ASSERT_T( pDesc->nDataType == VT_I4, "EOET_SLIDER is allowed only for VT_I4 type" );
				//NI_ASSERT_T( pDesc->defaultValue >= 0 && pDesc->defaultValue <= 100, NStr::Format( "VALUES from 0 to 100 allowed for slider, option %s", pDesc->szName.c_str() )  );
				variant_t val ;
				if ( !bDefault && pSystem->Get( pDesc->szName, &val ) )
					pOption = new COptionSlider( pDesc->szName.c_str(), pDesc->bInstantApply, 0, 100, long(val) );
				else
					pOption = new COptionSlider( pDesc->szName.c_str(), pDesc->bInstantApply, 0, 100, long(pDesc->defaultValue) );
			}
			
			break;
		case EOET_CLICK_SWITCHES:
			{
				NI_ASSERT_T( pDesc->nDataType == VT_BSTR, NStr::Format( "EOET_CLICK_SWITCHES is allowed only for VT_BSTR type (option %s)", pDesc->szName.c_str() ) );

				const std::vector<SOptionDropListValue>& values = pSystem->GetDropValues( pDesc->szName );
				NI_ASSERT_T( !values.empty(), NStr::Format( "cannot fill selections for %s", pDesc->szName ) );
				std::wstring szCurrent;
				variant_t val;
				if ( !bDefault && pSystem->Get( pDesc->szName, &val ) )
					szCurrent = (wchar_t*)(_bstr_t)val;
				else
					szCurrent = (wchar_t*)(_bstr_t)pDesc->defaultValue;
				pOption = new COptionSelection( pDesc->szName.c_str(), pDesc->bInstantApply, values, NStr::ToAscii(szCurrent).c_str() );
			}
			break;
		}
		if ( pOption )
		{
			if ( pSetOptionSystem )
				pOption->SetOptionSystem( pSetOptionSystem );
			optionsToGive.push_back( pOption );
		}
	}

	options.resize( optionsToGive.size() );
	for ( int i = 0; i < options.size(); ++i )
	{
		pList->AddItem();
		IUIListRow * pRow = pList->GetItem( i );
		CPtr<IUIStatic> pStatic = checked_cast<IUIStatic*>( pRow->GetElement( 0 ) );
		pStatic->SetWindowID( nIDToStartFrom + i * 2 );
		pStatic->EnableWindow( !bDisableChange );
		CPtr<IUIDialog> pDialog = checked_cast<IUIDialog*>( pRow->GetElement( 1 ) );
		pDialog->SetWindowID( nIDToStartFrom + i * 2 + 1 );
		pDialog->EnableWindow( !bDisableChange );
		
		CPtr<CUIOption> pOption = new CUIOption( pStatic, pDialog, optionsToGive[i] );
		options[i] = pOption;
	}
	pList->InitialUpdate();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void COptionsListWrapper::Apply()
{
	for ( int i = 0; i < options.size(); ++i )
		if ( options[i]->IsOptionValid() )
			options[i]->Apply();
	pList->InitialUpdate();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void COptionsListWrapper::CancelChanges()
{
	for ( int i = 0; i < options.size(); ++i )
		if ( options[i]->IsOptionValid() )
			options[i]->CancelChanges();
	pList->InitialUpdate();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void COptionsListWrapper::ToDefault()
{
	while( pList->GetNumberOfItems() )
		pList->RemoveItem( 0 );
	InitList( true );
	Apply();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void COptionsListWrapper::DisableChange() 
{ 
	bDisableChange = true; 
	for ( int nRow = 0; nRow < pList->GetNumberOfItems(); ++nRow )
	{
		IUIListRow * pRow = pList->GetItem( nRow );
		for ( int nElement = 0; nElement < pRow->GetNumberOfElements(); ++nElement )
		{
			IUIElement * pEl = pRow->GetElement( nElement );
			pEl->EnableWindow( false );
		}
	}
	pList->EnableWindow( false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool COptionsListWrapper::ProcessMessage( const SGameMessage &msg )
{
	switch( msg.nEventID )
	{
	case UI_NOTIFY_SELECTION_CHANGED:
		if ( msg.nParam == 7777 )
		{
			const int nOption = pList->GetSelectionItem();
			if( -1 != nOption && options[nOption]->IsOptionValid() )
				options[nOption]->OnSelected();
			return true;
		}

		break;
	case 7777:
		if ( msg.nParam == 7777 )
		{
			const int nOption = pList->GetSelectionItem();
			if( -1 != nOption && options[nOption]->IsOptionValid() )
				options[nOption]->OnClicked( true );
			return true;
		}

		break;
	case 7779:
		if ( msg.nParam == 7779 )
		{
			const int nOption = pList->GetSelectionItem();
			if( -1 != nOption && options[nOption]->IsOptionValid() )
				options[nOption]->OnClicked( false );
			return true;
		}

		break;
	case 7778:
		if ( msg.nParam == 7778 )
		{
			const int nOption = pList->GetSelectionItem();
			
			if( -1 != nOption && options[nOption]->IsOptionValid() )
			{
				options[nOption]->PositionChanged();
			}
			return true;
		}

		break;
	}
	return false;
}