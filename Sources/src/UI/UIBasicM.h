#ifndef __UIBASIC_M_H__
#define __UIBASIC_M_H__
#include "..\Misc\Manipulator.h"
extern CPropertiesRegister thePropertiesRegister;
/*
class CUIWindowStateManipulator : public CManipulator
{
	OBJECT_NORMAL_METHODS( CUIWindowStateManipulator );
	SWindowState *pWindowState;
public:
	CUIWindowStateManipulator();
	
	void SetElementState( SWindowState *_pWindowState ) { pWindowState = _pWindowState; }
	
	void SetLeftX( VARIANT value );
	void SetTopY( VARIANT value );
	void SetRightX( VARIANT value );
	void SetBottomY( VARIANT value );
	
	void GetLeftX( VARIANT *pValue );
	void GetTopY( VARIANT *pValue );
	void GetRightX( VARIANT *pValue );
	void GetBottomY( VARIANT *pValue );
};
*/

class CUIWindowManipulator : public CManipulator
{
	OBJECT_NORMAL_METHODS( CUIWindowManipulator );
	CSimpleWindow *pWindow;
public:
	CUIWindowManipulator();

	void SetWindow( CSimpleWindow *_pWindow ) { pWindow = _pWindow; }
	
	void SetPosX( const variant_t &value );
	void SetPosY( const variant_t &value );
	void SetSizeX( const variant_t &value );
	void SetSizeY( const variant_t &value );
	void SetWindowID( const variant_t &value );
	void SetVisibleStatus( const variant_t &value );
	void SetHighSound( const variant_t &value );
	
	void GetPosX( variant_t *pValue, int nIndex = -1 );
	void GetPosY( variant_t *pValue, int nIndex = -1 );
	void GetSizeX( variant_t *pValue, int nIndex = -1 );
	void GetSizeY( variant_t *pValue, int nIndex = -1 );
	void GetWindowID( variant_t *pValue, int nIndex = -1 );
	void GetVisibleStatus( variant_t *pValue, int nIndex = -1 );
	void GetTexture( variant_t *pValue, int nIndex = -1 );
	void GetHighSound( variant_t *pValue, int nIndex = -1 );
};
#endif		//__UIBASIC_M_H__
