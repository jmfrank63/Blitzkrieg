#ifndef __SEASON_H__
#define __SEASON_H__
#pragma ONCE
inline const char* GetSeasonName( const int nSeason )
{
	switch ( nSeason ) 
	{
		case 0: return "Summer";
		case 1: return "Winter";
		case 2: return "Africa";
		default: 
			NI_ASSERT_SLOW_T( nSeason >= 0 && nSeason < 3, NStr::Format("Wrong season %d (avail 0..3)", nSeason) );
			return "Summer";
	}
}
inline const char* GetSeasonApp( const int nSeason )
{
	return nSeason == 1 ? "w" : "";
}
inline const char* GetSeasonApp2( const int nSeason )
{
	switch ( nSeason ) 
	{
		case 0: return "";
		case 1: return "w";
		case 2: return "a";
		default: 
			NI_ASSERT_SLOW_T( nSeason >= 0 && nSeason < 3, NStr::Format("Wrong season %d (avail 0..3)", nSeason) );
			return "";
	}
}
#endif // __SEASON_H__
