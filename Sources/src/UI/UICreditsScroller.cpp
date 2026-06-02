#include "StdAfx.h"

#include "UICreditsScroller.h"
#include "..\Input\Input.h"
#include "..\GameTT\CommonId.h"
CUICreditsScroller::CUICreditsScroller()
{
	bWorking = false;
	nLastUpdate = 0;
	nCurrOffset = -511;
	nMaxOffset = 0;
}
void CUICreditsScroller::Visit( interface ISceneVisitor *pVisitor )
{
	if ( !nCmdShow || !states[0].pGfxText )
		return;
	IGFXText *pGFXText = states[0].pGfxText;
	if ( nMaxOffset == 0 )
	{
		pGFXText->SetWidth( vSize.x );
		pGFXText->EnableRedLine( false );
		nMaxOffset = pGFXText->GetNumLines() * pGFXText->GetLineSpace();
	}
	wndRect.x1 += 2;
	wndRect.x2 += 2;
	pVisitor->VisitUIText( pGFXText, wndRect, -nCurrOffset + 2, 0xff000000, FNT_FORMAT_CENTER );
	wndRect.x1 -= 2;
	wndRect.x2 -= 2;
	pVisitor->VisitUIText( pGFXText, wndRect, -nCurrOffset, dwTextColor, FNT_FORMAT_CENTER );
}
void CUICreditsScroller::Draw( IGFX *pGFX )
{
	NI_ASSERT_SLOW_T( false, "Can't user Draw() directly - use visitor pattern" );
	return;
	
	if ( !nCmdShow )
		return;
	pGFX->SetShadingEffect( 3 );
	CPtr<IGFXText> pGFXText = states[0].pGfxText;
	if ( nMaxOffset == 0 )
	{
		pGFXText->SetWidth( vSize.x );
		pGFXText->EnableRedLine( false );
		nMaxOffset = pGFXText->GetNumLines() * pGFXText->GetLineSpace();
	}
	pGFXText->SetColor( 0xff000000 );
	wndRect.x1 += 2;
	wndRect.x2 += 2;
	pGFX->DrawText( pGFXText, wndRect, -nCurrOffset + 2, FNT_FORMAT_CENTER );
	pGFXText->SetColor( dwTextColor );
	wndRect.x1 -= 2;
	wndRect.x2 -= 2;
	pGFX->DrawText( pGFXText, wndRect, -nCurrOffset, FNT_FORMAT_CENTER );
}
bool CUICreditsScroller::Update( const NTimer::STime &currTime )
{
	int nTimePassed = currTime - nLastUpdate;
	nTimePassed = nTimePassed - nTimePassed % 40;
	if ( bWorking )
	{
		nCurrOffset += nTimePassed / 20;
		if ( nCurrOffset > nMaxOffset && nMaxOffset != 0 )
				GetSingleton<IInput>()->AddMessage( SGameMessage( IMC_CANCEL ) );
	}
	nLastUpdate += nTimePassed ;
	return CSimpleWindow::Update( currTime );
}
void CUICreditsScroller::ShowWindow( int _nCmdShow )
{
	CSimpleWindow::ShowWindow( _nCmdShow );
	const bool bNewState = ( _nCmdShow != UI_SW_HIDE );
	if ( bNewState != bWorking )
	{
		bWorking = !bWorking;
		if ( bWorking )
		{
			nCurrOffset = -511;
			nLastUpdate = GetSingleton<IGameTimer>()->GetAbsTime();
		}
	}
}
