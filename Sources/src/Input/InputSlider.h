#ifndef __INPUTSLIDER_H__
#define __INPUTSLIDER_H__
#pragma ONCE
class CInputBinder;
class CInputSlider : public CTRefCount<IInputSlider>
{
	OBJECT_SERVICE_METHODS( CInputSlider );
	CPtr<CInputBinder> pInput;						// hook to input to get last input time
	struct SCommand *pCommand;						// hook to command to unregister
	float fLastValue;											// last sampled value
	DWORD dwLastTime;											// last sample time
	float fCoeff;													// coeff
public:
	CInputSlider() : pCommand( 0 ) {  }
	CInputSlider( CInputBinder *_pInput, SCommand *_pCommand, float _fCoeff );
	virtual ~CInputSlider();
	virtual float STDCALL GetDelta();
	virtual void STDCALL Reset();
};
#endif // __INPUTSLIDER_H__
