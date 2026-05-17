#include "StdAfx.h"

#include "Passangers.h"
#include "..\Common\Icons.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void AddPassanger( CPassangersList &passangers, IMOContainer *pContainer, IMOUnit *pUnit, IObjVisObj *pVisObj,
									 const CVec3 &vAdd, const CVec3 &vStep, DWORD dwAlignment )
{
	IObjVisObj *pVO = static_cast_ptr<IObjVisObj*>( pUnit->pVisObj );
	// add unit's HP bar to container
	int nPassangerID = 10000;
	if ( ISceneIcon *pIcon = pVO->GetIcon(ICON_HP_BAR) )
	{
		for ( CPassangersList::const_iterator it = passangers.begin(); it != passangers.end(); ++it )
			nPassangerID = Max( nPassangerID, it->nHPIconID );
		++nPassangerID;
		pVisObj->AddIcon( pIcon, nPassangerID, vAdd, vStep, 0, dwAlignment );
	}
	// add unit to container
	pUnit->SetContainer( pContainer );
	passangers.push_back( SPassanger(pUnit, nPassangerID) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RemovePassanger( CPassangersList &passangers, IMOUnit *pUnit, IObjVisObj *pVisObj )
{
	CPassangersList::iterator itPassanger = passangers.end();
	for ( CPassangersList::iterator it = passangers.begin(); it != passangers.end(); ++it )
	{
		if ( it->pUnit.GetPtr() == pUnit ) 
		{
			// add HP bar icon back to the unit and remove it from container
			IObjVisObj *pVO = static_cast_ptr<IObjVisObj*>( pUnit->pVisObj );
			pVisObj->RemoveIcon( it->nHPIconID );
			if ( ISceneIcon *pIcon = pVO->GetIcon(ICON_HP_BAR) )
			{
				pVO->AddIcon( 0, 0, VNULL3, VNULL3, 0, 0 );
				if ( !pUnit->IsSelected() ) 
					pVO->Select( SGVOSS_UNSELECTED );
				// update unit's HP bar visibility
				if ( pUnit->IsVisible() && ((pUnit->fHP < 1.0f) || pUnit->IsSelected()) )
					pIcon->Enable( true );
				else
					pIcon->Enable( false );
			}
			// remove unit from container
			pUnit->SetContainer( 0 );
			passangers.erase( it );
			return;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsPassangersVisible( const CPassangersList &passangers )
{
	for ( CPassangersList::const_iterator it = passangers.begin(); it != passangers.end(); ++it )
	{
		if ( it->pUnit->IsVisible() ) 
			return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void EnablePassangersIcons( const CPassangersList &passangers, IObjVisObj *pVisObj, bool bEnable )
{
	for ( CPassangersList::const_iterator it = passangers.begin(); it != passangers.end(); ++it )
	{
		if ( ISceneIcon *pIcon = pVisObj->GetIcon(it->nHPIconID) )
			pIcon->Enable( bEnable );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CalcPassangerHPAdd( const float fDepth )
{
//	CVec3 v( 1.0f, -1.0f, sqrt(2.0 / 3.0) );
//	Normalize( &v );
//	v *= fDepth * 4.0f / sqrt(3.0);
	return CVec3( FP_SQRT_2*fDepth, -FP_SQRT_2*fDepth, 1.1547*fDepth );
	// return CVec3( 0, 0, 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
