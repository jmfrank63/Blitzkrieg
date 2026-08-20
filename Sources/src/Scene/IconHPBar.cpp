#include "StdAfx.h"

#include "IconHPBar.h"
// BK_COLOR_TRACE: every write to an HP bar's colors, and the restored state,
// to pin down where a passenger bar's fill color comes from after a load.
static bool ColorTraceOn()
{
	static const bool bOn = getenv( "BK_COLOR_TRACE" ) != 0;
	return bOn;
}
CIconHPBar::CIconHPBar()
{
	bEnable = true;
	bMultiplayer = ( GetGlobalVar( "MultiplayerGame", 0 ) == 1 );
	fPercentage = 1;
	vPos.Set( 0, 0, 20 );
	infoBar.specular = infoMain.specular = 0xff000000;
	infoBar.color = 0xffffffff;
	infoMain.color = 0xffff0000;
	infoBar.pTexture = infoMain.pTexture = 0;
	infoMain.maps.SetEmpty();
	infoBar.maps.SetEmpty();
	dwBarColor = infoBar.color;
	bBarLocked = false;
}
int CIconHPBar::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add(  1, &pTexture );
	saver.Add(  2, &vPos );
	saver.Add(  3, &vSize );
	saver.Add(  4, &fPercentage );
	saver.Add(  5, &bEnable );
	saver.Add(  6, &infoMain.color );
	saver.Add(  7, &infoMain.pos );
	saver.Add(  8, &infoMain.rect );
	saver.Add(  9, &infoBar.color );
	saver.Add( 10, &infoBar.pos );
	saver.Add( 11, &infoBar.rect );
	saver.Add( 12, &infoMain.maps );
	saver.Add( 13, &dwBarColor );
	saver.Add( 14, &bBarLocked );
	if ( saver.IsReading() ) 
	{
		infoMain.pTexture = pTexture;
		if ( ColorTraceOn() )
			fprintf( stderr, "BK_COLOR_TRACE: bar %p restored main=%08x bar=%08x saved=%08x locked=%d hp=%.2f\n",
				(void*)this, (unsigned)infoMain.color, (unsigned)infoBar.color, (unsigned)dwBarColor, (int)bBarLocked, fPercentage );
	}
	return 0;
}
void CIconHPBar::Init( IGFXTexture *_pTexture )
{
	pTexture = _pTexture;
	infoMain.pTexture = pTexture;
}
void CIconHPBar::LockBarColor()
{
	bBarLocked = true;
	infoBar.color = infoMain.color;
	if ( ColorTraceOn() )
		fprintf( stderr, "BK_COLOR_TRACE: bar %p lock -> bar=%08x\n", (void*)this, (unsigned)infoBar.color );
}
void CIconHPBar::UnlockBarColor()
{
	bBarLocked = false;
	SetColor( dwBarColor );
	if ( ColorTraceOn() )
		fprintf( stderr, "BK_COLOR_TRACE: bar %p unlock -> bar=%08x\n", (void*)this, (unsigned)infoBar.color );
}
void CIconHPBar::ForceThinIcon()
{
	bMultiplayer = false;
}
void CIconHPBar::SetColor( DWORD _color ) 
{ 
	if ( ColorTraceOn() && _color != dwBarColor )
		fprintf( stderr, "BK_COLOR_TRACE: bar %p setcolor %08x locked=%d\n", (void*)this, (unsigned)_color, (int)bBarLocked );
	dwBarColor = _color;
	if ( !bBarLocked )
		infoBar.color = (infoBar.color & 0xff000000) | (_color & 0x00ffffff); 
}
void CIconHPBar::SetBorderColor( DWORD dwColor ) 
{ 
	if ( ColorTraceOn() && ((infoMain.color ^ dwColor) & 0x00ffffff) != 0 )
		fprintf( stderr, "BK_COLOR_TRACE: bar %p setborder %08x (was %08x) locked=%d\n", (void*)this, (unsigned)dwColor, (unsigned)infoMain.color, (int)bBarLocked );
	infoMain.color = (infoBar.color & 0xff000000) | (dwColor & 0x00ffffff); 
}
void CIconHPBar::SetAlpha( BYTE alpha ) 
{ 
	infoMain.color = (infoMain.color & 0x00ffffff) | (DWORD(alpha) << 24); 
	infoBar.color = (infoBar.color & 0x00ffffff) | (DWORD(alpha) << 24); 
}
void CIconHPBar::CalcSpriteInfo()
{
	if ( bMultiplayer )
	{
		infoBar.rect.x1 = -vSize.x/2.0f;
		infoBar.rect.y1 = -vSize.y * 1.5f;
		infoBar.rect.x2 = infoBar.rect.x1 + vSize.x * fPercentage;
		infoBar.rect.y2 = -vSize.y * 0.5;
	}
	else
	{
		infoBar.rect.x1 = -vSize.x/2.0f;
		infoBar.rect.y1 = -vSize.y/2.0f;
		infoBar.rect.x2 = infoBar.rect.x1 + vSize.x * fPercentage;
		infoBar.rect.y2 = vSize.y/2.0f;
	}
}
void CIconHPBar::SetSize( const CVec2 &_vSize, bool bHorizontal )
{
	vSize = _vSize;
	if ( pTexture ) 
	{
		const float fSizeX = pTexture->GetSizeX( 0 );
		const float fSizeY = pTexture->GetSizeY( 0 );
		if ( bMultiplayer )
		{
			infoMain.rect.x1 = -vSize.x/2.0f - 1;
			infoMain.rect.y1 = -vSize.y * 2.0f;
			infoMain.rect.x2 = vSize.x/2.0f + 1;
			infoMain.rect.y2 = vSize.y * 2.0f;
			infoMain.maps.Set( 0, 0, (vSize.x + 2.0f) / fSizeX, 1.0f );
			infoMain.maps.Move( 0.5f/fSizeX, 0.5f/fSizeY );
		}
		else
		{
			infoMain.rect.x1 = -vSize.x/2.0f - 1;
			infoMain.rect.y1 = -vSize.y/2.0f - 1;
			infoMain.rect.x2 = vSize.x/2.0f + 1;
			infoMain.rect.y2 = vSize.y/2.0f + 1;
			infoMain.maps.Set( 0, 0, (vSize.x + 2.0f) / fSizeX, (vSize.y + 2.0f) / fSizeY );
			infoMain.maps.Move( 0.5f/fSizeX, 0.5f/fSizeY );
		}
	}
	CalcSpriteInfo();
}
void CIconHPBar::SetLength( float _fPercentage )
{
	fPercentage = _fPercentage; 
	CalcSpriteInfo();
}
void CIconHPBar::Reposition( const CVec3 &vParentPos ) 
{ 
	infoMain.pos = vParentPos + vPos; 
	infoBar.pos = vParentPos + vPos; 
}
bool CIconHPBar::Draw( IGFX *pGFX )
{
	return false;
}
void CIconHPBar::Visit( ISceneVisitor *pVisitor, int nType )
{
	if ( bEnable )
	{
		pVisitor->VisitSprite( &infoMain, SGVOGT_ICON, 0 );
		pVisitor->VisitSprite( &infoBar, SGVOGT_ICON, 0 );
	}
}
