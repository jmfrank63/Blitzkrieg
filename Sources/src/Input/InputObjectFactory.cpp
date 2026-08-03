#include "StdAfx.h"

#include "InputObjectFactory.h"

#include "InputBinder.h"
#include "InputBind.h"
#include "InputSlider.h"
static CInputObjectFactory theInputObjectFactory;
CInputObjectFactory::CInputObjectFactory()
{
	REGISTER_CLASS( this, INPUT_INPUT, CInputBinder );
	REGISTER_CLASS( this, INPUT_BIND, CInputBind );
	REGISTER_CLASS( this, INPUT_SLIDER, CInputSlider );
}
static SModuleDescriptor theModuleDescriptor( "Input (DX8)", INPUT_INPUT, 0x0200, &theInputObjectFactory, 0 );
extern "C" BK_EXPORT const SModuleDescriptor* STDCALL GetModuleDescriptor()
{
	return &theModuleDescriptor;
}
