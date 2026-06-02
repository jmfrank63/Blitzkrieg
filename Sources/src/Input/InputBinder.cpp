#include "StdAfx.h"

#include "InputBinder.h"

#include "InputSlider.h"
#include "Visitors.h"
BASIC_REGISTER_CLASS( CCombo );
BASIC_REGISTER_CLASS( CInputBinder );
inline const EInputBindActivationType GetTypeFromName( const std::string &szName )
{
	if ( szName == "event down" ) 
		return INPUT_BIND_ACTIVATION_TYPE_EVENT_DOWN;
	else if ( szName == "event up" ) 
		return INPUT_BIND_ACTIVATION_TYPE_EVENT_UP;
	else if ( szName == "slider plus" ) 
		return INPUT_BIND_ACTIVATION_TYPE_SLIDER_PLUS;
	else if ( szName == "slider minus" ) 
		return INPUT_BIND_ACTIVATION_TYPE_SLIDER_MINUS;
	return INPUT_BIND_ACTIVATION_TYPE_UNKNOWN;
}
inline const std::string GetNameFromType( const EInputBindActivationType eType )
{
	switch ( eType ) 
	{
		case INPUT_BIND_ACTIVATION_TYPE_EVENT_DOWN:
			return "event down";
		case INPUT_BIND_ACTIVATION_TYPE_EVENT_UP:
			return "event up";
		case INPUT_BIND_ACTIVATION_TYPE_SLIDER_PLUS:
			return "slider plus";
		case INPUT_BIND_ACTIVATION_TYPE_SLIDER_MINUS:
			return "slider minus";
	}
	return "unknown";
}
void CControl::NotifyAllCombos( const bool bActivated, const DWORD time, const int nParam )
{
	if ( bActivated ) 
	{
		for ( CCombosList::iterator it = combos.begin(); it != combos.end(); ++it )
			(*it)->NotifyControlStateChanged( bActivated, time, nParam );
	}
	else
	{
		for ( CCombosList::reverse_iterator it = combos.rbegin(); it != combos.rend(); ++it )
			(*it)->NotifyControlStateChanged( bActivated, time, nParam );
	}
}
struct SCombosCmp
{
	const bool operator()( const CObj<CCombo> &c1, const CObj<CCombo> &c2 ) const
	{
		return c1->GetNumControls() > c2->GetNumControls();
	}
};
void CControl::AddCombo( CCombo *pCombo )
{
	const CObj<CCombo> comboPtr( pCombo );
	if ( std::find( combos.begin(), combos.end(), comboPtr ) == combos.end() )
	{
		combos.push_back( pCombo );
		combos.sort( SCombosCmp() );
	}
}
void CControl::SetBindSection( const std::string &szName )
{
	for ( CCombosList::iterator it = combos.begin(); it != combos.end(); ++it )
		(*it)->ChangeMappingSection( szName );
}
CCombo::CCombo( const CControlsPtrList &_controls ) 
: pMapping( 0 ), nSuppressCounter( 0 ), bFormed( false ), bLastNotifyFormed( false ), fPower( 0 ), nBestParam( 0 )
{  
	controls = _controls;
	eControlType = CONTROL_TYPE_KEY;
	for ( CControlsPtrList::const_iterator it = controls.begin(); it != controls.end(); ++it )
		eControlType = Max( eControlType, (*it)->GetType() );
}
CCombo::~CCombo()
{
	for ( CMappingsMap::iterator it = mappings.begin(); it != mappings.end(); ++it ) 
	{
		while ( !it->second.binds.empty() )
		{
			delete ( it->second.binds.back() );
			it->second.binds.pop_back();
		}
	}
}
void CCombo::SMapping::Suppress( const int nAdd, const DWORD time )
{
	for ( CCombo::CCombosList::iterator it = suppressives.begin(); it != suppressives.end(); ++it )
		(*it)->Suppress( nAdd, time );
}
void CCombo::Suppress( const int nAdd, DWORD time )
{
	if ( !IsBinded() ) 
		return;
	if ( (nSuppressCounter == 0) && (nAdd < 0) ) 
	{
		NStr::DebugTrace( "Special case reached: suppress counter == 0 and suppressing < 0! This can be possible only during bind section changing.\n" );
		return;
	}
	if ( (nSuppressCounter == 0) && bFormed ) 
		NotifyBinds( false, time );
	nSuppressCounter += nAdd;
	if ( nSuppressCounter == 0 ) 
		NotifyBinds( bFormed, time );
}
void CCombo::NotifyBinds( const bool bFormedLocal, const DWORD time, const bool bForced )
{
	if ( (bLastNotifyFormed != bFormedLocal) || bForced )
	{
		for ( CBindsPtrList::iterator it = pMapping->binds.begin(); it != pMapping->binds.end(); ++it )
			(*it)->NotifyComboStateChanged( bFormedLocal, fPower, eControlType, time, nBestParam );
	}
	bLastNotifyFormed = bFormedLocal;
}
void CCombo::ChangeMappingSection( const std::string &szMapping ) 
{ 
	pMapping = &( mappings[szMapping] ); 
	pMapping->szName = szMapping;
	nSuppressCounter = 0;
	nBestParam = 0;
}
void CCombo::NotifyControlStateChanged( const bool bActivated, const DWORD time, const int nParam )
{
	if ( !IsBinded() ) 
		return;
	if ( nParam & 0x40000000 ) 
		nBestParam = nParam;
	bool bFormedLocal = true;
	fPower = 0;
	for ( CControlsPtrList::const_iterator it = controls.begin(); it != controls.end(); ++it )
	{
		bFormedLocal &= (*it)->IsActive();
		const float fValue = (*it)->GetValue();
		if ( fabsf(fPower) == fabsf(fValue) ) 
			fPower = Min( fPower, fValue );
		else if ( fabsf(fPower) < fabsf(fValue) ) 
			fPower = fValue;
	}
	if ( bFormedLocal != bFormed ) 
	{
		pMapping->Suppress( bFormedLocal ? 1 : -1, time );
		if ( nSuppressCounter == 0 ) 
			NotifyBinds( bFormedLocal, time );
	}
	else if ( bFormedLocal && (nSuppressCounter == 0) )	// повторная активация с другим значением возможна только для 'rotational axis'
		NotifyBinds( bFormedLocal, time, true );
	bFormed = bFormedLocal;
	if ( !bActivated ) 
		nBestParam = 0;
}
void CCombo::AddBind( CBind *pBind )
{
	if ( std::find(pMapping->binds.begin(), pMapping->binds.end(), pBind) == pMapping->binds.end() )
	{
		pMapping->binds.push_back( pBind );
	}
	else
	{
		NI_ASSERT_T( false, "Bind already exist in the mapping!" );
	}
}
inline bool IsSubset( const std::vector<const CControl*> &c1, const std::vector<const CControl*> &c2 )
{
	for ( std::vector<const CControl*>::const_iterator it = c1.begin(); it != c1.end(); ++it )
	{
		if ( std::find(c2.begin(), c2.end(), *it) == c2.end() ) 
			return false;
	}
	return true;
}
inline const ESetCompare Compare( const std::vector<const CControl*> &c1, const std::vector<const CControl*> &c2 )
{
	if ( c1.size() == c2.size() ) 
		return IsSubset( c1, c2 ) ? SET_COMPARE_EQUAL : SET_COMPARE_UNRELATED;
	else if ( c1.size() < c2.size() ) 
		return IsSubset( c1, c2 ) ? SET_COMPARE_SUBSET : SET_COMPARE_UNRELATED;
	else
		return IsSubset( c2, c1 ) ? SET_COMPARE_SUPERSET : SET_COMPARE_UNRELATED;
}
const ESetCompare CCombo::Compare( const std::vector<const CControl*> &c1 ) const { return ::Compare( controls, c1 ); }
void CBindEventForm::NotifyComboStateChanged( const bool bFormed, const float fPower, const EControlType eControlType, 
																						  const DWORD time, const int nParam )
{
	if ( bFormed ) 
		pCommand->ActivateEvent( time, nParam );
}
void CBindEventDestroy::NotifyComboStateChanged( const bool bFormed, const float fPower, const EControlType eControlType, 
																								 const DWORD time, const int nParam )
{
	if ( !bFormed ) 
		pCommand->ActivateEvent( time, nParam );
}
void CBindSliderPlus::NotifyComboStateChanged( const bool bFormed, const float fPower, const EControlType eControlType, 
																							 const DWORD time, const int nParam )
{
	pCommand->ActivateSlider( bFormed, fPower, eControlType, time, nParam );
}
void CBindSliderMinus::NotifyComboStateChanged( const bool bFormed, const float fPower, const EControlType eControlType, 
																							  const DWORD time, const int nParam )
{
	pCommand->ActivateSlider( bFormed, -fPower, eControlType, time, nParam );
}
void SCommand::ActivateSlider( const bool bActivate, const float fPower, EControlType eControlType, const DWORD time, const int nParam )
{
	if ( (pInput->GetTextModeLocal() == INPUT_TEXT_MODE_SYSKEYS) && !bSystem ) 
		return;

	const char *pszType = 0;
	switch ( eControlType ) 
	{
		case CONTROL_TYPE_KEY:
			if ( bActivate ) 
				accumulator.ActivateKey( time, fPower );
			else
				accumulator.DeactivateKey( time );
			pszType = "key";
			break;
		case CONTROL_TYPE_AXIS:
			if ( bActivate ) 
				accumulator.ActivateAxis( time, fPower );
			else
				accumulator.DeactivateAxis( time );
			pszType = "axis";
			break;
		case CONTROL_TYPE_RAXIS:
		case CONTROL_TYPE_POV_X:
		case CONTROL_TYPE_POV_Y:
			if ( bActivate ) 
				accumulator.ActivateRAxis( time, fPower );
			else
				accumulator.DeactivateRAxis( time );
			pszType = "rotation axis";
			break;
	}
}
void SCommand::ActivateEvent( const DWORD time, const int nParam )
{
	if ( IsRegistered() && 
		   ((pInput->GetTextModeLocal() != INPUT_TEXT_MODE_SYSKEYS) || 
			  ((pInput->GetTextModeLocal() == INPUT_TEXT_MODE_SYSKEYS) && bSystem)) ) 
	{
		pInput->AddEventLocal( nEventID, nParam );
	}
}
CInputBinder::CInputBinder()
: szCurrentMapping( "default" )
{
	bMappingChanged = false;
}
template <class TYPE>
inline void Add( std::list<TYPE> &lst, TYPE element )
{
	if ( std::find(lst.begin(), lst.end(), element) == lst.end() )
		lst.push_back( element );
}
void CInputBinder::TransformBind( SBindsConfig::SBindSection::SCommandBind *pRes, const IInputBind *pBind )
{
	pRes->szName = pBind->GetCommand();
	pRes->szBindType = GetNameFromType( pBind->GetActivationType() );
	const int nNumControls = pBind->GetNumControls();
	pRes->controls.reserve( nNumControls );
	for ( int i = 0; i < nNumControls; ++i )
		pRes->controls.push_back( pBind->GetControl(i) );
}
void CInputBinder::TransformBind( IInputBind *pRes, const SBindsConfig::SBindSection::SCommandBind *pBind )
{
	pRes->SetCommand( pBind->szName.c_str(), GetTypeFromName(pBind->szBindType) );
	for ( std::vector<std::string>::const_iterator it = pBind->controls.begin(); it != pBind->controls.end(); ++it )
		pRes->AddControl( it->c_str() );
}
void CInputBinder::AddBind( const IInputBind *pInputBind )
{
	SBindsConfig::SBindSection::SCommandBind bind;
	TransformBind( &bind, pInputBind );

	if ( AddBindLocal(bind) )
	{
		SBindsConfig::SBindSection *pSection = 0;
		for ( std::vector<SBindsConfig::SBindSection>::iterator it = config.sections.begin(); it != config.sections.end(); ++it )
		{
			if ( it->szName == szCurrentMapping ) 
			{
				pSection = &( *it );
				break;
			}
		}
		if ( pSection == 0 ) 
		{
			SBindsConfig::SBindSection section;
			section.szName = szCurrentMapping;
			config.sections.push_back( section );
			pSection = &( config.sections.back() );
		}
		for ( std::vector<SBindsConfig::SBindSection::SCommandBind>::iterator cmd = pSection->commands.begin(); cmd != pSection->commands.end(); ++cmd )
		{
			if ( (cmd->szName == bind.szName) && (cmd->szBindType == bind.szBindType) ) 
			{
				*cmd = bind;
				return;
			}
		}
		pSection->commands.push_back( bind );
		bMappingChanged = true;
		return;
	}
}
bool CInputBinder::AddBindLocal( const SBindsConfig::SBindSection::SCommandBind &bind )
{
	if ( bind.szName.empty() || bind.szBindType.empty() || bind.controls.empty() )
		return false;
	const EInputBindActivationType eActivationType = GetTypeFromName( bind.szBindType );
	if ( eActivationType == INPUT_BIND_ACTIVATION_TYPE_UNKNOWN ) 
	{
		const std::string szError = NStr::Format( "ERROR (bind): section \"%s\", command \"%s\" of type \"%s\" - unknown bind type", 
			                                        szCurrentMapping.c_str(), bind.szName.c_str(), bind.szBindType.c_str() );
		NI_ASSERT_T( eActivationType != INPUT_BIND_ACTIVATION_TYPE_UNKNOWN, szError.c_str() );
		return false;
	}
	SCommand *pCommand = GetCommand( bind.szName );
	const int nNumControls = bind.controls.size();
	std::vector<const CControl*> controls;
	controls.reserve( bind.controls.size() );
	for ( std::vector<std::string>::const_iterator it = bind.controls.begin(); it != bind.controls.end(); ++it )
	{
		const CControl *pControl = GetControlByName( *it );
		if ( pControl == 0 ) 
		{
			/*
			const std::string szError = NStr::Format( "ERROR (bind): section \"%s\", command \"%s\" of type \"%s\" - unknown control \"%s\"", 
																								szCurrentMapping.c_str(), bind.szName.c_str(), bind.szBindType.c_str(), it->c_str() );
			NI_ASSERT_SLOW_T( pControl != 0, szError.c_str() );
			*/
			return false;
		}
		controls.push_back( pControl );
	}
	std::list<CCombo*> subsets, supersets;
	CPtr<CCombo> pCombo;
	for ( std::vector<const CControl*>::const_iterator control = controls.begin(); control != controls.end(); ++control )
	{
		const std::list< CObj<CCombo> > &combos = (*control)->GetCombos();
		for ( std::list< CObj<CCombo> >::const_iterator combo = combos.begin(); combo != combos.end(); ++combo )
		{
			switch ( (*combo)->Compare(controls) )
			{
				case SET_COMPARE_SUBSET:
					Add( subsets, combo->GetPtr() );
					break;
				case SET_COMPARE_SUPERSET:
					Add( supersets, combo->GetPtr() );
					break;
				case SET_COMPARE_EQUAL:
					pCombo = *combo;
					break;
			}
		}
	}
	if ( pCombo == 0 ) 
		pCombo = new CCombo( controls );
	pCombo->ChangeMappingSection( szCurrentMapping );
	for ( std::vector<const CControl*>::iterator it = controls.begin(); it != controls.end(); ++it )
		const_cast<CControl*>( (*it) )->AddCombo( pCombo );
	for ( std::list<CCombo*>::iterator it = subsets.begin(); it != subsets.end(); ++it )
		pCombo->AddSuppressive( *it );
	for ( std::list<CCombo*>::iterator it = supersets.begin(); it != supersets.end(); ++it )
		(*it)->AddSuppressive( pCombo );
	CBind *pLocalBind = 0;
	switch ( eActivationType ) 
	{
		case INPUT_BIND_ACTIVATION_TYPE_EVENT_DOWN:
			pLocalBind = new CBindEventForm( pCommand );
			break;
		case INPUT_BIND_ACTIVATION_TYPE_EVENT_UP:
			pLocalBind = new CBindEventDestroy( pCommand );
			break;
		case INPUT_BIND_ACTIVATION_TYPE_SLIDER_PLUS:
			pLocalBind = new CBindSliderPlus( pCommand );
			break;
		case INPUT_BIND_ACTIVATION_TYPE_SLIDER_MINUS:
			pLocalBind = new CBindSliderMinus( pCommand );
			break;
	}
	{
		CPtr<IInputBind> pInputBind = CreateObject<IInputBind>( INPUT_BIND );
		TransformBind( pInputBind, &bind );
		RemoveBind( pInputBind );
	}
	pCombo->AddBind( pLocalBind );
	return true;
}
void CInputBinder::SetBindSection( const char *pszSectionName )
{
	NStr::DebugTrace( "****** Set bind section to \"%s\"\n", pszSectionName );
	szCurrentMapping = pszSectionName;
	CSetBindSectionVisitor visitor( pszSectionName );
	VisitControls( &visitor );
}
void CInputBinder::RemoveBind( const IInputBind *pBind )
{
	const char *pszCommand = pBind->GetCommand();
	EInputBindActivationType eType = pBind->GetActivationType();
	std::vector<const CControl*> controls;
	for ( int i = 0; i < pBind->GetNumControls(); ++i )
	{
		const char *pszControl = pBind->GetControl( i );
		const CControl *pControl = GetControlByName( pszControl );
		if ( pControl != 0 ) 
			controls.push_back( pControl );
		else
		{
			NStr::DebugTrace( "Can't find control \"%s\" to remove bind with command \"%s\"\n", pszControl, pszCommand );
			return;
		}
	}
	CCommandsMap::iterator pos = commands.find( pszCommand );
	if ( pos == commands.end() ) 
		return;
	SCommand *pCommand = &( pos->second );
	CFindBindVisitor visitor( pCommand, eType, controls );
	VisitControls( &visitor );
	if ( (visitor.GetBind() == 0) || (visitor.GetCombo() == 0) ) 
		return;
	CPtr<CCombo> pCombo = visitor.GetCombo();
	pCombo->RemoveBind( visitor.GetBind() );
	delete ( visitor.GetBind() );
	if ( pCombo->IsEmpty() ) 
	{
		CRemoveComboVisitor visRemove( pCombo );
		VisitControls( &visRemove );
	}
}
void CInputBinder::RegisterCommand( const char *pszName, const int nEventID )
{
	SCommand *pCommand = GetCommand( pszName );
	pCommand->SetEventID( nEventID );
	pCommand->Register( true );
}
void CInputBinder::UnRegisterCommand( const char *pszName ) 
{  
	CCommandsMap::iterator pos = commands.find( pszName );
	if ( pos != commands.end() ) 
		pos->second.Register( false );
}
IInputSlider* CInputBinder::CreateSlider( const char *pszName, const float fPower ) 
{ 
	return new CInputSlider( this, GetCommand(pszName), fPower );
}
void CInputBinder::Repair( IDataTree *pSS, const bool bToDefault )
{
	if ( pSS == 0 ) 
		return;
	const std::string szOldBindSection = szCurrentMapping;
	SBindsConfig repairs;
	{
		CTreeAccessor saver = pSS;
		saver.Add( "Binds", &repairs );
	}
	if ( bToDefault ) 
	{
		config.Clear();
	}
	const SBindsConfig::SBindSection *pDefaultSection = 0;
	for ( std::vector<SBindsConfig::SBindSection>::const_iterator itConfigSect = config.sections.begin(); itConfigSect != config.sections.end(); ++itConfigSect )
	{
		if ( itConfigSect->szName == "default" ) 
		{
			pDefaultSection = &( *itConfigSect );
			break;
		}
	}
	for ( std::vector<std::string>::const_iterator it = repairs.dblclks.begin(); it != repairs.dblclks.end(); ++it )
	{
		if ( std::find(config.dblclks.begin(), config.dblclks.end(), *it) == config.dblclks.end() )
		{
			AddDoubleClick( *it );
			config.dblclks.push_back( *it );
			bMappingChanged = true;
		}
	}
	for ( std::vector<SBindsConfig::SControlPower>::const_iterator it = repairs.powers.begin(); it != repairs.powers.end(); ++it )
	{
		if ( std::find(config.powers.begin(), config.powers.end(), *it) == config.powers.end() ) 
		{
			SetPower( it->szControlName, it->fPower );
			config.powers.push_back( *it );
			bMappingChanged = true;
		}
	}
	for ( std::vector<SBindsConfig::SBindSection>::const_iterator itRepairSect = repairs.sections.begin(); itRepairSect != repairs.sections.end(); ++itRepairSect )
	{
		bool bFoundSection = false;
		for ( std::vector<SBindsConfig::SBindSection>::const_iterator itConfigSect = config.sections.begin(); itConfigSect != config.sections.end(); ++itConfigSect )
		{
			if ( itConfigSect->szName == itRepairSect->szName ) 
			{
				bFoundSection = true;
				break;
			}
		}
		if ( !bFoundSection )	
		{
			SetBindSection( itRepairSect->szName.c_str() );
			for ( std::vector<SBindsConfig::SBindSection::SCommandBind>::const_iterator bind = itRepairSect->commands.begin(); bind != itRepairSect->commands.end(); ++bind )
				AddBindLocal( *bind );
			if ( pDefaultSection ) 
			{
				for ( std::vector<SBindsConfig::SBindSection::SCommandBind>::const_iterator bind = pDefaultSection->commands.begin(); bind != pDefaultSection->commands.end(); ++bind )
					AddBindLocal( *bind );
			}
			else if ( itRepairSect->szName == "default" ) 
				pDefaultSection = &( *itRepairSect );
			config.sections.push_back( *itRepairSect );
			bMappingChanged = true;
		}
	}
	for ( std::vector<std::string>::const_iterator it = repairs.syscmds.begin(); it != repairs.syscmds.end(); ++it )
	{
		if ( std::find(config.syscmds.begin(), config.syscmds.end(), *it) == config.syscmds.end() )
		{
			SetSystemCommand( *it );
			config.syscmds.push_back( *it );
			bMappingChanged = true;
		}
	}
	SetBindSection( szOldBindSection.c_str() );
}
bool CInputBinder::SerializeConfig( IDataTree *pSS )
{
	if ( pSS == 0 ) 
		return false;
	CTreeAccessor saver = pSS;
	if ( saver.IsReading() ) 
		config.Clear();
	saver.Add( "Binds", &config );
	if ( saver.IsReading() ) 
	{
		for ( std::vector<std::string>::const_iterator it = config.dblclks.begin(); it != config.dblclks.end(); ++it )
			AddDoubleClick( *it );
		for ( std::vector<SBindsConfig::SControlPower>::const_iterator it = config.powers.begin(); it != config.powers.end(); ++it )
			SetPower( it->szControlName, it->fPower );
		const SBindsConfig::SBindSection *pDefaultSection = 0;
		for ( std::vector<SBindsConfig::SBindSection>::const_iterator section = config.sections.begin(); section != config.sections.end(); ++section )
		{
			SetBindSection( section->szName.c_str() );
			for ( std::vector<SBindsConfig::SBindSection::SCommandBind>::const_iterator bind = section->commands.begin(); bind != section->commands.end(); ++bind )
				AddBindLocal( *bind );
			if ( pDefaultSection ) 
			{
				for ( std::vector<SBindsConfig::SBindSection::SCommandBind>::const_iterator bind = pDefaultSection->commands.begin(); bind != pDefaultSection->commands.end(); ++bind )
					AddBindLocal( *bind );
			}
			if ( section->szName == "default" ) 
			{
				NI_ASSERT_T( pDefaultSection == 0, "Can't assign default section - this section already assigned" );
				pDefaultSection = &( *section );
			}
		}
		for ( std::vector<std::string>::const_iterator it = config.syscmds.begin(); it != config.syscmds.end(); ++it )
			SetSystemCommand( *it );
	}
	SetBindSection( "default" );
	return true;
}
int CInputBinder::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szCurrentMapping );
	if ( saver.IsReading() ) 
	{
		const std::string szMapping = szCurrentMapping;
		SetBindSection( szMapping.c_str() );
		bMappingChanged = false;
	}
	saver.AddTypedSuper( 10, static_cast<CInputAPI*>(this) );
	return 0;
}
