#include "StdAfx.h"

#include "AAFeedBacks.h"
CAAFeedBacks theAAFeedBacks;
#include "AIUnit.h"
#include "Updater.h"
#include "Diplomacy.h"
extern CUpdater updater;
extern CDiplomacy theDipl;
int CAAFeedBacks::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &feedbacks );
	return 0;
}
void CAAFeedBacks::SendFeedBack( CAIUnit *pAA ) const
{
	const CVec2 vCenter( pAA->GetCenter() );
	updater.AddFeedBack( SAIFeedBack(EFB_AAGUN_FIRED, MAKELONG(vCenter.x, vCenter.y) ) );
}
void CAAFeedBacks::Clear()
{
	feedbacks.clear();
}
void CAAFeedBacks::Fired( CAIUnit *pAA, CAIUnit *pTarget )
{
	if ( theDipl.GetDiplStatus( pAA->GetPlayer(), theDipl.GetMyNumber() ) != EDI_ENEMY ) 
		return;
	
	const int nAAID = pAA->GetUniqueId();
	const int nTargetID = pTarget->GetUniqueId();

	CTargetList &targets = feedbacks[nAAID];	// yes, explisit creation. i need it.

	if ( std::find( targets.begin(), targets.end(), nTargetID ) == targets.end() )		
	{
		targets.push_back( nTargetID );
		SendFeedBack( pAA );
	}
}
void CAAFeedBacks::PlaneDeleted( CAIUnit *pTarget )
{
	const int nTargetID = pTarget->GetUniqueId();
	for ( CAAFeedBacksList::iterator it = feedbacks.begin(); it != feedbacks.end(); ++it )
		it->second.remove( nTargetID );
}
