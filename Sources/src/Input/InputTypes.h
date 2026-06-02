#ifndef __INPUT_TYPES_H__
#define __INPUT_TYPES_H__
#pragma ONCE
enum EControlType
{
	CONTROL_TYPE_KEY		= 1,							// key on the keyboard (push/pop)
	CONTROL_TYPE_AXIS		= 2,							// sliding axis
	CONTROL_TYPE_RAXIS	= 3,							// rotation axis
	CONTROL_TYPE_POV		= 4,							// POV = [P]oint [O]f [V]iew (direction)
	CONTROL_TYPE_POV_X	= 5,							// POV = [P]oint [O]f [V]iew (direction) as rotational axis X component
	CONTROL_TYPE_POV_Y	= 6,							// POV = [P]oint [O]f [V]iew (direction) as rotational axis Y component
	CONTROL_TYPE_SLIDER	= 7,							// slider (sliding axis)
	CONTROL_TYPE_UNKNOWN	= 0x7fffffff		// unknown control
};
enum EDeviceType
{
	DEVICE_TYPE_KEYBOARD			= 1,				// keyboard
	DEVICE_TYPE_MOUSE					= 2,				// mouse
	DEVICE_TYPE_JOYSTICK			= 3,				// joystick
	DEVICE_TYPE_GAMEPAD				= 4,				// gamepad
	DEVICE_TYPE_FLIGHT				= 5,				// controller for flight simulation
	DEVICE_TYPE_DRIVING				= 6,				// device for steering
	DEVICE_TYPE_FIRST_PERSON	= 7,				// first-person action game device

	DEVICE_TYPE_UNKNOWN	= 0x7fffffff			// device that does not fall into another category
};
#define INPUT_CONTROL_MOUSE_AXIS_X	0
#define INPUT_CONTROL_MOUSE_AXIS_Y	4
#define INPUT_CONTROL_MOUSE_AXIS_Z	8
#define INPUT_CONTROL_MOUSE_BUTTON0	12
#define INPUT_CONTROL_MOUSE_BUTTON1	13
#define INPUT_CONTROL_MOUSE_BUTTON2	14
#define INPUT_CONTROL_MOUSE_BUTTON3	15
#define INPUT_CONTROL_MOUSE_BUTTON4	16
#define INPUT_CONTROL_MOUSE_BUTTON5	17
#define INPUT_CONTROL_MOUSE_BUTTON6	18
#define INPUT_CONTROL_MOUSE_BUTTON7	19
#define INPUT_CONTROL_MOUSE_BUTTON8	20
#define INPUT_CONTROL_MOUSE_BUTTON9	21
struct SDeviceDesc
{
	GUID guid;														// GUID of this device
	EDeviceType eType;										// type of this device
	int nID;															// run-time ID of this device
	bool bPoll;														// is this device require 'Poll()' for data retrieving
	std::string szName;										// computer name (ZB. MOUSE)
	std::string szFriendlyName;						// user-friendly name of this device (ZB. Mouse)
};
struct SControlDesc
{
	EControlType eType;										// type of this control
	int nID;															// run-time ID of this control
	std::string szName;										// computer name (ZB. AXIS_X)
	std::string szFriendlyName;						// user-friendly name (ZB. Left Button)
	int nGranularity;											// number of units, which determined one control state change 
};
class CKeyAccumulator
{
	float fAccValue;											// current accumulated value
	DWORD dwActivationTime;								// activation time
	float fActivationPower;								// activation power
public:
	CKeyAccumulator() : fAccValue( 0 ), dwActivationTime( -1 ), fActivationPower( 0 ) {  }
	bool IsActive() const { return dwActivationTime != -1; }
	void Activate( const DWORD time, const float fPower )
	{
		Deactivate( time );
		dwActivationTime = time;
		fActivationPower = fPower;
	}
	void Deactivate( const DWORD time )
	{
		fAccValue += fActivationPower * ( time - dwActivationTime );
		fActivationPower = 0;
		dwActivationTime = -1;
	}
	const float Sample( const DWORD time ) const
	{
		return ( !IsActive() || (time < dwActivationTime) ) ? fAccValue : fAccValue + fActivationPower*( time - dwActivationTime );
	}
};
class CAxisAccumulator
{
	float fAccValue;											// current accumulated value
	DWORD dwActivationTime;								// activation time
public:
	CAxisAccumulator() : fAccValue( 0 ), dwActivationTime( -1 ) {  }
	void Add( const float fValue ) { fAccValue += fValue; }
	const float Sample( const DWORD time ) const { return fAccValue; }
};
class CAccumulator
{
	CKeyAccumulator key;									// key accumulator
	CAxisAccumulator axis;								// axis accumulator
	CKeyAccumulator raxis;								// rotational axis accumulator
public:
	void ActivateKey( const DWORD time, const float fPower ) { key.Activate( time, fPower ); }
	void DeactivateKey( const DWORD time ) { key.Deactivate( time ); }
	void ActivateAxis( const DWORD time, const float fPower ) { axis.Add( fPower ); }
	void DeactivateAxis( const DWORD time ) {  }
	void ActivateRAxis( const DWORD time, const float fPower ) { raxis.Activate( time, fPower ); }
	void DeactivateRAxis( const DWORD time ) { raxis.Deactivate( time ); }
	const float Sample( const DWORD time ) const { return key.Sample( time ) + axis.Sample( time ) + raxis.Sample( time ); }
};
interface IInputVisitor
{
	virtual bool STDCALL VisitControl( class CControl *pControl ) = 0;
	virtual bool STDCALL VisitCombo( class CCombo *pCombo ) = 0;
	virtual bool STDCALL VisitBind( class CBind *pBind ) = 0;
	virtual bool STDCALL VisitCommand( struct SCommand *pCommand ) = 0;
};
#endif // __INPUT_TYPES_H__