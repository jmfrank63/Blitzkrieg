#ifndef __GUN_DESCRIPTOR_INTERNAL__
#define __GUN_DESCRIPTOR_INTERNAL__

#pragma ONCE
#include "Guns.h"
class CCommonUnit;
class CUnitGunDescriptor : public IGunDescriptor
{
	OBJECT_COMPLETE_METHODS( CUnitGunDescriptor );
	DECLARE_SERIALIZE;

	CPtr<CCommonUnit> pUnit;
	int nUnitGun;
public:
	CUnitGunDescriptor() { }
	CUnitGunDescriptor( CCommonUnit *_pUnit, const int _nUnitGun )
		: pUnit( _pUnit ), nUnitGun( _nUnitGun ) { }

	virtual bool IsCorrect() const;
	interface IGun* GetGun() const;
	virtual const int GetNGun() const { return nUnitGun; }
};
#endif // __GUN_DESCRIPTOR_INTERNAL__
