#ifndef __SETANIM_H__
#define __SETANIM_H__
#include "..\Main\RPGStats.h"
inline void SetAnim( IVisObj *pVisObj, const SGDBObjectDesc *pDesc, const int nType )
{
	if ( (pDesc->eGameType == SGVOGT_UNIT) && (pDesc->eVisType == SGVOT_MESH) ) 
	{
		const SUnitBaseRPGStats *pRPG = static_cast<const SUnitBaseRPGStats*>( GetSingleton<IObjectsDB>()->GetRPGStats(pDesc) );
		if ( const std::vector<SUnitBaseRPGStats::SAnimDesc> *pAnims = pRPG->GetAnims(nType) )
		{
			if ( !pAnims->empty() ) 
				static_cast<IObjVisObj*>(pVisObj)->SetAnimation( (*pAnims)[0].nIndex );
		}
	}
	else
		static_cast<IObjVisObj*>(pVisObj)->SetAnimation( nType );
}
#endif // __SETANIM_H__