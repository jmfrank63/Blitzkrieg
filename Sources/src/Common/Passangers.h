#ifndef __PASSANGERS_H__
#define __PASSANGERS_H__
#pragma ONCE
#include "MapObject.h"
struct SPassanger
{
	CObj<IMOUnit> pUnit;									// unit-passanger
	int nHPIconID;												// HP icon, which was ported to container
	SPassanger() {  }
	SPassanger( IMOUnit *_pUnit, int _nHPIconID )
		: pUnit( _pUnit ), nHPIconID( _nHPIconID ) {  }
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &pUnit );
		saver.Add( 2, &nHPIconID );
		return 0;
	}
};
typedef std::list<SPassanger> CPassangersList;
void AddPassanger( CPassangersList &passangers, IMOContainer *pContainer, IMOUnit *pUnit, IObjVisObj *pVisObj,
									 const CVec3 &vAdd, const CVec3 &vStep, DWORD dwAlignment );
void RemovePassanger( CPassangersList &passangers, IMOUnit *pUnit, IObjVisObj *pVisObj );
bool IsPassangersVisible( const CPassangersList &passangers );
void EnablePassangersIcons( const CPassangersList &passangers, IObjVisObj *pVisObj, bool bEnable );
CVec3 CalcPassangerHPAdd( const float fDepth );
#endif // __PASSANGERS_H__