#include "StdAfx.h"
#include <comdef.h>
#include "UIBasic.h"
#include "UIBasicM.h"
CPropertiesRegister thePropertiesRegister;

CUIWindowManipulator::CUIWindowManipulator() 
: CManipulator( &thePropertiesRegister, "UIWindow" ), pWindow( 0 )
{
	BEGIN_PROPERTIES_MAP( CUIWindow );
	AddNewProperty( this, "Pos.X", &CUIWindowManipulator::SetPosX, &CUIWindowManipulator::GetPosX, SPropertyDesc::VAL_INT, SBaseProperty::LEAF );
	AddNewProperty( this, "Pos.Y", &CUIWindowManipulator::SetPosY, &CUIWindowManipulator::GetPosY, SPropertyDesc::VAL_INT, SBaseProperty::LEAF );
	AddNewProperty( this, "Size.X", &CUIWindowManipulator::SetSizeX, &CUIWindowManipulator::GetSizeX, SPropertyDesc::VAL_INT, SBaseProperty::LEAF );
	AddNewProperty( this, "Size.Y", &CUIWindowManipulator::SetSizeY, &CUIWindowManipulator::GetSizeY, SPropertyDesc::VAL_INT, SBaseProperty::LEAF );
	AddNewProperty( this, "Window ID", &CUIWindowManipulator::SetWindowID, &CUIWindowManipulator::GetWindowID, SPropertyDesc::VAL_INT, SBaseProperty::LEAF );
	AddNewProperty( this, "Visible status", &CUIWindowManipulator::SetVisibleStatus, &CUIWindowManipulator::GetVisibleStatus, SPropertyDesc::VAL_INT, SBaseProperty::LEAF );
	AddNewProperty( this, "States", CUIWindowProperty::SETFUNCTION(0), &CUIWindowManipulator::GetTexture, SPropertyDesc::VAL_BROWSEFILE, SBaseProperty::VECTOR );
	AddNewProperty( this, "High sound", &CUIWindowManipulator::SetHighSound, &CUIWindowManipulator::GetHighSound, SPropertyDesc::VAL_BROWSEFILE, SBaseProperty::LEAF );
	END_PROPERTIES_MAP;
}

void CUIWindowManipulator::SetPosX( const variant_t &value )
{
	pWindow->vPos.x = value.intVal;
}

void CUIWindowManipulator::SetPosY( const variant_t &value )
{
	pWindow->vPos.y = value.intVal;
}

void CUIWindowManipulator::SetSizeX( const variant_t &value )
{
	pWindow->vSize.x = value.intVal;
	
	for ( int i=0; i<pWindow->states.size(); i++ )
	{
		for ( int k=0; k<4; k++ )
		{
			CUIWindowSubState &subState = pWindow->states[i].subStates[k];
			for ( int a=0; a<subState.subRects.size(); a++ )
			{
				subState.subRects[a].rc.right = pWindow->vSize.x;
			}
		}
	}
}

void CUIWindowManipulator::SetSizeY( const variant_t &value )
{
	pWindow->vSize.y = value.intVal;
	
	for ( int i=0; i<pWindow->states.size(); i++ )
	{
		for ( int k=0; k<4; k++ )
		{
			CUIWindowSubState &subState = pWindow->states[i].subStates[k];
			for ( int a=0; a<subState.subRects.size(); a++ )
			{
				subState.subRects[a].rc.bottom = pWindow->vSize.y;
			}
		}
	}
}

void CUIWindowManipulator::SetWindowID( const variant_t &value )
{
	pWindow->nID = value.intVal;
}

void CUIWindowManipulator::SetVisibleStatus( const variant_t &value )
{
	pWindow->nCmdShow = value.intVal;
}

void CUIWindowManipulator::SetHighSound( const variant_t &value )
{
	bstr_t bstrVal = value.bstrVal;
	std::string szName = static_cast<const char*>( bstrVal );
	if ( szName.size() == 0 )
		return;
	szName = szName.substr( strlen("c:\\a7\\data\\" ) );
	
	pWindow->szHighSound = szName;
}


void CUIWindowManipulator::GetPosX( variant_t *pValue, int nIndex )
{
	pValue->vt = VT_INT;
	pValue->intVal = pWindow->vPos.x;
}

void CUIWindowManipulator::GetPosY( variant_t *pValue, int nIndex )
{
	pValue->vt = VT_INT;
	pValue->intVal = pWindow->vPos.y;
}

void CUIWindowManipulator::GetSizeX( variant_t *pValue, int nIndex )
{
	pValue->vt = VT_INT;
	pValue->intVal = pWindow->vSize.x;
}

void CUIWindowManipulator::GetSizeY( variant_t *pValue, int nIndex )
{
	pValue->vt = VT_INT;
	pValue->intVal = pWindow->vSize.y;
}

void CUIWindowManipulator::GetWindowID( variant_t *pValue, int nIndex )
{
	pValue->vt = VT_INT;
	pValue->intVal = pWindow->nID;
}

void CUIWindowManipulator::GetVisibleStatus( variant_t *pValue, int nIndex )
{
	pValue->vt = VT_INT;
	pValue->boolVal = pWindow->nCmdShow;
}

void CUIWindowManipulator::GetTexture( variant_t *pValue, int nIndex )
{
	if ( nIndex >= pWindow->states.size() )
	{
		pValue->vt = VT_BYREF;
		pValue->byref = 0;
	}
	else
	{
		IManipulator *pMan = pWindow->states[nIndex].GetManipulator();
		pValue->vt = VT_BYREF;
		pValue->byref = pMan;
	}
}

void CUIWindowManipulator::GetHighSound( variant_t *pValue, int nIndex )
{
	pValue->vt = VT_BSTR;
	pValue->bstrVal = bstr_t( pWindow->szHighSound.c_str() );

}
