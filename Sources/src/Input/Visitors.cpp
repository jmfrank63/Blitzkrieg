#include "StdAfx.h"

#include "Visitors.h"
#include "InputBinder.h"
bool CSetBindSectionVisitor::VisitControl( CControl *pControl )
{
	if ( pControl ) 
		pControl->Visit( this );
	return true;
}
bool CSetBindSectionVisitor::VisitCombo( CCombo *pCombo ) 
{ 
	if ( combos.find(pCombo) == combos.end() ) 
	{
		pCombo->ChangeMappingSection( szBindSection, dwTime ); 
		combos.insert( pCombo );
	}
	return true;
}
CFindBindVisitor::CFindBindVisitor( const SCommand *_pCommand, const EInputBindActivationType _eType, 
																	  const std::vector<const CControl*> &_controls )
: pCommand2Find( _pCommand ), eType( _eType ), pCurrControl( 0 ), pCurrCombo( 0 ), pFoundCombo( 0 ), pFoundBind( 0 ) 
{  
	for ( std::vector<const CControl*>::const_iterator it = _controls.begin(); it != _controls.end(); ++it )
		controls.insert( CControlsMap::value_type( *it, std::list<CBind*>() ) );
}
bool CFindBindVisitor::VisitControl( CControl *pControl ) 
{  
	if ( controls.find(pControl) == controls.end() ) 
		return true;												// continue to search
	pCurrControl = pControl;
	return pControl->Visit( this );
}
bool CFindBindVisitor::VisitCombo( CCombo *pCombo ) 
{  
	pCurrCombo = pCombo;
	return pCombo->Visit( this );
}
bool CFindBindVisitor::VisitBind( CBind *pBind ) 
{  
	if ( (pBind->GetType() == eType) && (pBind->Visit(this) == true) )
	{
		controls[pCurrControl].push_back( pBind );
		binds[pBind].nCounter++;
		NI_ASSERT_T( (binds[pBind].pCombo == 0) || (binds[pBind].pCombo == pCurrCombo), NStr::Format("ERROR: bind belong to the more then one combo!!!") );
		binds[pBind].pCombo = pCurrCombo;
	}
	return true;
}
bool CFindBindVisitor::VisitCommand( SCommand *pCommand ) 
{  
	return pCommand2Find == pCommand;
}
void CFindBindVisitor::FinalCheck() const
{
	if ( (pFoundCombo != 0) || (pFoundBind != 0) ) 
		return;
	const int nNumControls = controls.size();
	for ( CBindsMap::const_iterator it = binds.begin(); it != binds.end(); ++it )
	{
		if ( it->second.nCounter == nNumControls ) 
		{
			pFoundCombo = it->second.pCombo;
			pFoundBind = it->first;
		}
	}
}
bool CRemoveComboVisitor::VisitControl( class CControl *pControl )
{
	pControl->Visit( this );
	pControl->RemoveCombo( pCombo2Remove );
	return true;
}
bool CRemoveComboVisitor::VisitCombo( CCombo *pCombo ) 
{ 
	pCombo->RemoveSuppressive( pCombo2Remove, true );
	return true; 
}
