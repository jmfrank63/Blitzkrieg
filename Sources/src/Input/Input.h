#ifndef __INPUT_H__
#define __INPUT_H__
#pragma ONCE
namespace NPlatform { struct PlatformEvent; }
enum 
{
	INPUT_BASE_VALUE	= 0x10040000,
	INPUT_INPUT				= INPUT_BASE_VALUE + 1,
	INPUT_BIND        = INPUT_BASE_VALUE + 2,
	INPUT_SLIDER      = INPUT_BASE_VALUE + 3,
};
enum EInputTextMode
{
	INPUT_TEXT_MODE_NOTEXT		= 0,				// don't process any text inputs
	INPUT_TEXT_MODE_TEXTONLY	= 1,				// convert all keyboard input to the text - don't allow any key event processing
	INPUT_TEXT_MODE_SYSKEYS		= 2,				// convert to text and allow special 'system' keys processing
	INPUT_TEXT_MODE_ALLKEYS		= 3					// allow all keys processing parallel with text convertion
};
enum EInputBindActivationType
{
	INPUT_BIND_ACTIVATION_TYPE_UNKNOWN			= 0,// unknown activation type
	INPUT_BIND_ACTIVATION_TYPE_EVENT_DOWN		= 1,// 'combo formed' event
	INPUT_BIND_ACTIVATION_TYPE_EVENT_UP			= 2,// 'combo destroyed' event
	INPUT_BIND_ACTIVATION_TYPE_SLIDER_PLUS	= 3,// add to slider value
	INPUT_BIND_ACTIVATION_TYPE_SLIDER_MINUS	= 4	// subtract from slider value
};
interface IInputBind : public IRefCount
{
	virtual void Clear() = 0;
	virtual void STDCALL AddControl( const char *pszControl ) = 0;
	virtual void STDCALL SetCommand( const char *pszCommand, const EInputBindActivationType eType ) = 0;

	virtual int STDCALL GetNumControls() const = 0;
	virtual const char* STDCALL GetControl( const int nIndex ) const = 0;
	virtual const char* STDCALL GetCommand() const = 0;
	virtual EInputBindActivationType STDCALL GetActivationType() const = 0;
};
interface IInputSlider : public IRefCount
{
	virtual float STDCALL GetDelta() = 0;
	virtual void STDCALL Reset() = 0;
};
interface IInput : public IRefCount
{
	enum { tidTypeID = INPUT_INPUT };
	virtual bool STDCALL Init() = 0;
	virtual bool STDCALL Done() = 0;
	virtual int STDCALL operator&( IStructureSaver &ss ) = 0;
	virtual bool STDCALL SerializeConfig( IDataTree *pSS ) = 0;
	virtual bool STDCALL IsChanged() const = 0;
	virtual void STDCALL Repair( IDataTree *pSS, const bool bToDefault ) = 0;
	virtual void STDCALL SetDeviceEmulationStatus( const EDeviceType eDeviceType, const bool bEmulate ) = 0;
	virtual bool STDCALL IsEmulated( const EDeviceType eDeviceType ) const = 0;
	virtual void STDCALL EmulateInput( const EDeviceType eDeviceType, const int nControlID, 
		                                 const int nValue, const DWORD time, const int nParam ) = 0;
	virtual void STDCALL ConsumePlatformEvent( const NPlatform::PlatformEvent &event ) = 0;
	virtual void STDCALL PumpMessages( const bool bFocus ) = 0;
	virtual void STDCALL AddMessage( const SGameMessage &msg ) = 0;
	virtual bool STDCALL GetMessage( SGameMessage *pMsg ) = 0;
	virtual bool STDCALL GetTextMessage( STextMessage *pMsg ) = 0;
	virtual IInputSlider* STDCALL CreateSlider( const char *pszName, const float fPower = 1.0f ) = 0;
	virtual void STDCALL ClearMessages() = 0;
	virtual void STDCALL SetBindSection( const char *pszSectionName ) = 0;
	virtual void STDCALL AddBind( const IInputBind *pBind ) = 0;
	virtual void STDCALL RemoveBind( const IInputBind *pBind ) = 0;
	virtual void STDCALL RegisterCommand( const char *pszName, const int nEventID ) = 0;
	virtual void STDCALL UnRegisterCommand( const char *pszName ) = 0;
	virtual EInputTextMode STDCALL GetTextMode() = 0;
	virtual bool STDCALL SetTextMode( const EInputTextMode eMode ) = 0;
	virtual void STDCALL SetCodePage( const int nCodePage ) = 0;
};
#endif // __INPUT_H__
