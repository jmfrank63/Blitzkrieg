#include "stdafx.h"

#include "GunDescriptorInternal.h"
#include "CommonUnit.h"
BASIC_REGISTER_CLASS( CUnitGunDescriptor );
bool CUnitGunDescriptor::IsCorrect() const 
{ 
	return IsValid() && pUnit->IsValid() && pUnit->IsAlive(); 
}
IGun* CUnitGunDescriptor::GetGun() const 
{ 
	NI_ASSERT_TF( IsCorrect(), "Can't return non-valid gun", 0 ); 
	return pUnit->GetGun( nUnitGun ); 
}
