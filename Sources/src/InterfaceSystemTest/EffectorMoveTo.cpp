
#include "StdAfx.h"
#include "EffectorMoveTo.h"

#include "Window.h"
#include "UISCreen.h"
#include "..\GFX\GFX.h"
int CEffectorMoveTo::operator&( IStructureSaver &ss )
{
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
}

void CEffectorMoveTo::SetElement( CWindow *_pElement )
{
	pElement = _pElement;
	if ( fMoveTime == 0 ) 
	{
		bFinished = true;
		return;
	}
	bFinished = false;
	int x, y;
	pElement->GetPlacement( &x, &y, 0, 0 );
	vMoveFrom = CVec2( x, y );
	vSpeed = vMoveOffset;
	const float fSpeed = fabs( vSpeed ) / fMoveTime;
	if ( fSpeed == 0.0f )
	{
		bFinished = true;
		return;
	}
	Normalize( &vSpeed );
	vSpeed *= fSpeed;
	fElapsedTime = 0;
	
	/*static int a = 0;
	if ( a == 0 )
	{
		CScreen *pScreen = pElement->GetScreen();
		CWindow *pNew = (CWindow*)pElement->Clone();
		pNew->SetName( "Child2" );
		pScreen->AddChild( pNew );
		a = 1;
		pScreen->Init( GetSingleton<IGFX>()->GetScreenRect() );
	}*/
}
const CVec2 CEffectorMoveTo::GetCur() const
{
	CVec2 vCur;
	if ( fElapsedTime >= fMoveTime )
		vCur = vMoveOffset + vMoveFrom;
	else
		vCur = vMoveFrom + vSpeed * fElapsedTime;
	return vCur;
}
void CEffectorMoveTo::Segment( const NTimer::STime timeDiff, interface IScreen *pScreen )
{
	fElapsedTime += timeDiff;
	const CVec2 vCur ( GetCur() );
	if ( vCur == vMoveOffset + vMoveFrom )
	{
		bFinished = true;
		fElapsedTime = fMoveTime;
	}

	pElement->SetPlacement( vCur.x, vCur.y, 0, 0, EWPF_POS_X|EWPF_POS_Y );
}