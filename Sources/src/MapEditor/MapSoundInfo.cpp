#include "StdAfx.h"
#include "TemplateEditorFrame1.h"
#include "PropertieDialog.h"
#include "frames.h"

#include "MapSoundInfo.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

CPropertiesRegister thePropertiesRegister04;

const char CMSHelper::DEFAULT_SOUND_NAME[] = "sounds";

void CMSHelper::Initialize()
{
	CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>();

	int nDescCount = pODB->GetNumDescs();
	const SGDBObjectDesc *descPtr = pODB->GetAllDescs(); 
	for ( int descIndex = 0; descIndex < nDescCount; ++descIndex )
	{
		if ( descPtr[descIndex].eGameType == SGVOGT_SOUND ) 
		{
			CGDBPtr<SSoundRPGStats> pStats = NGDB::GetRPGStats<SSoundRPGStats>( descPtr[descIndex].GetName() );
			if ( pStats )
			{
				soundsList.push_back( descPtr[descIndex].GetName() );
			}
		}
	}
	isInitialized = true;
}

IManipulator* CMutableMapSoundInfo::GetManipulator()
{
	CMapSoundInfoManipulator  *pManipulator = new CMapSoundInfoManipulator();  	
	pManipulator->SetObject( this );
	return pManipulator; 
}

CMapSoundInfoManipulator::CMapSoundInfoManipulator() 
	: CManipulator( &thePropertiesRegister04, "Map Sound Info" )
{
	if ( DoWeNeedFillProps() )							
	{																				
		typedef SProperty<CMapSoundInfoManipulator> CMapSoundInfoProperty;

		CMapSoundInfoProperty *pProperty = 0;
		
		pProperty = AddNewProperty( this,
																"Name",
																&CMapSoundInfoManipulator::SetName,
																&CMapSoundInfoManipulator::GetName,
																SPropertyDesc::VAL_COMBO,
																SBaseProperty::LEAF );
		{
			for( std::list<std::string>::const_iterator soundIterator = g_frameManager.GetTemplateEditorFrame()->msHelper.soundsList.begin(); soundIterator != g_frameManager.GetTemplateEditorFrame()->msHelper.soundsList.end(); ++soundIterator )
			{
				pProperty->values.push_back( soundIterator->c_str() ) ;
			}
		} 
	}
}


void CMapSoundInfoManipulator::SetName( const variant_t &value )
{
/**
	bstr_t bstrVal = value.bstrVal;
	std::string szName = bstrVal;
	szName = szName.substr( strlen("c:\\a7\\data\\" ) );
	int pointPos = szName.rfind( '.' );
	if ( pointPos >= 0 )
	{
		szName = szName.substr( 0, pointPos );	
	}
	pMutableObject->szName = szName;
/**/

	CString szBuffer( value.bstrVal );
	pMutableObject->szName = szBuffer;
}

void CMapSoundInfoManipulator::GetName( variant_t *pValue, int nIndex )
{


	for( std::list<std::string>::const_iterator soundIterator = g_frameManager.GetTemplateEditorFrame()->msHelper.soundsList.begin(); soundIterator != g_frameManager.GetTemplateEditorFrame()->msHelper.soundsList.end(); ++soundIterator )
	{
		if ( pMutableObject->szName.compare( *soundIterator ) == 0 )
		{
			*pValue = pMutableObject->szName.c_str();
			return;
		}
	}
	pMutableObject->szName = "";
	*pValue = pMutableObject->szName.c_str();
}
