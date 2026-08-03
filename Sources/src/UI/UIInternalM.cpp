#include "StdAfx.h"
#include <comdef.h>
#include "../Main/TextSystem.h"
#include "UIInternal.h"
#include "UIInternalM.h"

namespace
{
	inline const WORD* ToWordText( const wchar_t *pszText )
	{
		return reinterpret_cast<const WORD*>( pszText );
	}
}

CUIWindowSubStateManipulator::CUIWindowSubStateManipulator() 
: CManipulator( &thePropertiesRegister, "WindowSubState" ), pSubState( 0 )
{
	BEGIN_PROPERTIES_MAP( CUIWindowSubState );
	AddNewProperty( this, "Pos.X", &CUIWindowSubStateManipulator::SetMapPosX, &CUIWindowSubStateManipulator::GetMapPosX, SPropertyDesc::VAL_INT, SBaseProperty::LEAF );
	AddNewProperty( this, "Pos.Y", &CUIWindowSubStateManipulator::SetMapPosY, &CUIWindowSubStateManipulator::GetMapPosY, SPropertyDesc::VAL_INT, SBaseProperty::LEAF );
	AddNewProperty( this, "Size.X", &CUIWindowSubStateManipulator::SetMapSizeX, &CUIWindowSubStateManipulator::GetMapSizeX, SPropertyDesc::VAL_INT, SBaseProperty::LEAF );
	AddNewProperty( this, "Size.Y", &CUIWindowSubStateManipulator::SetMapSizeY, &CUIWindowSubStateManipulator::GetMapSizeY, SPropertyDesc::VAL_INT, SBaseProperty::LEAF );
	AddNewProperty( this, "Texture", &CUIWindowSubStateManipulator::SetTexture, &CUIWindowSubStateManipulator::GetTexture, SPropertyDesc::VAL_BROWSEFILE, SBaseProperty::LEAF );
	END_PROPERTIES_MAP;
}

void CUIWindowSubStateManipulator::SetTexture( const variant_t &value )
{
	bstr_t bstrVal = value.bstrVal;
	std::string szName = static_cast<const char*>( bstrVal );
	szName = szName.substr( strlen("c:\\a7\\data\\" ) );
	/*
	std::string szRel;
	if ( MakeRelativePath( szName, "c:\\a7\\data\\", szRel ) )
		szName = szRel;
	*/
	IGFXTexture *pTexture = GetSingleton<ITextureManager>()->GetTexture( szName.c_str() );
	pSubState->pTexture = pTexture;
}

void CUIWindowSubStateManipulator::SetMapPosX( const variant_t &value )
{
	float fSizeX = 256.0f;
	if ( pSubState->pTexture )
		fSizeX = pSubState->pTexture->GetSizeX( 0 );
	
	pSubState->subRects[0].mapa.x2 = pSubState->subRects[0].mapa.Width() + (0.5f + value.intVal) / fSizeX;
	pSubState->subRects[0].mapa.x1 = (0.5f + value.intVal) / fSizeX;
}

void CUIWindowSubStateManipulator::SetMapPosY( const variant_t &value )
{
	float fSizeY = 256.0f;
	if ( pSubState->pTexture )
		fSizeY = pSubState->pTexture->GetSizeY( 0 );
	
	pSubState->subRects[0].mapa.y2 = pSubState->subRects[0].mapa.Height() + (0.5f + value.intVal) / fSizeY;
	pSubState->subRects[0].mapa.y1 = (0.5f + value.intVal) / fSizeY;
}

void CUIWindowSubStateManipulator::SetMapSizeX( const variant_t &value )
{
	float fSizeX = 256.0f;
	if ( pSubState->pTexture )
		fSizeX = pSubState->pTexture->GetSizeX( 0 );
	
	pSubState->subRects[0].mapa.x2 = pSubState->subRects[0].mapa.x1 + (0.5f + value.intVal) / fSizeX;
}

void CUIWindowSubStateManipulator::SetMapSizeY( const variant_t &value )
{
	float fSizeY = 256.0f;
	if ( pSubState->pTexture )
		fSizeY = pSubState->pTexture->GetSizeY( 0 );

	pSubState->subRects[0].mapa.y2 = pSubState->subRects[0].mapa.y1 + (0.5f + value.intVal) / fSizeY;
}

void CUIWindowSubStateManipulator::GetTexture( variant_t *pValue, int nIndex )
{
	std::string szName;
	if ( pSubState->pTexture )
		szName = GetSingleton<ITextureManager>()->GetTextureName( pSubState->pTexture );
	pValue->vt = VT_BSTR;
	pValue->bstrVal = bstr_t( szName.c_str() );
}

void CUIWindowSubStateManipulator::GetMapPosX( variant_t *pValue, int nIndex )
{
	float fSizeX = 256.0f;
	if ( pSubState->pTexture )
		fSizeX = pSubState->pTexture->GetSizeX( 0 );
	
	pValue->vt = VT_INT;
	pValue->intVal = pSubState->subRects[0].mapa.x1 * fSizeX - 0.5f;
}

void CUIWindowSubStateManipulator::GetMapPosY( variant_t *pValue, int nIndex )
{
	float fSizeY = 256.0f;
	if ( pSubState->pTexture )
		fSizeY = pSubState->pTexture->GetSizeY( 0 );
	
	pValue->vt = VT_INT;
	pValue->intVal = pSubState->subRects[0].mapa.y1 * fSizeY - 0.5f;
}

void CUIWindowSubStateManipulator::GetMapSizeX( variant_t *pValue, int nIndex )
{
	float fSizeX = 256.0f;
	if ( pSubState->pTexture )
		fSizeX = pSubState->pTexture->GetSizeX( 0 );
	
	pValue->vt = VT_INT;
	pValue->intVal = pSubState->subRects[0].mapa.Width() * fSizeX;
}

void CUIWindowSubStateManipulator::GetMapSizeY( variant_t *pValue, int nIndex )
{
	float fSizeY = 256.0f;
	if ( pSubState->pTexture )
		fSizeY = pSubState->pTexture->GetSizeY( 0 );
	
	pValue->vt = VT_INT;
	pValue->intVal = pSubState->subRects[0].mapa.Height() * fSizeY;
}

CWindowStateManipulator::CWindowStateManipulator() 
: CManipulator( &thePropertiesRegister, "WindowState" ), pState( 0 )
{
	BEGIN_PROPERTIES_MAP( CWindowState );
	AddNewProperty( this, "SubStates", CWindowStateProperty::SETFUNCTION(0), &CWindowStateManipulator::GetTexture, SPropertyDesc::VAL_BROWSEFILE, SBaseProperty::VECTOR );
	AddNewProperty( this, "Push sound", &CWindowStateManipulator::SetPushSound, &CWindowStateManipulator::GetPushSound, SPropertyDesc::VAL_BROWSEFILE, SBaseProperty::LEAF );
	AddNewProperty( this, "Click sound", &CWindowStateManipulator::SetClickSound, &CWindowStateManipulator::GetClickSound, SPropertyDesc::VAL_BROWSEFILE, SBaseProperty::LEAF );
	AddNewProperty( this, "Text", &CWindowStateManipulator::SetText, &CWindowStateManipulator::GetText, SPropertyDesc::VAL_COMBO, SBaseProperty::LEAF );
	END_PROPERTIES_MAP;
}

void CWindowStateManipulator::SetPushSound( const variant_t &value )
{
	bstr_t bstrVal = value.bstrVal;
	std::string szName = static_cast<const char*>( bstrVal );
	szName = szName.substr( strlen("c:\\a7\\data\\" ) );

	pState->szPushSound = szName;
}

void CWindowStateManipulator::SetClickSound( const variant_t &value )
{
	bstr_t bstrVal = value.bstrVal;
	std::string szName = static_cast<const char*>( bstrVal );
	szName = szName.substr( strlen("c:\\a7\\data\\" ) );
	pState->szClickSound = szName;
}

void CWindowStateManipulator::SetText( const variant_t &value )
{
	bstr_t bstrVal = value.bstrVal;
	IText *pText = pState->pGfxText->GetText();
	const WORD *pszText = ToWordText( (const wchar_t*)bstrVal );
	pText->SetText( pszText );
}

void CWindowStateManipulator::GetTexture( variant_t *pValue, int nIndex )
{
	if ( nIndex >= 3 )
	{
		pValue->vt = VT_BYREF;
		pValue->byref = 0;
	}
	else
	{
		IManipulator *pMan = pState->subStates[nIndex].GetManipulator();
		pValue->vt = VT_BYREF;
		pValue->byref = pMan;
	}
}

void CWindowStateManipulator::GetText( variant_t *pValue, int nIndex )
{
	pValue->vt = VT_BSTR;
	IText *pText = pState->pGfxText->GetText();
	pValue->bstrVal = bstr_t( pText->GetString() );
}

void CWindowStateManipulator::GetPushSound( variant_t *pValue, int nIndex )
{
	std::string szName;
	pValue->vt = VT_BSTR;
	pValue->bstrVal = bstr_t( pState->szPushSound.c_str() );
}

void CWindowStateManipulator::GetClickSound( variant_t *pValue, int nIndex )
{
	pValue->vt = VT_BSTR;
	pValue->bstrVal = bstr_t( pState->szClickSound.c_str() );
}
