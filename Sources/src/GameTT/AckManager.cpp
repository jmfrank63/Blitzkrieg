#include "StdAfx.h"

#include "AckManager.h"
#include "..\Common\Actions.h"
#include "..\Scene\Scene.h"
#include "..\Common\MapObject.h"
#include "..\Main\TextSystem.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS(CClientAckManager);
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CClientAckManager::MIN_UNITS_TO_RUSH_ACK;
int CClientAckManager::MIN_UNITS_TO_TRAVEL_ACK;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*		 								   CBoredUnitsContainer																		*
//*******************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CClientAckManager::CBoredUnitsContainer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &boredUnits );							
	saver.Add( 2, &nCounter );
	saver.Add( 3, &timeLastBored );
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientAckManager::CBoredUnitsContainer::Clear()
{
	boredUnits.clear();
	nCounter = 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CClientAckManager::CBoredUnitsContainer::CBoredUnitsContainer() 
: nCounter( 0 ), timeLastBored ( 0 )
{  
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientAckManager::CBoredUnitsContainer::Copy( const CClientAckManager::CBoredUnitsContainer &cp )
{
	nCounter = cp.nCounter;
	boredUnits = cp.boredUnits;
	timeLastBored = cp.timeLastBored;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientAckManager::CBoredUnitsContainer::AddUnit( interface IMOUnit *pUnit )
{
	CBoredUnits::iterator it = boredUnits.find( pUnit );
	if ( it == boredUnits.end() )
	{
		boredUnits[pUnit] = true;
		++nCounter;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientAckManager::CBoredUnitsContainer::DelUnit( interface IMOUnit *pUnit )
{
	CBoredUnits::iterator it = boredUnits.find( pUnit );
	if ( it != boredUnits.end() )
	{
		boredUnits.erase( it );
		--nCounter;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CClientAckManager::CBoredUnitsContainer::SendAck( const NTimer::STime curTime, 
																											 const EUnitAckType eBored, 
																											 IClientAckManager *pAckManager,
																											 const NTimer::STime timeInterval )
{
	NI_ASSERT_T( nCounter >= 0, "wrong counter" );
	if ( timeLastBored == 0 )
		timeLastBored = curTime + int (timeInterval * ( 1.0f + 1.0f * rand() / RAND_MAX ) );

	if ( nCounter == 0 || timeLastBored > curTime )
	{
		return false;
	}
	else 
	{
		bool bSayAck = true;
		switch ( eBored )
		{
		case ACK_BORED_RUSH:
			bSayAck = nCounter > CClientAckManager::MIN_UNITS_TO_RUSH_ACK;
			break;
		case ACK_BORED_INFANTRY_TRAVEL:
			bSayAck = nCounter > CClientAckManager::MIN_UNITS_TO_TRAVEL_ACK;
			break;
		}
		if ( bSayAck )
		{
			NI_ASSERT_T( !boredUnits.empty(), "list is empty");
			CPtr<IMOUnit> pUnit ;
			pUnit = (*boredUnits.begin()).first;
			while( !pUnit.IsValid() && nCounter != 0 )
			{
				DelUnit( pUnit );
				pUnit = 0;
				if ( nCounter )
					pUnit = (*boredUnits.begin()).first;	
			}
			if ( pUnit.IsValid() )
				pUnit->SendAcknowledgement( pAckManager, eBored, 0 );

			timeLastBored = curTime + int (timeInterval * ( 1.0f + 1.0f * rand() / RAND_MAX ) );
		}
		return bSayAck;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// ** particular actions
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CClientAckManager::operator& (IStructureSaver &ss) 
{
	CSaverAccessor saver = &ss;
	if ( saver.IsReading() )
	{
		Init();
	}
	saver.Add( 1, &unitAcks );
	saver.Add( 3, &pLastSelected );
	saver.Add( 4, &nSelectionCounter );
	saver.Add( 5, &boredUnits );	
	//saver.Add( 6, &timeNextBored );
	saver.Add( 7, &deathAcks );
	saver.Add( 8, &timeLastDeath );	
	saver.Add( 9, &acksPresence );
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CClientAckManager::SDeathAck::operator& (IStructureSaver &ss) 
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szSoundName );
	saver.Add( 2, &vPos );
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CClientAckManager::SUnitAck::operator& (IStructureSaver &ss) 
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &acks );
	saver.Add( 2, &wSoundID );
	saver.Add( 3, &timeRun );
	saver.Add( 4, &eCurrentAck );
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CClientAckManager::SAck::operator& (IStructureSaver &ss) 
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &eMixType );
	saver.Add( 2, &sound );
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CClientAckManager::IsNegative( const enum EUnitAckType eAck )
{
	return acksInfo[eAck].eType == ACKT_NEGATIVE;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientAckManager::Clear()
{
	unitAcks.clear();
	boredUnits.clear();
}
CClientAckManager::CClientAckManager() 
: MIN_ACK_RADIUS( 0 ), 
	MAX_ACK_RADIUS( 0 ), 
	TIME_ACK_WAIT( 0 ), 
	nSelectionCounter( 0 )/*,
	timeNextBored( 0 )*/
{  
}

#define STRING_ENUM_ADD(TypeConverter,eEnum) TypeConverter[#eEnum] = eEnum;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientAckManager::InitConsts()
{
	CTableAccessor constsTbl = NDB::OpenDataTable( "consts.xml" );
	MIN_ACK_RADIUS = constsTbl.GetInt( "Scene", "Sound.Acknowledgements.MinRadius", 15 );
	MAX_ACK_RADIUS = constsTbl.GetInt( "Scene", "Sound.Acknowledgements.MaxRadius", 30 );
	TIME_ACK_WAIT = constsTbl.GetInt( "Scene", "Sound.Acknowledgements.WaitForCancel", 500 );
	NUM_SELECTIONS_BEFORE_F_OFF = constsTbl.GetInt( "Scene", "Sound.Acknowledgements.SelectionsBoredCounter", 3 );
	MIN_UNITS_TO_RUSH_ACK = constsTbl.GetInt( "Scene", "Sound.Acknowledgements.MinUnitsToRush", 10 );
	MIN_UNITS_TO_TRAVEL_ACK = constsTbl.GetInt( "Scene", "Sound.Acknowledgements.MinUnitsToTravel", 10 );

	ACK_BORED_INTERVAL = constsTbl.GetInt( "Scene", "Sound.Acknowledgements.BoredInterval", 5000 );
	ACK_BORED_INTERVAL_RANDOM = constsTbl.GetInt( "Scene", "Sound.Acknowledgements.BoredIntervalRandom", 5000 );


	STRING_ENUM_ADD( loadHelper, ACK_POSITIVE )
	STRING_ENUM_ADD( loadHelper, ACK_NEGATIVE )
	STRING_ENUM_ADD( loadHelper, ACK_SELECTED )
	STRING_ENUM_ADD( loadHelper, ACK_INVALID_TARGET)
	STRING_ENUM_ADD( loadHelper, ACK_DONT_SEE_THE_ENEMY)
	STRING_ENUM_ADD( loadHelper, ACK_NOT_IN_ATTACK_ANGLE)
	STRING_ENUM_ADD( loadHelper, ACK_NOT_IN_FIRE_RANGE)
	STRING_ENUM_ADD( loadHelper, ACK_ENEMY_IS_TO_CLOSE)
	STRING_ENUM_ADD( loadHelper, ACK_NO_AMMO)
	STRING_ENUM_ADD( loadHelper, ACK_CANNOT_PIERCE)
	STRING_ENUM_ADD( loadHelper, ACK_CANNOT_FIND_PATH_TO_TARGET)
	STRING_ENUM_ADD( loadHelper, ACK_ENEMY_ISNT_IN_FIRE_SECTOR)
	STRING_ENUM_ADD( loadHelper, ACK_CANNOT_MOVE_WAITING_FOR_LOADERS)
	STRING_ENUM_ADD( loadHelper, ACK_CANNOT_MOVE_NEED_TO_BE_TOWED_TO_MOVE)
	STRING_ENUM_ADD( loadHelper, ACK_NO_TOO_HEAVY_ARTILLERY_FOR_TRANSPORT)
	STRING_ENUM_ADD( loadHelper, ACK_CANNOT_MOVE_TRACK_DAMAGED)
	STRING_ENUM_ADD( loadHelper, ACK_GOING_TO_STORAGE)
	STRING_ENUM_ADD( loadHelper, ACK_CANNOT_SUPPLY_NOT_PATH)
	STRING_ENUM_ADD( loadHelper, ACK_NO_RESOURCES_CANT_FIND_DEPOT)
	STRING_ENUM_ADD( loadHelper, ACK_NO_RESOURCES_CANT_FIND_PATH_TO_DEPOT)
	STRING_ENUM_ADD( loadHelper, ACK_START_SERVICE_REPAIR)
	STRING_ENUM_ADD( loadHelper, ACK_START_SERVICE_RESUPPLY)
	STRING_ENUM_ADD( loadHelper, ACK_NO_CANNOT_HOOK_ARTILLERY_NO_PATH)
	STRING_ENUM_ADD( loadHelper, ACK_NO_CANNOT_UNHOOK_ARTILLERY_HERE)
	STRING_ENUM_ADD( loadHelper, ACK_NO_ENGINEERS_CANNOT_REACH_BUILDPOINT)
	STRING_ENUM_ADD( loadHelper, ACK_PLANE_TAKING_OFF)
	STRING_ENUM_ADD( loadHelper, ACK_PLANE_LEAVING)
	STRING_ENUM_ADD( loadHelper, ACK_NEED_INSTALL)
	STRING_ENUM_ADD( loadHelper, ACK_BORED_RUSH)
	STRING_ENUM_ADD( loadHelper, ACK_BORED_ATTACK)
	STRING_ENUM_ADD( loadHelper, ACK_BORED_LOW_AMMO)
	STRING_ENUM_ADD( loadHelper, ACK_BORED_NO_AMMO)
	STRING_ENUM_ADD( loadHelper, ACK_BORED_IDLE)
	STRING_ENUM_ADD( loadHelper, ACK_BORED_SNIPER_SNEAK)
	STRING_ENUM_ADD( loadHelper, ACK_BORED_MINIMUM_MORALE)
	STRING_ENUM_ADD( loadHelper, ACK_BORED_LOW_HIT_POINTS)
	STRING_ENUM_ADD( loadHelper, ACK_PLANE_REACH_POINT_START_ATTACK)
	STRING_ENUM_ADD( loadHelper, ACK_GETTING_AMMO)
	STRING_ENUM_ADD( loadHelper, ACK_ATTACKING_AVIATION)
	STRING_ENUM_ADD( loadHelper, ACK_BEING_ATTACKED_BY_AVIATION)
	STRING_ENUM_ADD( loadHelper, ACK_SELECTION_TO_MUCH)
	STRING_ENUM_ADD( loadHelper, ACK_BORED_INFANTRY_TRAVEL)
	STRING_ENUM_ADD( loadHelper, ACK_KILLED_ENEMY)
	STRING_ENUM_ADD( loadHelper, ACK_BUILDING_FINISHED)
	STRING_ENUM_ADD( loadHelper, ACK_CANNOT_FINISH_BUILD)
	STRING_ENUM_ADD( loadHelper, ACK_CANNOT_START_BUILD)
	STRING_ENUM_ADD( loadHelper, ACK_KILLED_ENEMY_INFANTRY)
	STRING_ENUM_ADD( loadHelper, ACK_KILLED_ENEMY_AVIATION)
	STRING_ENUM_ADD( loadHelper, ACK_KILLED_ENEMY_TANK)
	STRING_ENUM_ADD( loadHelper, ACK_UNIT_DIED )


	std::vector<SUnitAckInfoForLoad> forLoad;
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( "Sounds\\Ack\\acks.xml" , STREAM_ACCESS_READ );
	NI_ASSERT_T( pStream != 0, "CANNOT FIND FILE Sounds\\Ack\\acks.xml" );
		
	CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
	tree.Add( "Acks", &forLoad );

	for ( int i = 0; i < forLoad.size(); ++i )
	{
		const SUnitAckInfoForLoad &load = forLoad[i];
		const EAcknowledgementType eType = static_cast<EAcknowledgementType>(loadHelper[load.szAckName]);
		acksInfo[eType] = SUnitAckInfo( load.eType, load.szTextKey.c_str(), load.eColor, load.eSound, load.ePosition, load.nTimeAfterPrevious );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientAckManager::Init()
{
	InitConsts();
	pTextManager = GetSingleton<ITextManager>();
	pConsoleBuffer = GetSingleton<IConsoleBuffer>();
	pGameTimer = GetSingleton<IGameTimer>();

	NTimer::STime curTime = pGameTimer->GetAbsTime();
	//timeNextBored = curTime + ACK_BORED_INTERVAL + rand() * ACK_BORED_INTERVAL_RANDOM / RAND_MAX;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientAckManager::AddDeathAcknowledgement( const CVec3 &vPos, const std::string &sound, const unsigned int timeSinceStart )
{
	if ( pGameTimer->GetGameTimer()->IsPaused() ) 
		return;
	
	//NI_ASSERT_T( timeSinceStart < pGameTimer->GetAbsTime(), NStr::Format( "somebody dead before times with sound %s, timeSinceStart = %d, absTime =%d", sound.c_str(), timeSinceStart, pGameTimer->GetGameTimer() ) );
	
	if ( deathAcks.empty() || ( timeSinceStart < pGameTimer->GetAbsTime() &&timeLastDeath < pGameTimer->GetAbsTime() - timeSinceStart) )
	{
		timeLastDeath = pGameTimer->GetAbsTime() + acksInfo[ACK_UNIT_DIED].nTimeAfterPrevious - timeSinceStart;
		deathAcks.push_back( SDeathAck( vPos, sound, timeSinceStart ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientAckManager::AddAcknowledgement( interface IMOUnit *pUnit, const enum EUnitAckType eAck, const std::string &sound, const int nSet, const unsigned int nTimeSinceStart )
{
	EAcknowledgementType eType = acksInfo[eAck].eType;
	if ( pGameTimer->GetGameTimer()->IsPaused() && ACKT_NOTIFY != eType ) return;


	if ( ACK_UNIT_DIED == eAck )
	{
		if ( deathAcks.empty() || 
				 ( nTimeSinceStart < pGameTimer->GetAbsTime() && timeLastDeath < pGameTimer->GetAbsTime() - nTimeSinceStart) )
		{
		}
		else
			return;
	}

	//�������� ����� ��� � ����� ����
	NI_ASSERT_T( acksInfo.find( eAck ) != acksInfo.end(), NStr::Format( "unredistered Ack %d", eAck ) );
	NI_ASSERT_T( pUnit->IsValid(), "added ack from invalid unit" );

	SAck ack;
	ack.sound = sound;
	ack.eAck = eAck ;
	ack.eMixType = acksInfo[eAck].ePosition == ESP_FROM_INTERFACE ? SFX_INTERFACE : SFX_MIX_IF_TIME_EQUALS;

	switch( eType )
	{
	case ACKT_POSITIVE:
		if ( unitAcks[pUnit].timeRun < pGameTimer->GetAbsTime() && 0 == unitAcks[pUnit].wSoundID )
		{
			unitAcks[pUnit].timeRun = pGameTimer->GetAbsTime() + TIME_ACK_WAIT;
			unitAcks[pUnit].acks.push_back( ack );
		}

		break;
	case ACKT_NEGATIVE:
		{
			// ����� ��� �������� � ������� � ������.
			CAckPredicate  pr( acksInfo, ACKT_POSITIVE );
			CAcks::iterator positives = std::remove_if( unitAcks[pUnit].acks.begin(), unitAcks[pUnit].acks.end(), pr );
			if ( positives == unitAcks[pUnit].acks.end() ) 
			{
				// �� ������ Positive, Negative ������������
			}
			else
			{
				unitAcks[pUnit].acks.erase( positives, unitAcks[pUnit].acks.end() );
				// �������� ���� ��� � �������
				unitAcks[pUnit].acks.push_back( ack );
			}
		}
		
		break;
	case ACKT_SELECTION:
		if ( 0 == unitAcks[pUnit].wSoundID )
		{
			if ( pLastSelected == pUnit )
				++nSelectionCounter;
			else
				nSelectionCounter = 0;
			pLastSelected = pUnit;
			if ( nSelectionCounter >= NUM_SELECTIONS_BEFORE_F_OFF )
			{
				pUnit->AIUpdateAcknowledgement( ACK_SELECTION_TO_MUCH, this, nSet );
				pLastSelected = 0;
			}
			else
				unitAcks[pUnit].acks.push_back( ack );
		}

		break;
	case ACKT_NOTIFY:
		// check if the same ack exists in the list
		{
			if ( unitAcks[pUnit].eCurrentAck != ack.eAck &&
					unitAcks[pUnit].acks.end() == std::find( unitAcks[pUnit].acks.begin(), unitAcks[pUnit].acks.end(), ack ) )
			{
				unitAcks[pUnit].acks.push_back( ack );
			}
		}
		// 
		
		break;
	case ACKT_BORED:
		unitAcks[pUnit].acks.push_back( ack );
		
		break;
	case ACKT_VOID:
		
		break;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD CClientAckManager::GetMessageColor( enum CClientAckManager::EAcknowledgementColor eColor )
{
	//CRAP{ ����� ���������� ��� ��������
	switch ( eColor) 
	{
	case ACOL_INFORMATION:
		return GetGlobalVar( "Scene.Colors.Summer.Text.Information.Color", int(0xffffffff) );
	default:
		return 0xffff0000;
	}
	//CRAP}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char * CClientAckManager::GetAdditionalSound( enum CClientAckManager::EAcknowledgementAdditionalSound eSound )
{

	switch( eSound )
	{
	case AAS_NONE:
		return 0;
	case AAS_INFORMATION:
	case AAS_TAKING_OFF:
		//CRAP{
		//static std::string a = "sounds\\reports\\information";
		//CRAP}
		//return a.c_str();
		break;
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientAckManager::RegisterAck( SUnitAck *ack, const NTimer::STime curTime )
{
	ack->eCurrentAck = (*ack->acks.begin()).eAck;
	acksPresence[ack->eCurrentAck] = curTime;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientAckManager::UnregisterAck( SUnitAck *ack )
{
	ack->eCurrentAck = -1;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientAckManager::Update( interface IScene * pScene )
{
	NTimer::STime curTime = pGameTimer->GetAbsTime();
	ISingleTimer * pTimer = pGameTimer->GetGameTimer();

	if ( !pTimer->IsPaused() )
	{
		for ( std::unordered_map<int, CBoredUnitsContainer>::iterator it = boredUnits.begin(); it != boredUnits.end(); ++it )
		{
			const EUnitAckType eType = static_cast<EUnitAckType>( (*it).first );
			(*it).second.SendAck( curTime, eType, this, acksInfo[eType].nTimeAfterPrevious  );
		}
	}

	//death acks
	for ( CDeathAcks::iterator it = deathAcks.begin(); it != deathAcks.end(); )
	{
		pScene->AddSound( it->szSoundName.c_str(), it->vPos, 
											SFX_MIX_IF_TIME_EQUALS, 
											SAM_ADD_N_FORGET,
											ESCT_GENERIC,
											MIN_ACK_RADIUS, 
											MAX_ACK_RADIUS,
											it->timeSinceStart );
		it = deathAcks.erase( it );
	}

	for ( CUnitsAcks::iterator it = unitAcks.begin(); it != unitAcks.end(); ++it )
	{
		IMOUnit * pUnit = (*it).first;
		SUnitAck &ack = (*it).second;
		if ( 0 != ack.wSoundID && pScene->IsSoundFinished( ack.wSoundID ) )
		{
			pScene->RemoveSound( ack.wSoundID );
			ack.wSoundID = 0;
			UnregisterAck( &ack );
		}

		if ( 0 == ack.wSoundID )
		{
			// run next acknowledgement
			if ( ack.acks.begin() != ack.acks.end() )
			{
				SAck &addedAck = *ack.acks.begin();
				const SUnitAckInfo & currentAskInfo = acksInfo[addedAck.eAck];
				
				// ���� ����� ��� ����������� �� ������
				if ( currentAskInfo.eType == ACKT_POSITIVE && ack.timeRun > curTime )
					continue;
				
				//��������� �� ������� �� ��� Ack ������� ����.
				// ���� ��������, �� ���� �� ���������.
				if ( acksPresence.find(addedAck.eAck) == acksPresence.end() ||
						 curTime - acksPresence[int(addedAck.eAck)] >= currentAskInfo.nTimeAfterPrevious )
				{
					//run acknowledgement sound
					std::string &sound = addedAck.sound;
					if ( 0 != sound.size() )
					{
						CVec3 vPos;
						WORD wDir;
						pUnit->GetPlacement( &vPos, &wDir );
						ack.wSoundID = pScene->AddSound(	sound.c_str(), 
																							vPos, 
																							//CRAP{ IF 1 MORE OF THIS KIND WILL BE ADDED, THEN MOVE TO CONFIGURATION FILE 
																							addedAck.eAck == ACK_BORED_IDLE ? SFX_MIX_IF_TIME_EQUALS : static_cast<ESoundMixType>(addedAck.eMixType), 
																							SAM_NEED_ID,
																							addedAck.eAck == ACK_BORED_IDLE ? ESCT_MUTE_DURING_COMBAT : ESCT_GENERIC,
																							//CRAP}
																							MIN_ACK_RADIUS, 
																							MAX_ACK_RADIUS );
						RegisterAck( &ack, curTime );
					}
				}
				//display string info
				{
					const std::string & szKey = acksInfo[addedAck.eAck].szTextKey;
					IText * pText = pTextManager->GetString( szKey.c_str() );
					IText * pName = pUnit->GetLocalName();
					std::wstring szToDisplay;

					if ( pName )
						szToDisplay += pName->GetString();
					if ( pText )
					{
						szToDisplay += pText->GetString();
						pConsoleBuffer->Write( CONSOLE_STREAM_CHAT, 
																	szToDisplay.c_str(),
																	GetMessageColor( acksInfo[addedAck.eAck].eColor ) );
					}					
				}
				//run additional interface sound
				{
					const char *pAdditionalSound = GetAdditionalSound( acksInfo[addedAck.eAck].eSound );
					if ( pAdditionalSound )
						pScene->AddSound( pAdditionalSound, VNULL3, SFX_INTERFACE, SAM_ADD_N_FORGET );
				}
				ack.acks.pop_front();
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientAckManager::UnitDead( struct SMapObject *pUnit, interface IScene * pScene )
{
	IMOUnit * pTipaUnit = static_cast<IMOUnit*>( pUnit );
	CUnitsAcks::iterator it = unitAcks.find( pTipaUnit ) ;

	if ( it != unitAcks.end() )
	{
		SUnitAck &ack = (*it).second;
		if ( 0 != ack.wSoundID )
		{
			UnregisterAck( &ack );
			pScene->RemoveSound( ack.wSoundID );
		}
	}
	
	unitAcks.erase( it );

	for ( BoredUnits::iterator it = boredUnits.begin(); it != boredUnits.end(); ++it )
		it->second.DelUnit( pTipaUnit );

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientAckManager::RegisterAsBored( EUnitAckType eBored, interface IMOUnit *pObject )
{
	NI_ASSERT_T( eBored <= _ACK_BORED_END && eBored >= _ACK_BORED_BEGIN, "not bored ack passed" );
	NI_ASSERT_T( acksInfo.find( eBored ) != acksInfo.end(), NStr::Format( "unredistered Ack %d", eBored ) );
	NI_ASSERT_T( pObject->IsValid(), "added ack from invalid unit" );

	boredUnits[eBored].AddUnit( pObject );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientAckManager::UnRegisterAsBored( EUnitAckType eBored, interface IMOUnit *pObject )
{
	NI_ASSERT_T( eBored <= _ACK_BORED_END && eBored >= _ACK_BORED_BEGIN, "not bored ack passed" );
	NI_ASSERT_T( acksInfo.find( eBored ) != acksInfo.end(), NStr::Format( "unredistered Ack %d", eBored ) );
	boredUnits[eBored].DelUnit( pObject );
}