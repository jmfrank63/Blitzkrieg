#ifndef __OPTIONSCONVERT_H__
#define __OPTIONSCONVERT_H__
#pragma ONCE
namespace NOptionsConvert
{
	const int GetSpeed( const std::string &szSpeed )
	{
		const int nMaxSpeed = +GetGlobalVar( "maxspeed", 10 );
		const int nMinSpeed = -GetGlobalVar( "minspeed", 10 );
	
		if ( szSpeed == "VeryFast" )
			return 4;
		else if ( szSpeed == "Fast" )
			return 2;
		else if ( szSpeed == "Slow" )
			return -2;
		else if ( szSpeed == "VerySlow" )
			return -4;
		else 
			return 0;
	}
};
#endif // __OPTIONSCONVERT_H__
