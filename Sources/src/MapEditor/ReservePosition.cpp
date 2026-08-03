#include "StdAfx.h"
#include "TemplateEditorFrame1.h"
#include "PropertieDialog.h"

#include "ReservePosition.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/**
CPropertiesRegister thePropertiesRegister;

IManipulator* CMutableAIStartCommand::GetManipulator()
{
	CAIStartCommandManipulator  *pManipulator = new CAIStartCommandManipulator();  	
	pManipulator->SetObject( this );
	return pManipulator; 
}

void CMutableAIStartCommand::Update( CTemplateEditorFrame *pFrame )
{
	if ( pFrame->dlg )
	{
		pFrame->dlg->ClearVariables();
		CPtr<IManipulator> pMan = GetManipulator();
		pFrame->dlg->AddObjectWithProp( pMan );
	}
}

void CMutableAIStartCommand::Update( TMutableAIStartCommandList *pCommands, CTemplateEditorFrame *pFrame )
{
	if ( pFrame->dlg )
	{
		CPtr<IMultiManipulator> pMan = new CMultiManipulator;
		for ( TMutableAIStartCommandList::iterator it = pCommands->begin(); it != pCommands->end(); ++it )
		{
			pMan->AddManipulator( it->GetManipulator() );
		}
		pFrame->dlg->AddObjectWithProp( pMan );
	}
}

CAIStartCommandManipulator::CAIStartCommandManipulator() 
: CManipulator( &thePropertiesRegister, "AI Start Command" )
{
	if ( DoWeNeedFillProps() )							
	{																				

		typedef SProperty<CAIStartCommandManipulator> CAIStartCommandProperty;

		CAIStartCommandProperty *pProperty = 0;
		
		pProperty = AddNewProperty( this,
																"Command",
																&CAIStartCommandManipulator::SetCmdType,
																&CAIStartCommandManipulator::GetCmdType,
																SPropertyDesc::VAL_COMBO,
																SBaseProperty::LEAF );
		{
			for( int index = 0; index < ACTION_COMMAND_COUNT; ++ index )
			{
				pProperty->values.push_back( szActionCommandNames[index] ) ;
			}
		}
		pProperty = AddNewProperty( this,
																"Units",
																&CAIStartCommandManipulator::SetUnitNumber,
																&CAIStartCommandManipulator::GetUnitNumber,
																SPropertyDesc::VAL_UNITS,
																SBaseProperty::LEAF );
		pProperty = AddNewProperty( this,
																"LinkID",
																&CAIStartCommandManipulator::SetLinkID,
																&CAIStartCommandManipulator::GetLinkID,
																SPropertyDesc::VAL_INT,
																SBaseProperty::LEAF );
		pProperty = AddNewProperty( this,
																"vPos:x",
																&CAIStartCommandManipulator::SetVPosX,
																&CAIStartCommandManipulator::GetVPosX,
																SPropertyDesc::VAL_FLOAT,
																SBaseProperty::LEAF );
		pProperty = AddNewProperty( this,
																"vPos:y",
																&CAIStartCommandManipulator::SetVPosY,
																&CAIStartCommandManipulator::GetVPosY,
																SPropertyDesc::VAL_FLOAT,
																SBaseProperty::LEAF );
		pProperty = AddNewProperty( this,
																"Number",
																&CAIStartCommandManipulator::SetNumber,
																&CAIStartCommandManipulator::GetNumber,
																SPropertyDesc::VAL_FLOAT,
																SBaseProperty::LEAF );
	}
}


void CAIStartCommandManipulator::SetCmdType( const variant_t &value )
{
	pMutableObject->cmdType = (EActionCommand)DEFAULT_ACTION_COMMAND;	
	for ( int index = 0; index < ACTION_COMMAND_COUNT; ++index )
	{
		if ( value == variant_t(szActionCommandNames[index]) )
		{
			pMutableObject->cmdType = (EActionCommand)index;	
		}
	}
}

void CAIStartCommandManipulator::GetCmdType( variant_t *pValue, int nIndex )
{
	if ( ( pMutableObject->cmdType < 0 ) || ( pMutableObject->cmdType >= ACTION_COMMAND_COUNT ) )
	{
		*pValue = variant_t(szActionCommandNames[DEFAULT_ACTION_COMMAND]);
	}
	else
	{
		*pValue = variant_t(szActionCommandNames[pMutableObject->cmdType]);
	}
}

void CAIStartCommandManipulator::SetUnitNumber( const variant_t &value )
{
}

void CAIStartCommandManipulator::GetUnitNumber( variant_t *pValue, int nIndex )
{
	if ( pMutableObject->pMapObjects.empty() )
	{
		*pValue = variant_t("no nits");
	}
	else
	{
		*pValue = variant_t( NStr::Format( "%d units", pMutableObject->pMapObjects.size() ) );
	}
}

void CAIStartCommandManipulator::SetLinkID( const variant_t &value )
{
}

void CAIStartCommandManipulator::GetLinkID( variant_t *pValue, int nIndex )
{
	*pValue = variant_t( long( pMutableObject->linkID ) );
}

void CAIStartCommandManipulator::SetVPosX( const variant_t &value )
{
	pMutableObject->vPos.x = float(value);
}

void CAIStartCommandManipulator::GetVPosX( variant_t *pValue, int nIndex )
{
	*pValue = variant_t( pMutableObject->vPos.x );
}

void CAIStartCommandManipulator::SetVPosY( const variant_t &value )
{
	pMutableObject->vPos.y = float(value);
}

void CAIStartCommandManipulator::GetVPosY( variant_t *pValue, int nIndex )
{
	*pValue = variant_t( pMutableObject->vPos.y );
}


void CAIStartCommandManipulator::SetNumber( const variant_t &value )
{
	pMutableObject->fromExplosion = float(value);
}

void CAIStartCommandManipulator::GetNumber( variant_t *pValue, int nIndex )
{
	*pValue = variant_t( pMutableObject->fNumber );
}
/**/
