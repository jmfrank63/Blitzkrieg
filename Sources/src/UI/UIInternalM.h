#ifndef __UI_INTERNAL_MANIPULATOR_H__
#define __UI_INTERNAL_MANIPULATOR_H__

#include "../Misc/Manipulator.h"
struct SUIWindowSubState;
extern CPropertiesRegister thePropertiesRegister;

class CUIWindowSubStateManipulator : public CManipulator
{
	OBJECT_NORMAL_METHODS( CUIWindowSubStateManipulator );
	CUIWindowSubState *pSubState;
public:
	CUIWindowSubStateManipulator();
	void SetSubState( CUIWindowSubState *_pSubState ) { pSubState = _pSubState; }
	
	void SetTexture( const variant_t &value );
	void SetMapPosX( const variant_t &value );
	void SetMapPosY( const variant_t &value );
	void SetMapSizeX( const variant_t &value );
	void SetMapSizeY( const variant_t &value );
		
	void GetTexture( variant_t *pValue, int nIndex = -1 );
	void GetMapPosX( variant_t *pValue, int nIndex = -1 );
	void GetMapPosY( variant_t *pValue, int nIndex = -1 );
	void GetMapSizeX( variant_t *pValue, int nIndex = -1 );
	void GetMapSizeY( variant_t *pValue, int nIndex = -1 );
};
class CWindowStateManipulator : public CManipulator
{
	OBJECT_NORMAL_METHODS( CWindowStateManipulator );
	CWindowState *pState;
public:
	CWindowStateManipulator();
	void SetState( CWindowState *_pState ) { pState = _pState; }
	void SetPushSound( const variant_t &value );
	void SetClickSound( const variant_t &value );
	void SetText( const variant_t &value );
	
	void GetTexture( variant_t *pValue, int nIndex = -1 );
	void GetPushSound( variant_t *pValue, int nIndex = -1 );
	void GetClickSound( variant_t *pValue, int nIndex = -1 );
	void GetText( variant_t *pValue, int nIndex = -1 );
};
#endif		//__UI_INTERNAL_MANIPULATOR_H__
