#include "stdafx.h"
#include "TemplateEditorFrame1.h"
#include "PropertieDialog.h"
#include "frames.h"

#include "UnitCreation.h"
#include "PESelectStringsDialog.h"
#include "PEPointsListDialog.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

CPropertiesRegister thePropertiesRegister02;

const char CUCHelper::DEFAULT_AVIATION_NAME[] = "aviation";
const char CUCHelper::DEFAULT_SQUADS_NAME[] = "squads";
const char CUCHelper::DEFAULT_UNITS_NAME[] = "units";

void CMutableUnitCreationInfo::MutableValidate()
{
	while ( mutableUnits.size() < ( UT_COUNT - 1 ) )
	{
		mutableUnits.push_back( SUnitCreation() );
	}
	for ( int nUnitTypeIndex = 0; nUnitTypeIndex < mutableUnits.size(); ++nUnitTypeIndex )
	{
		for ( int nAircraftIndex = SUCAviation::AT_SCOUT; nAircraftIndex < SUCAviation::AT_COUNT; ++nAircraftIndex )
		{
			if ( mutableUnits[nUnitTypeIndex].aviation.aircrafts[nAircraftIndex].szName.empty() )
			{
				mutableUnits[nUnitTypeIndex].aviation.aircrafts[nAircraftIndex].szName = DEFAULT_AIRCRAFT_NAME[nAircraftIndex];
			}
		}
		if ( mutableUnits[nUnitTypeIndex].aviation.szParadropSquadName.empty() )
		{ 
			mutableUnits[nUnitTypeIndex].aviation.szParadropSquadName = DEFAULT_PARADROP_SOLDIER_NAME;
		}
		/**
		if ( mutableUnits[nUnitTypeIndex].aviation.vAppearPoints.empty() )
		{
			mutableUnits[nUnitTypeIndex].aviation.vAppearPoints.push_back( VNULL3 );
		}
		/**/
		if ( mutableUnits[nUnitTypeIndex].aviation.nRelaxTime <= 0 )
		{
			mutableUnits[nUnitTypeIndex].aviation.nRelaxTime = DEFAULT_RELAX_TIME;
		}
		if ( mutableUnits[nUnitTypeIndex].szPartyName.empty() )
		{
			mutableUnits[nUnitTypeIndex].szPartyName = DEFAULT_PARTY_NAME;
		}
	}
}

void CUCHelper::Initialize()
{
	CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>();

	int nDescCount = pODB->GetNumDescs();
	const SGDBObjectDesc *descPtr = pODB->GetAllDescs(); 
	for ( int descIndex = 0; descIndex < nDescCount; ++descIndex )
	{
		std::vector<std::string> splitNames;
		NStr::SplitString( descPtr[descIndex].szPath, splitNames, '\\');
		
		bool bUnit = false;
		for ( int splitNameIndex = 0; splitNameIndex < splitNames.size(); ++splitNameIndex )
		{
			std::string strName = splitNames[splitNameIndex];
			NStr::ToLower( strName );
			if ( ( strName.find( DEFAULT_AVIATION_NAME ) != std::string::npos ) && ( descPtr[descIndex].eGameType == SGVOGT_UNIT ) )
			{
				aircraftsList.push_back( descPtr[descIndex].GetName() );
				break;
			}
			else if ( ( strName.find( DEFAULT_SQUADS_NAME ) != std::string::npos ) && ( descPtr[descIndex].eGameType == SGVOGT_SQUAD ) )
			{
				squadsList.push_back( descPtr[descIndex].GetName() );
				break;
			}
		}
	}
	{
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( "partys.xml" , STREAM_ACCESS_READ );
		CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
		tree.Add( "PartyInfo", &partyDependentInfo );
		for ( std::vector<CUnitCreation::SPartyDependentInfo>::const_iterator partyIterator = partyDependentInfo.begin(); partyIterator != partyDependentInfo.end(); ++partyIterator )
		{
			partyList.push_back( partyIterator->szPartyName );
		}
	}
	isInitialized = true;
}

const char* CMutableUCAviation::AIRCRAFT_TYPE_NAMES[SUCAviation::AT_COUNT] =
{
	"Scouts",
	"Fighters",
	"Paradropers",
	"Bombers",
	"Attack planes",
};
const char* CMutableUnitCreationInfo::UNIT_TYPE_NAMES[3] =
{
	"Player",
	"Enemy",
	"Neutral",
};

IManipulator* CMutableUCAircraft::GetManipulator()
{
	CUCAircraftManipulator  *pManipulator = new CUCAircraftManipulator();  	
	pManipulator->SetObject( this );
	return pManipulator; 
}

IManipulator* CMutableUCAviation::GetManipulator()
{
	CUCAviationManipulator *pManipulator = new CUCAviationManipulator();  	
	pManipulator->SetObject( this );
	return pManipulator; 
}

IManipulator* CMutableUnitCreation::GetManipulator()
{
	CUnitCreationManipulator  *pManipulator = new CUnitCreationManipulator();  	
	pManipulator->SetObject( this );
	return pManipulator; 
}

IManipulator* CMutableUnitCreationInfo::GetManipulator()
{
	CUnitCreationInfoManipulator  *pManipulator = new CUnitCreationInfoManipulator();  	
	pManipulator->SetObject( this );
	return pManipulator; 
}

CUCAircraftManipulator::CUCAircraftManipulator()
	: CManipulator( &thePropertiesRegister02, "UCAircraft" )
{
	if ( DoWeNeedFillProps() )							
	{																				
		typedef SProperty<CUCAircraftManipulator> CUCAircraftProperty;
		
		CUCAircraftProperty *pProperty = 0;

		pProperty = AddNewProperty( this,
																"Name",
																&CUCAircraftManipulator::SetName,
																&CUCAircraftManipulator::GetName,
																SPropertyDesc::VAL_COMBO,
																SBaseProperty::LEAF );
		if ( g_frameManager.GetTemplateEditorFrame() )
		{
			for( std::list<std::string>::const_iterator aircraftIterator = g_frameManager.GetTemplateEditorFrame()->ucHelper.aircraftsList.begin(); aircraftIterator != g_frameManager.GetTemplateEditorFrame()->ucHelper.aircraftsList.end(); ++aircraftIterator )
			{
				pProperty->values.push_back( aircraftIterator->c_str() ) ;
			}
		}

		pProperty = AddNewProperty( this,
																"Formation size",
																&CUCAircraftManipulator::SetFormationSize,
																&CUCAircraftManipulator::GetFormationSize,
																SPropertyDesc::VAL_INT,
																SBaseProperty::LEAF );
		pProperty = AddNewProperty( this,
																"Count",
																&CUCAircraftManipulator::SetCount,
																&CUCAircraftManipulator::GetCount,
																SPropertyDesc::VAL_INT,
																SBaseProperty::LEAF );
	}
}

CUCAviationManipulator::CUCAviationManipulator()
	: CManipulator( &thePropertiesRegister02, "UCAviation" )
{
	if ( DoWeNeedFillProps() )							
	{																				
		typedef SProperty<CUCAviationManipulator> CUCAviationProperty;
		
		CUCAviationProperty *pProperty = 0;

		pProperty = AddNewProperty( this,
																NStr::Format( "Aircrafts.%s", CMutableUCAviation::AIRCRAFT_TYPE_NAMES[SUCAviation::AT_SCOUT] ),
																CUCAviationProperty::SETFUNCTION(0),
																&CUCAviationManipulator::GetAircraft00,
																SPropertyDesc::VAL_INT,
																SBaseProperty::SINGLE );

		pProperty = AddNewProperty( this,
																NStr::Format( "Aircrafts.%s", CMutableUCAviation::AIRCRAFT_TYPE_NAMES[SUCAviation::AT_FIGHTER] ),
																CUCAviationProperty::SETFUNCTION(0),
																&CUCAviationManipulator::GetAircraft01,
																SPropertyDesc::VAL_INT,
																SBaseProperty::SINGLE );
		
		pProperty = AddNewProperty( this,
																NStr::Format( "Aircrafts.%s", CMutableUCAviation::AIRCRAFT_TYPE_NAMES[SUCAviation::AT_PARADROPER] ),
																CUCAviationProperty::SETFUNCTION(0),
																&CUCAviationManipulator::GetAircraft02,
																SPropertyDesc::VAL_INT,
																SBaseProperty::SINGLE );
		
		pProperty = AddNewProperty( this,
																NStr::Format( "Aircrafts.%s", CMutableUCAviation::AIRCRAFT_TYPE_NAMES[SUCAviation::AT_BOMBER] ),
																CUCAviationProperty::SETFUNCTION(0),
																&CUCAviationManipulator::GetAircraft03,
																SPropertyDesc::VAL_INT,
																SBaseProperty::SINGLE );

		pProperty = AddNewProperty( this,
																NStr::Format( "Aircrafts.%s", CMutableUCAviation::AIRCRAFT_TYPE_NAMES[SUCAviation::AT_BATTLEPLANE] ),
																CUCAviationProperty::SETFUNCTION(0),
																&CUCAviationManipulator::GetAircraft04,
																SPropertyDesc::VAL_INT,
																SBaseProperty::SINGLE );

		pProperty = AddNewProperty( this,
																"Paratroop squad name",
																&CUCAviationManipulator::SetParadropSquadName,
																&CUCAviationManipulator::GetParadropSquadName,
																SPropertyDesc::VAL_COMBO,
																SBaseProperty::LEAF );
		if ( g_frameManager.GetTemplateEditorFrame() )
		{
			for( std::list<std::string>::const_iterator squadsIterator = g_frameManager.GetTemplateEditorFrame()->ucHelper.squadsList.begin(); squadsIterator != g_frameManager.GetTemplateEditorFrame()->ucHelper.squadsList.end(); ++squadsIterator )
			{
				pProperty->values.push_back( squadsIterator->c_str() ) ;
			}
		}

		pProperty = AddNewProperty( this,
																"Paratroop squads count",
																&CUCAviationManipulator::SetParadropSquadCount,
																&CUCAviationManipulator::GetParadropSquadCount,
																SPropertyDesc::VAL_INT,
																SBaseProperty::LEAF );

		pProperty = AddNewProperty( this,
																"Relax time",
																&CUCAviationManipulator::SetRelaxTime,
																&CUCAviationManipulator::GetRelaxTime,
																SPropertyDesc::VAL_INT,
																SBaseProperty::LEAF );
		
		pProperty = AddNewProperty( this,
																"Appear points",
																&CUCAviationManipulator::SetAppearPoints,
																&CUCAviationManipulator::GetAppearPoints,
																SPropertyDesc::VAL_UNITS,
																SBaseProperty::LEAF );
	}
}
CUnitCreationManipulator::CUnitCreationManipulator()
	: CManipulator( &thePropertiesRegister02, "Unit Creation" )
{
	if ( DoWeNeedFillProps() )							
	{																				
		typedef SProperty<CUnitCreationManipulator> CUnitCreationProperty;
		
		CUnitCreationProperty *pProperty = 0;
		
		pProperty = AddNewProperty( this,
																"Aviation",
																CUnitCreationProperty::SETFUNCTION(0),
																&CUnitCreationManipulator::GetAviation,
																SPropertyDesc::VAL_INT,
																SBaseProperty::SINGLE );
		pProperty = AddNewProperty( this,
																"Party name",
																&CUnitCreationManipulator::SetPartyName,
																&CUnitCreationManipulator::GetPartyName,
																SPropertyDesc::VAL_COMBO,
																SBaseProperty::LEAF );
		if ( g_frameManager.GetTemplateEditorFrame() )
		{
			for( std::list<std::string>::const_iterator partyIterator = g_frameManager.GetTemplateEditorFrame()->ucHelper.partyList.begin(); partyIterator != g_frameManager.GetTemplateEditorFrame()->ucHelper.partyList.end(); ++partyIterator )
			{
				pProperty->values.push_back( partyIterator->c_str() ) ;
			}
		}
	}
}


CUnitCreationInfoManipulator::CUnitCreationInfoManipulator() 
	: CManipulator( &thePropertiesRegister02, "Unit Creation Info" )
{
	if ( DoWeNeedFillProps() )							
	{																				
		typedef SProperty<CUnitCreationInfoManipulator> CUnitCreationInfoProperty;
		
		CUnitCreationInfoProperty *pProperty = 0;
		
		pProperty = AddNewProperty( this,
																"Player",
																CUnitCreationInfoProperty::SETFUNCTION(0),
																&CUnitCreationInfoManipulator::GetUnits,
																SPropertyDesc::VAL_INT,
																SBaseProperty::VECTOR );
	}
}

void CUCAircraftManipulator::SetName( const variant_t &value )
{
	CString szBuffer( value.bstrVal );
	pMutableObject->szName = szBuffer;
}

void CUCAircraftManipulator::GetName( variant_t *pValue, int nIndex )
{
	if ( g_frameManager.GetTemplateEditorFrame() )
	{
		for( std::list<std::string>::const_iterator aircraftIterator = g_frameManager.GetTemplateEditorFrame()->ucHelper.aircraftsList.begin(); aircraftIterator != g_frameManager.GetTemplateEditorFrame()->ucHelper.aircraftsList.end(); ++aircraftIterator )
		{
			if ( pMutableObject->szName.compare( *aircraftIterator ) == 0 )
			{
				*pValue = pMutableObject->szName.c_str();
				return;
			}
		}
	}
	pMutableObject->szName = SUnitCreationInfo::DEFAULT_AIRCRAFT_NAME[SUCAviation::AT_SCOUT];
	*pValue = pMutableObject->szName.c_str();
}

void CUCAircraftManipulator::SetFormationSize( const variant_t &value )
{
	pMutableObject->nFormationSize = long( value );
}

void CUCAircraftManipulator::GetFormationSize( variant_t *pValue, int nIndex )
{
	*pValue = variant_t( long( pMutableObject->nFormationSize ) );
}

void CUCAircraftManipulator::SetCount( const variant_t &value )
{
	pMutableObject->nPlanes = long( value );
}

void CUCAircraftManipulator::GetCount( variant_t *pValue, int nIndex )
{
	*pValue = variant_t( long( pMutableObject->nPlanes ) );
}

void CUCAviationManipulator::GetAircraft00( variant_t *pValue, int nIndex )
{
	pValue->byref = pMutableObject->mutableAircrafts[SUCAviation::AT_SCOUT].GetManipulator();
	pValue->vt = VT_EMPTY;
}

void CUCAviationManipulator::GetAircraft01( variant_t *pValue, int nIndex )
{
	pValue->byref = pMutableObject->mutableAircrafts[SUCAviation::AT_FIGHTER].GetManipulator();
	pValue->vt = VT_EMPTY;
}

void CUCAviationManipulator::GetAircraft02( variant_t *pValue, int nIndex )
{
	pValue->byref = pMutableObject->mutableAircrafts[SUCAviation::AT_PARADROPER].GetManipulator();
	pValue->vt = VT_EMPTY;
}

void CUCAviationManipulator::GetAircraft03( variant_t *pValue, int nIndex )
{
	pValue->byref = pMutableObject->mutableAircrafts[SUCAviation::AT_BOMBER].GetManipulator();
	pValue->vt = VT_EMPTY;
}

void CUCAviationManipulator::GetAircraft04( variant_t *pValue, int nIndex )
{
	pValue->byref = pMutableObject->mutableAircrafts[SUCAviation::AT_BATTLEPLANE].GetManipulator();
	pValue->vt = VT_EMPTY;
}

void CUCAviationManipulator::SetParadropSquadName( const variant_t &value )
{
	CString szBuffer( value.bstrVal );
	pMutableObject->szParadropSquadName = szBuffer;	
}

void CUCAviationManipulator::GetParadropSquadName( variant_t *pValue, int nIndex )
{
	if ( g_frameManager.GetTemplateEditorFrame() )
	{
		for( std::list<std::string>::const_iterator squadsIterator = g_frameManager.GetTemplateEditorFrame()->ucHelper.squadsList.begin(); squadsIterator != g_frameManager.GetTemplateEditorFrame()->ucHelper.squadsList.end(); ++squadsIterator )
		{
			if ( pMutableObject->szParadropSquadName.compare( *squadsIterator ) == 0 )
			{
				*pValue = pMutableObject->szParadropSquadName.c_str();
				return;
			}
		}
	}
	pMutableObject->szParadropSquadName = SUnitCreationInfo::DEFAULT_PARADROP_SOLDIER_NAME;
	*pValue = pMutableObject->szParadropSquadName.c_str();
}

void CUCAviationManipulator::SetParadropSquadCount( const variant_t &value )
{
	pMutableObject->nParadropSquadCount = long( value );
}

void CUCAviationManipulator::GetParadropSquadCount( variant_t *pValue, int nIndex )
{
	*pValue = variant_t( long( pMutableObject->nParadropSquadCount ) );
}

void CUCAviationManipulator::SetRelaxTime( const variant_t &value )
{
	pMutableObject->nRelaxTime = long( value );
}

void CUCAviationManipulator::GetRelaxTime( variant_t *pValue, int nIndex )
{
	*pValue = variant_t( long( pMutableObject->nRelaxTime ) );
}

void CUCAviationManipulator::SetAppearPoints( const variant_t &value )
{
	CPEPointsListDialog dialog;
	dialog.szDialogName = "Appear Points";
	for ( std::list<CVec3>::const_iterator pointIterator = pMutableObject->vAppearPoints.begin(); pointIterator != pMutableObject->vAppearPoints.end(); ++pointIterator )
	{
		dialog.points.push_back( *pointIterator );
	}
	if (  dialog.DoModal() == IDOK )
	{
		pMutableObject->vAppearPoints.clear();
		for ( std::vector<CVec3>::const_iterator pointIterator = dialog.points.begin(); pointIterator != dialog.points.end(); ++pointIterator )
		{
			pMutableObject->vAppearPoints.push_back( *pointIterator );
		}
	}
}

void CUCAviationManipulator::GetAppearPoints( variant_t *pValue, int nIndex )
{
	if ( pMutableObject->vAppearPoints.empty() )
	{
		*pValue = variant_t("No appear points");
	}
	else if ( pMutableObject->vAppearPoints.size() < 2 )
	{
		*pValue = variant_t( NStr::Format( "%d appear point", pMutableObject->vAppearPoints.size() ) );
	}
	else
	{
		*pValue = variant_t( NStr::Format( "%d appear points", pMutableObject->vAppearPoints.size() ) );
	}
}

void CUnitCreationManipulator::GetAviation( variant_t *pValue, int nIndex )
{
	pValue->byref = pMutableObject->mutableAviation.GetManipulator();
	pValue->vt = VT_EMPTY;
}

void CUnitCreationManipulator::SetPartyName( const variant_t &value )
{
	CString szBuffer( value.bstrVal );
	pMutableObject->szPartyName = szBuffer;
	if ( g_frameManager.GetTemplateEditorFrame() )
	{
		g_frameManager.GetTemplateEditorFrame()->ResetPlayersForFlags();
	}
}

void CUnitCreationManipulator::GetPartyName( variant_t *pValue, int nIndex )
{
	if ( g_frameManager.GetTemplateEditorFrame() )
	{
		for ( std::list<std::string>::const_iterator partyIterator = g_frameManager.GetTemplateEditorFrame()->ucHelper.partyList.begin(); partyIterator != g_frameManager.GetTemplateEditorFrame()->ucHelper.partyList.end(); ++partyIterator )
		{
			if ( pMutableObject->szPartyName.compare( *partyIterator ) == 0 )
			{
				*pValue = pMutableObject->szPartyName.c_str();
				return;
			}
		}
		
		if ( !g_frameManager.GetTemplateEditorFrame()->ucHelper.partyList.empty() )
		{
			pMutableObject->szPartyName = g_frameManager.GetTemplateEditorFrame()->ucHelper.partyList.front();
		}
	}
	*pValue = pMutableObject->szPartyName.c_str();
}

void CUnitCreationInfoManipulator::GetUnits( variant_t *pValue, int nIndex )
{
	if ( nIndex < pMutableObject->mutableUnits.size() )
	{
		pValue->byref = pMutableObject->mutableUnits[nIndex].GetManipulator();
		pValue->vt = VT_EMPTY;
	}
	else
	{
		pValue->byref = 0;
		pValue->vt = VT_EMPTY;
	}
}
