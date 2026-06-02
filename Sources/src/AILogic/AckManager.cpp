#include "stdafx.h"
#include "AckManager.h"
#include "AIUnit.h"
#include "Diplomacy.h"
CAckManager theAckManager;
extern NTimer::STime curTime;
extern CDiplomacy theDipl;
int CAckManager::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &acknowledgements );
	saver.Add( 4, &bored );
	return 0;
}
CAckManager::CAckManager()
{
}
CAckManager::~CAckManager()
{
}
void CAckManager::Clear()
{
	acknowledgements.clear();
	bored.clear();
}
void CAckManager::UpdateAcknowledgments( SAIBoredAcknowledgement **pAckBuffer, int *pnLen )
{
	int nSize = 0;
	for ( CAckTypeBoredPrecence::iterator it = bored.begin(); it != bored.end(); ++it )
	{
		nSize += it->second.size();
	}
	*pnLen = nSize;
	*pAckBuffer = GetTempBuffer<SAIBoredAcknowledgement>( *pnLen );
	
	int i = 0;
	for ( CAckTypeBoredPrecence::iterator it = bored.begin(); it != bored.end(); ++it )
	{
		for ( CBoredPresence::iterator presIter = it->second.begin(); presIter != it->second.end(); ++ presIter )
		{
			(*pAckBuffer)[i].bPresent = presIter->second.second;
			(*pAckBuffer)[i].pObj = presIter->second.first;
			(*pAckBuffer)[i].eAck = static_cast<EUnitAckType>( it->first );
			++i;
		}
	}
	
	bored.clear();
}
void CAckManager::UpdateAcknowledgments( SAIAcknowledgment **pAckBuffer, int *pnLen )
{
	*pnLen = acknowledgements.size();
	*pAckBuffer = GetTempBuffer<SAIAcknowledgment>( *pnLen );

	int i = 0;
	for (		CAcknowledgments::iterator it = acknowledgements.begin(); 
					it != acknowledgements.end(); ++it )
	{
		const SAIAcknowledgment &curAck = *it;
		const IUpdatableObj * pObj = static_cast<const IUpdatableObj*>( curAck.pObj );
		if ( pObj->IsValid() && pObj->IsAlive() ) // non't send dead units' acks
		{
			(*pAckBuffer)[i] = curAck;
			++i;
		}
	}
	*pnLen = i;
	acknowledgements.clear();
}
void CAckManager::AddAcknowledgment( const SAIAcknowledgment &ack )
{
	acknowledgements.push_back( ack );
}
void CAckManager::AddAcknowledgment(	EUnitAckType eAck, IRefCount *pObject, const int nSet )
{
	SAIAcknowledgment ack( eAck, pObject, nSet );
	AddAcknowledgment( ack );
}
void CAckManager::RegisterAsBored( EUnitAckType eBored, class CAIUnit *pObject )
{
	if ( pObject->GetPlayer() != theDipl.GetMyNumber() ) return;
	NI_ASSERT_T( eBored <= _ACK_BORED_END && eBored >= _ACK_BORED_BEGIN, "not bored ack passed" );
	bored[eBored][pObject->GetUniqueId()] = CUnitBoredPresence( pObject, true );
}
void CAckManager::UnRegisterAsBored( EUnitAckType eBored, class CAIUnit *pObject )
{
	if ( pObject->GetPlayer() != theDipl.GetMyNumber() ) return;
	NI_ASSERT_T( eBored <= _ACK_BORED_END && eBored >= _ACK_BORED_BEGIN, "not bored ack passed" );
	bored[eBored][pObject->GetUniqueId()] = CUnitBoredPresence( pObject, false );
}
void CAckManager::UnitDead( class CAIUnit *pObject )
{
	for ( CAckTypeBoredPrecence::iterator it = bored.begin(); it != bored.end(); ++it )
		it->second.erase( pObject->GetUniqueId() );
}