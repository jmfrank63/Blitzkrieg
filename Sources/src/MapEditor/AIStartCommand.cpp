#include "stdafx.h"
#include "editor.h"
#include "TemplateEditorFrame1.h"
#include "PropertieDialog.h"
#include "frames.h"

#include "AIStartCommand.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

CPropertiesRegister thePropertiesRegister01;

const int CAISCHelper::DEFAULT_ACTION_COMMAND_INDEX = 9;

void CAISCHelper::Initialize()
{
	commands.clear();

	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	if ( pStorage )
	{
		CPtr<IDataBase> pDB = OpenDataBase( 0, TABLE_ACCESS_READ ); // to work with packed resources
		CPtr<IDataStream> pDataStream = pStorage->OpenStream( "editor\\actions.ini", STREAM_ACCESS_READ );
		if ( pDataStream != 0 )
		{
			CTableAccessor table = OpenIniDataTable( pDataStream );
			std::vector<std::string> szRowNames, szEntryNames;
			table.GetRowNames( szRowNames );
			NI_ASSERT( szRowNames.size() == 1 );
			table.GetEntryNames( szRowNames[0].c_str(), szEntryNames );
			
			for ( int nCommandIndex = 0; nCommandIndex < szEntryNames.size(); ++nCommandIndex )
			{
				commands.push_back( SCommand() );
				commands.back().szName = szEntryNames[nCommandIndex];
				commands.back().nID = table.GetInt( szRowNames[0].c_str(), szEntryNames[nCommandIndex].c_str(), -1 );
				NI_ASSERT( commands.back().nID != -1 );
			}
		}
	}
	isInitialized = true;
}

IManipulator* CMutableAIStartCommand::GetManipulator()
{
	CAIStartCommandManipulator  *pManipulator = new CAIStartCommandManipulator();  	
	pManipulator->SetObject( this );
	return pManipulator; 
}

/**
void CMutableAIStartCommand::Update( CTemplateEditorFrame *pFrame )
{
	if ( pFrame->dlg )
	{
		pFrame->dlg->ClearVariables();
		CPtr<IManipulator> pMan = GetManipulator();
		pFrame->dlg->AddObjectWithProp( pMan );
	}
}
/**/
/**
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
/**/

CAIStartCommandManipulator::CAIStartCommandManipulator() 
: CManipulator( &thePropertiesRegister01, "AI Start Command" )
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
			for( int nCommandIndex = 0; nCommandIndex < g_frameManager.GetTemplateEditorFrame()->aiscHelper.commands.size(); ++nCommandIndex )
			{
				pProperty->values.push_back( g_frameManager.GetTemplateEditorFrame()->aiscHelper.commands[nCommandIndex].szName.c_str() ) ;
			}
		}
		pProperty = AddNewProperty( this,
																"Units",
																&CAIStartCommandManipulator::SetUnitNumber,
																&CAIStartCommandManipulator::GetUnitNumber,
																SPropertyDesc::VAL_UNITS,
																SBaseProperty::LEAF );
		pProperty = AddNewProperty( this,
																"Position:x",
																&CAIStartCommandManipulator::SetVPosX,
																&CAIStartCommandManipulator::GetVPosX,
																SPropertyDesc::VAL_FLOAT,
																SBaseProperty::LEAF );
		pProperty = AddNewProperty( this,
																"Position:y",
																&CAIStartCommandManipulator::SetVPosY,
																&CAIStartCommandManipulator::GetVPosY,
																SPropertyDesc::VAL_FLOAT,
																SBaseProperty::LEAF );
/**
		{
			VARIANT variant;
			variant.vt = VT_BSTR;
			variant.bstrVal = "false";
			pProperty->values.push_back( variant ) ;
			variant.bstrVal = "true";
			pProperty->values.push_back( variant ) ;
		}
/**/
		pProperty = AddNewProperty( this,
																"Parameter",
																&CAIStartCommandManipulator::SetNumber,
																&CAIStartCommandManipulator::GetNumber,
																SPropertyDesc::VAL_FLOAT,
																SBaseProperty::LEAF );
	}
}


void CAIStartCommandManipulator::SetCmdType( const variant_t &value )
{
	pMutableObject->cmdType = static_cast<EActionCommand>( g_frameManager.GetTemplateEditorFrame()->aiscHelper.commands[CAISCHelper::DEFAULT_ACTION_COMMAND_INDEX].nID );	
	for ( int nCommandIndex = 0; nCommandIndex < g_frameManager.GetTemplateEditorFrame()->aiscHelper.commands.size(); ++nCommandIndex )
	{
		if ( value == variant_t( g_frameManager.GetTemplateEditorFrame()->aiscHelper.commands[nCommandIndex].szName.c_str() ) )
		{
			pMutableObject->cmdType = static_cast<EActionCommand>( g_frameManager.GetTemplateEditorFrame()->aiscHelper.commands[nCommandIndex].nID );	
			break;
		}
	}
}

void CAIStartCommandManipulator::GetCmdType( variant_t *pValue, int nIndex )
{
	*pValue = variant_t( g_frameManager.GetTemplateEditorFrame()->aiscHelper.commands[CAISCHelper::DEFAULT_ACTION_COMMAND_INDEX].szName.c_str() );
	for ( int nCommandIndex = 0; nCommandIndex < g_frameManager.GetTemplateEditorFrame()->aiscHelper.commands.size(); ++nCommandIndex )
	{
		if ( pMutableObject->cmdType == g_frameManager.GetTemplateEditorFrame()->aiscHelper.commands[nCommandIndex].nID )
		{
			*pValue = variant_t( g_frameManager.GetTemplateEditorFrame()->aiscHelper.commands[nCommandIndex].szName.c_str() );
			break;
		}
	}
}

void CAIStartCommandManipulator::SetUnitNumber( const variant_t &value )
{
}

void CAIStartCommandManipulator::GetUnitNumber( variant_t *pValue, int nIndex )
{
	if ( pMutableObject->pMapObjects.empty() )
	{
		*pValue = variant_t("No units");
	}
	else
	{
		int nUnitsNumber = 0;
		std::set<IRefCount*> squads;
		for ( std::list<SMapObject*>::const_iterator objectIterator = pMutableObject->pMapObjects.begin(); objectIterator != pMutableObject->pMapObjects.end(); ++objectIterator )
		{
			IRefCount* pSquad = GetSingleton<IAIEditor>()->GetFormationOfUnit( ( *objectIterator )->pAIObj ) ;
			if ( pSquad )
			{
				squads.insert( pSquad );
			}
			else
			{
				++nUnitsNumber;
			}
		}
		for( std::set< IRefCount* >::const_iterator squadIterator = squads.begin(); squadIterator != squads.end(); ++squadIterator )
		{
			++nUnitsNumber;
		}
		
		if ( nUnitsNumber == 1 )
		{
			*pValue = variant_t( NStr::Format( "%d unit", nUnitsNumber ) );
		}
		else
		{
			*pValue = variant_t( NStr::Format( "%d units", nUnitsNumber ) );
		}
	}
}

void CAIStartCommandManipulator::SetVPosX( const variant_t &value )
{
 	pMutableObject->vPos.x = float( value ) * 2 * SAIConsts::TILE_SIZE;
}

void CAIStartCommandManipulator::GetVPosX( variant_t *pValue, int nIndex )
{
	*pValue = variant_t( pMutableObject->vPos.x / ( 2 * SAIConsts::TILE_SIZE ) );
}

void CAIStartCommandManipulator::SetVPosY( const variant_t &value )
{
	pMutableObject->vPos.y = float( value ) * 2 * SAIConsts::TILE_SIZE;
}

void CAIStartCommandManipulator::GetVPosY( variant_t *pValue, int nIndex )
{
	*pValue = variant_t( pMutableObject->vPos.y / ( 2 * SAIConsts::TILE_SIZE ) );
}

void CAIStartCommandManipulator::SetNumber( const variant_t &value )
{
	pMutableObject->fNumber = float( value );
}

void CAIStartCommandManipulator::GetNumber( variant_t *pValue, int nIndex )
{
	*pValue = variant_t( pMutableObject->fNumber );
}
