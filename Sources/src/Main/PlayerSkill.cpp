#include "StdAfx.h"
#include "PlayerSkill.h"
#include "..\StreamIO\RandomGen.h"
#include "..\Misc\Win32Random.h"
void SPlayerSkill::NormalizeValues( const bool bInitial )
{
	if ( bInitial )
	{
		fValue = 0.35f;
		fFormerValue = 0.35f;
	}
	else
	{
		if ( fValue <= 0.4f )
			fValue = NWin32Random::Random( 0.35f, 0.4f );
		/*if ( fFormerValue <= 0.4f )
			fFormerValue = Random( 0.35f, 0.4f );*/
	}
}