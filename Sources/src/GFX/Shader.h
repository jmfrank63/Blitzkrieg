#ifndef __SHADER_H__
#define __SHADER_H__
#pragma once

#include "RenderPipelineState.h"

class CShader : public CRenderPipelineState
{
public:
	typedef SStateValue SShade;
	typedef CStateValuesList CShadesList;
	typedef SStateValues SShadeValues;
};
#endif // __SHADER_H__
