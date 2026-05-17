#include "stdafx.h"
#include "editor.h"
#include "..\GFX\GFX.h"
#include <comdef.h>
#include <Mmsystem.h>
#include <set>
#include "TemplateEditorFrame1.h"
#include "SEditorMApObject.h"
#include "..\AILogic\AILogic.h"
#include "frames.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IManipulator*		SUnitEditorObjectItem::GetManipulator()
{
	CUnitManipulator  *pTmp = new CUnitManipulator;  	
	pTmp->SetObject( this );
	return pTmp; 
}

IManipulator*		STrenchEditorObjectItem::GetManipulator()
{
	CTrenchManipulator  *pTmp = new CTrenchManipulator;  	
	pTmp->SetObject( this );
	return pTmp; 
}

IManipulator*		SBuildingEditorObjectItem::GetManipulator()
{
	CBuildingManipulator  *pTmp = new CBuildingManipulator;  	
	pTmp->SetObject( this );
	return pTmp; 
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** ������������ ��� ��������� ��������
// **
// **
// **
// ************************************************************************************************************************ //



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CPropertiesRegister thePropertiesRegisterForInitObj;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ************************************************************************************************************************ //
// **
// ** ����������� ��� ������� 
// **
// ************************************************************************************************************************ //


CBuildingManipulator::CBuildingManipulator() 
: CManipulator( &thePropertiesRegisterForInitObj, "BuildingObject" )
{
	typedef SProperty<CBuildingManipulator> CBuildingProperty;
	if ( DoWeNeedFillProps() )							
	{																				
		//BEGIN_PROPERTIES_MAP( CBuilding );
		//
		AddNewProperty( this, "Units", &CBuildingManipulator::SetUnitNumber, &CBuildingManipulator::GetUnitNumber, SPropertyDesc::VAL_UNITS, SBaseProperty::LEAF );
		AddNewProperty( this, "Script ID", &CBuildingManipulator::SetScriptID, &CBuildingManipulator::GetScriptID, SPropertyDesc::VAL_INT, SBaseProperty::LEAF );
		CBuildingProperty *pProp = AddNewProperty( this, "Player", &CBuildingManipulator::SetPlayer, &CBuildingManipulator::GetPlayer, SPropertyDesc::VAL_COMBO, SBaseProperty::LEAF );
		{
			if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
			{
				for ( int nPlayerIndex = 0; nPlayerIndex < pFrame->currentMapInfo.diplomacies.size(); ++nPlayerIndex )
				{
					pProp->values.push_back( NStr::Format ( "%d", nPlayerIndex ) );
				}
			}
		}
		AddNewProperty( this, "Health", &CBuildingManipulator::SetHealth, &CBuildingManipulator::GetHealth, SPropertyDesc::VAL_FLOAT, SBaseProperty::LEAF );
		//
		//END_PROPERTIES_MAP;
	}
	else
	{
		if ( CBuildingProperty *pProp = dynamic_cast<CBuildingProperty*>( GetProperty( "Player" )  ) )
		{
			pProp->values.clear();
			if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
			{
				for ( int nPlayerIndex = 0; nPlayerIndex < pFrame->currentMapInfo.diplomacies.size(); ++nPlayerIndex )
				{
					pProp->values.push_back( NStr::Format ( "%d", nPlayerIndex ) );
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingManipulator::SetUnitNumber( const variant_t &value )		
{
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( m_obj->pObj->pAIObj )
	{
		CTemplateEditorFrame *ptr = g_frameManager.GetTemplateEditorFrame();
		ptr->PopFromBuilding( m_obj );
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingManipulator::GetUnitNumber( variant_t *pValue, int )		
{
	pValue->vt = VT_BSTR;
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );

	CTemplateEditorFrame *ptr = g_frameManager.GetTemplateEditorFrame();
	int n = ptr->GetNumSoldiersInBuilding( m_obj );
	if ( n > 0 )
	{
		_bstr_t tmpVal;
		if ( n == 1 )
		{
			tmpVal = NStr::Format( "%d unit linked, ( DblClick for unlink unit.)", n );
		}
		else
		{
			tmpVal = NStr::Format( "%d units linked, ( DblClick for unlink units.)", n );
		}
		pValue->bstrVal = tmpVal; 												
	}
	else
	{
		_bstr_t tmpVal = "0 units linked";
		pValue->bstrVal = tmpVal; 												
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CBuildingManipulator::SetScriptID( const variant_t &value )		
{
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( m_obj->pObj->pAIObj )
	{
		 m_obj->nScriptID = value.intVal;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingManipulator::GetScriptID( variant_t *pValue, int )		
{
	pValue->vt = VT_INT;
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( m_obj->pObj->pVisObj )
	{
		pValue->intVal = m_obj->nScriptID; 												
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingManipulator::SetPlayer( const variant_t &value )		
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		for ( int nPlayerIndex = 0; nPlayerIndex < pFrame->currentMapInfo.diplomacies.size(); ++nPlayerIndex )
		{
			if ( value == variant_t( NStr::Format( "%d", nPlayerIndex ) ) )
			{
				m_obj->nPlayer = nPlayerIndex;
				if ( m_obj->pObj && m_obj->sDesc.eGameType == SGVOGT_BUILDING )
				{
					GetSingleton<IAIEditor>()->SetPlayer( m_obj->pObj->pAIObj, nPlayerIndex );
				}

				bool bNeedUpdateStorages = false;
				if ( m_obj->pObj->pDesc->eGameType == SGVOGT_BUILDING )
				{
					CGDBPtr<SBuildingRPGStats> pStats = dynamic_cast<const SBuildingRPGStats*>( GetSingleton<IObjectsDB>()->GetRPGStats( m_obj->pObj->pDesc ) );
					if ( ( pStats->eType == SBuildingRPGStats::TYPE_MAIN_RU_STORAGE ) ||
							 ( pStats->eType == SBuildingRPGStats::TYPE_TEMP_RU_STORAGE ) )
					{
						bNeedUpdateStorages = true;
					}
				}
				if ( bNeedUpdateStorages )
				{
					pFrame->ShowStorageCoverage();
				}
				else if ( g_frameManager.GetMiniMapWindow() )
				{
					g_frameManager.GetMiniMapWindow()->UpdateMinimap( true );
				}
				return;
			}
		}
		m_obj->nPlayer = 0;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingManipulator::GetPlayer( variant_t *pValue, int )		
{
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		if ( m_obj->pObj->pVisObj )
		{
			*pValue = variant_t( NStr::Format( "%d", m_obj->nPlayer ) );
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingManipulator::SetHealth( const variant_t &value )
{
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( m_obj->pObj->pAIObj )
	{
		const float fRealValue = value.fltVal / 100.0f;
		const float fValue = Clamp( fRealValue, 0.0f, 1.0f );

		float fDamageValue = ( m_obj->pObj->fHP - fValue ) * m_obj->pObj->pRPG->fMaxHP;
		GetSingleton<IAIEditor>()->DamageObject( m_obj->pObj->pAIObj, fDamageValue );
		
		IGameTimer *pTimer = GetSingleton<IGameTimer>();
		int time = pTimer->GetGameTime( );
		
		CTemplateEditorFrame *ptr = g_frameManager.GetTemplateEditorFrame();
		ptr->Update( time );
		ptr->RedrawWindow();	
	}
}

void CBuildingManipulator::GetHealth( variant_t *pValue, int nIndex )
{
	pValue->vt = VT_R4;
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( m_obj->pObj->pVisObj )
	{
		pValue->fltVal = m_obj->pObj->fHP * 100.0f;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** ����������� ��� ������ 
// **
// ************************************************************************************************************************ //
CTrenchManipulator::CTrenchManipulator() 
: CManipulator( &thePropertiesRegisterForInitObj, "TrenchObject" )
{
	typedef SProperty<CTrenchManipulator> CTrenchProperty;
	if ( DoWeNeedFillProps() )							
	{																				
		//BEGIN_PROPERTIES_MAP( CTrench );
		//
		AddNewProperty( this, "Units", &CTrenchManipulator::SetUnitNumber, &CTrenchManipulator::GetUnitNumber, SPropertyDesc::VAL_UNITS, SBaseProperty::LEAF );
		AddNewProperty( this, "Script ID", &CTrenchManipulator::SetScriptID, &CTrenchManipulator::GetScriptID, SPropertyDesc::VAL_INT, SBaseProperty::LEAF );
		//
		//END_PROPERTIES_MAP;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTrenchManipulator::SetUnitNumber( const variant_t &value )		
{
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( m_obj->pObj->pAIObj )
	{
		CTemplateEditorFrame *ptr = g_frameManager.GetTemplateEditorFrame();
		ptr->PopFromBuilding( m_obj );
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTrenchManipulator::GetUnitNumber( variant_t *pValue, int )		
{
	pValue->vt = VT_BSTR;
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );

	CTemplateEditorFrame *ptr = g_frameManager.GetTemplateEditorFrame();
	int n = ptr->GetNumSoldiersInBuilding( m_obj );
	if ( n > 0 )
	{
		_bstr_t tmpVal;
		if ( n == 1 )
		{
			tmpVal = NStr::Format( "%d unit linked, ( DblClick for unlink unit.)", n );
		}
		else
		{
			tmpVal = NStr::Format( "%d units linked, ( DblClick for unlink units.)", n );
		}
		pValue->bstrVal = tmpVal; 												
	}
	else
	{
		_bstr_t tmpVal = "0 units linked";
		pValue->bstrVal = tmpVal; 												
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTrenchManipulator::SetScriptID( const variant_t &value )		
{
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( m_obj->pObj->pAIObj )
	{
		 m_obj->nScriptID = value.intVal;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTrenchManipulator::GetScriptID( variant_t *pValue, int )		
{
	pValue->vt = VT_INT;
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( m_obj->pObj->pVisObj )
	{
		pValue->intVal = m_obj->nScriptID; 												
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ************************************************************************************************************************ //
// **
// ** ����������� ��� c��������� 
// **
// ************************************************************************************************************************ //

const std::string CUnitManipulator::FORMATIONS_LABELS[5] =
{
	"DEFAULT",
	"MOVEMENT",
	"DEFENSIVE",
	"OFFENSIVE",
	"SNEAK",
};

CUnitManipulator::CUnitManipulator() 
: CManipulator( &thePropertiesRegisterForInitObj, "UnitObject" )
{
		typedef SProperty<CUnitManipulator> CUnitProperty;
	if ( DoWeNeedFillProps() )							
	{																				
		//BEGIN_PROPERTIES_MAP( CUnit );
		//
		AddNewProperty( this, "Units", &CUnitManipulator::SetUnitNumber, &CUnitManipulator::GetUnitNumber, SPropertyDesc::VAL_UNITS, SBaseProperty::LEAF );
		AddNewProperty( this, "Angle", &CUnitManipulator::SetAngle, &CUnitManipulator::GetAngle, SPropertyDesc::VAL_INT, SBaseProperty::LEAF );
		AddNewProperty( this, "Script ID", &CUnitManipulator::SetScriptID, &CUnitManipulator::GetScriptID, SPropertyDesc::VAL_INT, SBaseProperty::LEAF );
		{
			CUnitProperty *pProp = AddNewProperty( this, "ScenarioUnit", &CUnitManipulator::SetScenarioUnit, &CUnitManipulator::GetScenarioUnit, SPropertyDesc::VAL_COMBO, SBaseProperty::LEAF );
			pProp->values.push_back( "TRUE" ) ;
			pProp->values.push_back( "FALSE" ) ;
		}
		{
			CUnitProperty *pProp = AddNewProperty( this, "Player", &CUnitManipulator::SetPlayer, &CUnitManipulator::GetPlayer, SPropertyDesc::VAL_COMBO, SBaseProperty::LEAF );
			if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
			{
				for ( int nPlayerIndex = 0; nPlayerIndex < pFrame->currentMapInfo.diplomacies.size(); ++nPlayerIndex )
				{
					pProp->values.push_back( NStr::Format ( "%d", nPlayerIndex ) );
				}
			}
		}
		AddNewProperty( this, "Health", &CUnitManipulator::SetHealth, &CUnitManipulator::GetHealth, SPropertyDesc::VAL_FLOAT, SBaseProperty::LEAF );
		{
			CUnitProperty *pProp = AddNewProperty( this, "Formation", &CUnitManipulator::SetFrameIndex, &CUnitManipulator::GetFrameIndex, SPropertyDesc::VAL_COMBO, SBaseProperty::LEAF );
			
			for ( int nFormationIndex = 0; nFormationIndex < 5; ++nFormationIndex )
			{
				pProp->values.push_back( FORMATIONS_LABELS[nFormationIndex].c_str() );
			}
		}
		//
		//END_PROPERTIES_MAP;
	}
	else
	{
		if ( CUnitProperty *pProp = dynamic_cast<CUnitProperty*>( GetProperty( "Player" ) ) )
		{
			pProp->values.clear();
			if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
			{
				for ( int nPlayerIndex = 0; nPlayerIndex < pFrame->currentMapInfo.diplomacies.size(); ++nPlayerIndex )
				{
					pProp->values.push_back( NStr::Format ( "%d", nPlayerIndex ) );
				}
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitManipulator::SetUnitNumber( const variant_t &value )
{
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( m_obj->pObj->pAIObj )
	{
		CTemplateEditorFrame *ptr = g_frameManager.GetTemplateEditorFrame();
		ptr->PopFromBuilding( m_obj );
	}
}

void CUnitManipulator::GetUnitNumber( variant_t *pValue, int nIndex )
{
	pValue->vt = VT_BSTR;
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );

	CTemplateEditorFrame *ptr = g_frameManager.GetTemplateEditorFrame();
	int n = ptr->GetNumSoldiersInBuilding( m_obj );
	if ( n > 0 )
	{
		_bstr_t tmpVal;
		if ( n == 1 )
		{
			tmpVal = NStr::Format( "%d unit linked, ( DblClick for unlink unit.)", n );
		}
		else
		{
			tmpVal = NStr::Format( "%d units linked, ( DblClick for unlink units.)", n );
		}
		pValue->bstrVal = tmpVal; 												
	}
	else
	{
		_bstr_t tmpVal = "0 units linked";
		pValue->bstrVal = tmpVal; 												
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitManipulator::SetAngle( const variant_t &value )		
{
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( m_obj->pObj->pAIObj )
	{
		int newVal = ( ( value.intVal * 65536.0f ) / 360.0f + 0.5f );
		GetSingleton<IAIEditor>()->TurnObject( m_obj->pObj->pAIObj, newVal );
		IGameTimer *pTimer = GetSingleton<IGameTimer>();
		pTimer->Update( timeGetTime() );
		//frame->Update( pTimer->GetGameTime() );
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitManipulator::GetAngle( variant_t *pValue, int )		
{
	pValue->vt = VT_INT;
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( m_obj->pObj->pVisObj )
	{
		pValue->intVal = ( m_obj->pObj->pVisObj->GetDirection() * 360.0f / 65536.0f + 0.5f ); 												
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitManipulator::SetPlayer( const variant_t &value )		
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		for ( int nPlayerIndex = 0; nPlayerIndex < pFrame->currentMapInfo.diplomacies.size(); ++nPlayerIndex )
		{
			if ( value == variant_t( NStr::Format( "%d", nPlayerIndex ) ) )
			{
				m_obj->nPlayer = nPlayerIndex;
				if ( m_obj->pObj && m_obj->pObj->pDesc->eGameType == SGVOGT_BUILDING )
				{
					GetSingleton<IAIEditor>()->SetPlayer( m_obj->pObj->pAIObj, nPlayerIndex );
				}
				else if ( m_obj->pObj && m_obj->pObj->pDesc->eGameType == SGVOGT_FLAG )
				{
					if ( !pFrame->ucHelper.IsInitialized() )
					{
						pFrame->ucHelper.Initialize();	
					}

					const std::string szFlagPrefix = "Flag_";
					const std::string szPartyNeutral = "neutral";
					std::string szLocalGeneralPartyName;
					if ( ( nPlayerIndex >= 0 ) && ( nPlayerIndex < pFrame->m_unitCreationInfo.mutableUnits.size() ) )
					{
						const std::string szPartyName = pFrame->m_unitCreationInfo.mutableUnits[nPlayerIndex].szPartyName;
						for ( int nPartyIndex = 0; nPartyIndex < pFrame->ucHelper.partyDependentInfo.size(); ++nPartyIndex )
						{
							if ( pFrame->ucHelper.partyDependentInfo[nPartyIndex].szPartyName == szPartyName )
							{
								szLocalGeneralPartyName = pFrame->ucHelper.partyDependentInfo[nPartyIndex].szGeneralPartyName;
								NStr::ToLower( szLocalGeneralPartyName );
								break;
							}
						}
					}
					if ( szLocalGeneralPartyName.empty() )
					{
						szLocalGeneralPartyName = szPartyNeutral;	
					}
					const std::string szFlagName = szFlagPrefix + szLocalGeneralPartyName;
					
					if ( szFlagName != m_obj->pObj->pDesc->szKey )
					{
						SMapObjectInfo mapObjectInfo;
						mapObjectInfo.szName = szFlagName;
						
						WORD wDir = 0;
						CVec3 vPos = VNULL3;
						m_obj->pObj->GetPlacement( &vPos, &wDir );
						mapObjectInfo.vPos = vPos;
						mapObjectInfo.nDir = wDir;
						Vis2AI( &( mapObjectInfo.vPos ) );

						mapObjectInfo.nPlayer = m_obj->nPlayer;
						mapObjectInfo.nScriptID = m_obj->nScriptID;
						mapObjectInfo.fHP = m_obj->pObj->fHP;

						SMapObject *pOldObj = m_obj->pObj;
						SMapObject *pNewObj = pFrame->AddObjectByAI( mapObjectInfo );
						//�������� selection ( ���� ����, � �� ���� )
						if ( pFrame->m_currentMovingObjectPtrAI == pOldObj )
						{
							pFrame->m_currentMovingObjectPtrAI = pNewObj;
						}
						else
						{
							for ( int nObjectIndex = 0; nObjectIndex < pFrame->m_currentMovingObjectsAI.size(); ++nObjectIndex )
							{
								if ( pFrame->m_currentMovingObjectsAI[nObjectIndex] == pOldObj )
								{
									pFrame->m_currentMovingObjectsAI[nObjectIndex] = pNewObj;
									break;
								}
							}
						}
						m_obj = pFrame->m_objectsAI[pNewObj];
						pFrame->RemoveObject( pOldObj );
					}	
				}
				
				if ( g_frameManager.GetMiniMapWindow() )
				{
					g_frameManager.GetMiniMapWindow()->UpdateMinimap( true );
				}
				return;
			}
		}
		m_obj->nPlayer = 0;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitManipulator::GetPlayer( variant_t *pValue, int )		
{
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		if ( m_obj->pObj->pVisObj )
		{
			*pValue = variant_t( NStr::Format( "%d", m_obj->nPlayer ) );
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitManipulator::SetScriptID( const variant_t &value )		
{
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( m_obj->pObj->pAIObj )
	{
		 m_obj->nScriptID = value.intVal;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitManipulator::GetScriptID( variant_t *pValue, int )		
{
	pValue->vt = VT_INT;
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( m_obj->pObj->pVisObj )
	{
		pValue->intVal = m_obj->nScriptID; 												
	}

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitManipulator::SetFrameIndex( const variant_t &value )
{
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( IObjectsDB *pIDB = GetSingleton<IObjectsDB>() )
	{
		if ( IAIEditor *pAIEditor = GetSingleton<IAIEditor>() )
		{
			if ( m_obj->pObj->pAIObj )
			{
				if ( IRefCount *pAIObject = pAIEditor->GetFormationOfUnit( m_obj->pObj->pAIObj ) )
				{
					if ( SMapObject *pMapObj = g_frameManager.GetTemplateEditorFrame()->FindByAI( pAIObject ) )
					{
						if ( const SSquadRPGStats *pSquadRPGStats = NGDB::GetRPGStats<SSquadRPGStats>( pIDB, pMapObj->pDesc ) )
						{
							for ( int nSquadFormationIndex = 0; nSquadFormationIndex < pSquadRPGStats->formations.size(); ++nSquadFormationIndex )
							{
								if ( value == variant_t( FORMATIONS_LABELS[pSquadRPGStats->formations[nSquadFormationIndex].type].c_str() ) )
								{
									m_obj->frameIndex = nSquadFormationIndex;
									break;
								}
							}
						}
					}
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitManipulator::GetFrameIndex( variant_t *pValue, int nIndex )
{
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	*pValue = variant_t( FORMATIONS_LABELS[0].c_str() );
	if ( IObjectsDB *pIDB = GetSingleton<IObjectsDB>() )
	{
		if ( IAIEditor *pAIEditor = GetSingleton<IAIEditor>() )
		{
			if ( m_obj->pObj->pAIObj )
			{
				if ( IRefCount *pAIObject = pAIEditor->GetFormationOfUnit( m_obj->pObj->pAIObj ) )
				{
					if ( SMapObject *pMapObj = g_frameManager.GetTemplateEditorFrame()->FindByAI( pAIObject ) )
					{
						if ( const SSquadRPGStats *pSquadRPGStats = NGDB::GetRPGStats<SSquadRPGStats>( pIDB, pMapObj->pDesc ) )
						{
							if ( ( m_obj->frameIndex >= 0 ) && ( m_obj->frameIndex < pSquadRPGStats->formations.size() ) )
							{
								*pValue = variant_t( FORMATIONS_LABELS[pSquadRPGStats->formations[m_obj->frameIndex].type].c_str() );
							}
						}
					}
				}
			}
		}
	}
}

void CUnitManipulator::SetScenarioUnit( const variant_t &value )		
{
	if ( value == variant_t( "TRUE" ) || value == variant_t( "FALSE" ) )
	{
		NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
		if ( m_obj->pObj->pAIObj )
		{
			m_obj->bScenarioUnit = ( value == variant_t( "TRUE" ) );
			if ( m_obj->bScenarioUnit )
			{
				m_obj->pObj->pVisObj->SetSpecular( 0xFF0000FF );
			}
			else
			{
				m_obj->pObj->pVisObj->SetSpecular( 0x00000000 );
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitManipulator::GetScenarioUnit( variant_t *pValue, int )		
{
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( m_obj->pObj->pVisObj )
	{
		if ( m_obj->bScenarioUnit )
		{
			*pValue = variant_t( "TRUE" );
		}
		else
		{
			*pValue = variant_t( "FALSE" );
		}
	}
}
/**/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitManipulator::SetTestString( const variant_t &value )		
{
	testString = CString( value.bstrVal );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitManipulator::GetTestString( variant_t *pValue, int )		
{    
	pValue->vt = VT_BSTR;
	_bstr_t tmpVal = testString.c_str();
	pValue->bstrVal = tmpVal;
//	CVariant v =  CString( testString.c_str() );
//	pValue->pbstrVal = v.;
}

void CUnitManipulator::SetHealth( const variant_t &value )
{
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( m_obj->pObj->pAIObj )
	{
		const float fRealValue = value.fltVal / 100.0f;
		const float fValue = Clamp( fRealValue, 0.01f, 1.0f );

		float fDamageValue = ( m_obj->pObj->fHP - fValue ) * m_obj->pObj->pRPG->fMaxHP;
		GetSingleton<IAIEditor>()->DamageObject( m_obj->pObj->pAIObj, fDamageValue );
		
		IGameTimer *pTimer = GetSingleton<IGameTimer>();
		int time = pTimer->GetGameTime( );
		
		CTemplateEditorFrame *ptr = g_frameManager.GetTemplateEditorFrame();
		ptr->Update( time );
		ptr->RedrawWindow();	
	}
}

void CUnitManipulator::GetHealth( variant_t *pValue, int nIndex )
{
	pValue->vt = VT_R4;
	NI_ASSERT_T( m_obj->pObj != 0, "object is empty" );
	if ( m_obj->pObj->pVisObj )
	{
		pValue->fltVal = m_obj->pObj->fHP * 100.0f;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** multi-unit manipulator
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
CMultiUnitManipulator::CMultiUnitManipulator() 
: CManipulator( &thePropertiesRegisterForInitObj, "MultiUnitObject" )
{
	BEGIN_PROPERTIES_MAP( CMultiUnit );
	//
	AddNewProperty( this, "Angle", &CMultiUnitManipulator::SetAngle, &CMultiUnitManipulator::GetAngle, SPropertyDesc::VAL_INT, SBaseProperty::LEAF );
	AddNewProperty( this, "Script ID", &CMultiUnitManipulator::SetScriptID, &CMultiUnitManipulator::GetScriptID, SPropertyDesc::VAL_INT, SBaseProperty::LEAF );
	CMultiUnitProperty *pProp = AddNewProperty( this, "Player", &CMultiUnitManipulator::SetPlayer, &CMultiUnitManipulator::GetPlayer, SPropertyDesc::VAL_COMBO, SBaseProperty::LEAF );
	{
		VARIANT v;
		v.intVal = 0;
		v.vt = VT_INT;
		pProp->values.push_back( v ) ;
		v.intVal = 1;
		v.vt = VT_INT;
		pProp->values.push_back( v ) ;
		v.intVal = 2;
		v.vt = VT_INT;
		pProp->values.push_back( v ) ;
	}
		//SUnitsLogics logic;
	CMultiUnitProperty *pProp2 = AddNewProperty( this, "Behavior", &CMultiUnitManipulator::SetBehavior, &CMultiUnitManipulator::GetBehavior, SPropertyDesc::VAL_COMBO, SBaseProperty::LEAF );
	{
		for(	std::unordered_map<std::string, EActionCommand>::iterator it = logic.logics.begin(); it !=  logic.logics.end(); ++it )
			pProp2->values.push_back( it->first.c_str() ) ;
		pProp2->values.push_back( "normal" ); 
	}
	//
	END_PROPERTIES_MAP;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiUnitManipulator::SetAngle( const variant_t &value )		
{
	for ( std::vector<SEditorObjectItem*>::iterator it = m_objects.begin(); it != m_objects.end(); ++it )
	{
		NI_ASSERT_T( (*it)->pObj != 0, "object is empty" );
		if ( (*it)->pObj->pAIObj )
		{
			int  newVal = ( value.intVal * 65536 ) / 360.0f ;
			GetSingleton<IAIEditor>()->TurnObject
			( (*it)->pObj->pAIObj, newVal );
			IGameTimer *pTimer = GetSingleton<IGameTimer>();
			pTimer->Update( timeGetTime() );
			//frame->Update( pTimer->GetGameTime() );
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiUnitManipulator::GetAngle( variant_t *pValue, int )		
{
	std::set<int> vals;
	for ( std::vector<SEditorObjectItem*>::iterator it = m_objects.begin(); it != m_objects.end(); ++it )
	{
		vals.insert( (*it)->pObj->pVisObj->GetDirection() );
	}
	if ( vals.size() == 1 )
	{
		pValue->vt = VT_INT;
		pValue->intVal = ( m_objects[0]->pObj->pVisObj->GetDirection() * 360 ) / 65536.0f; 												
	}
	else
	{
		pValue->vt = VT_BSTR;
		_bstr_t tmpVal = "...";
		pValue->bstrVal = tmpVal;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiUnitManipulator::SetPlayer( const variant_t &value )		
{
	for ( std::vector<SEditorObjectItem*>::iterator it = m_objects.begin(); it != m_objects.end(); ++it )
	{
		NI_ASSERT_T( (*it)->pObj != 0, "object is empty" );
		if ( (*it)->pObj->pAIObj )
		{
		 (*it)->nPlayer = value.intVal;
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiUnitManipulator::GetPlayer( variant_t *pValue, int )		
{
	std::set<int> vals;
	for ( std::vector<SEditorObjectItem*>::iterator it = m_objects.begin(); it != m_objects.end(); ++it )
	{
		vals.insert( (*it)->nPlayer );
	}
	if ( vals.size() == 1 )
	{
		pValue->vt = VT_INT;
		pValue->intVal = m_objects[0]->nPlayer; 												
	}
	else
	{
		pValue->vt = VT_BSTR;
		_bstr_t tmpVal = "...";
		pValue->bstrVal = tmpVal;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiUnitManipulator::SetScriptID( const variant_t &value )		
{
	for ( std::vector<SEditorObjectItem*>::iterator it = m_objects.begin(); it != m_objects.end(); ++it )
	{
		NI_ASSERT_T( (*it)->pObj != 0, "object is empty" );
		if ( (*it)->pObj->pAIObj )
		{
			 (*it)->nScriptID = value.intVal;
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiUnitManipulator::GetScriptID( variant_t *pValue, int )		
{
	std::set<int> vals;
	for ( std::vector<SEditorObjectItem*>::iterator it = m_objects.begin(); it != m_objects.end(); ++it )
	{
		vals.insert( (*it)->nScriptID );
	}
	if ( vals.size() == 1 )
	{
		pValue->vt = VT_INT;
		pValue->intVal = m_objects[0]->nScriptID; 												
	}
	else
	{
		pValue->vt = VT_BSTR;
		_bstr_t tmpVal = "...";
		pValue->bstrVal = tmpVal;
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiUnitManipulator::SetBehavior( const variant_t &value )		
{
	for ( std::vector<SEditorObjectItem*>::iterator it = m_objects.begin(); it != m_objects.end(); ++it )
	{
		NI_ASSERT_T( (*it)->pObj != 0, "object is empty" );
		if ( (*it)->pObj->pAIObj )
		{
			CString tmp = CString ( value.bstrVal );
			if( tmp == "normal" )
				(*it)->szBehavior = "";
			else
				 (*it)->szBehavior =tmp;
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiUnitManipulator::GetBehavior( variant_t *pValue, int )		
{
	std::set<std::string> vals;
	for ( std::vector<SEditorObjectItem*>::iterator it = m_objects.begin(); it != m_objects.end(); ++it )
	{
		vals.insert( (*it)->szBehavior );
	}
	if ( vals.size() == 1 )
	{
			if( m_objects[0]->szBehavior == "" )	
		{
			_bstr_t tmpVal = "normal";
			pValue->vt = VT_BSTR;
			pValue->bstrVal = tmpVal ; 										
		}
		else
		{
			_bstr_t tmpVal =  m_objects[0]->szBehavior.c_str();
			pValue->vt = VT_BSTR;
			pValue->bstrVal = tmpVal ; 										
		}
	}
	else
	{
		pValue->vt = VT_BSTR;
		_bstr_t tmpVal = "...";
		pValue->bstrVal = tmpVal;
	}
}
/**/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
