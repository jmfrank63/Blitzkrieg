#include "StdAfx.h"

void CBSpline::DumpState() const
{
	GetSingleton<IConsoleBuffer>()->WriteASCII( 500, NStr::Format("spline: x=(%g,%g), dx=(%g,%g), d2x=(%g,%g), d3x=(%g,%g)", x.x, x.y, dx.x, dx.y, d2x.x, d2x.y, d3x.x, d3x.y ), 0, true );
}
