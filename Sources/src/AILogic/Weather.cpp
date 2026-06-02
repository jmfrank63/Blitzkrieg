#include "StdAfx.h"

#include "Weather.h"
#include "Diplomacy.h"
#include "UnitCreation.h"
#include "updater.h"
extern CUpdater updater; 
CWeather theWeather;
extern NTimer::STime curTime;
extern CDiplomacy theDipl;
extern CUnitCreation theUnitCreation;
int CWeather::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 2, &bAutoChangeWeather );
	saver.Add( 3, &timeNextCheck );
	saver.Add( 4, &eState );

	return 0;
}
void CWeather::Clear()
{
	bAutoChangeWeather = true;
	timeNextCheck = 0;
	eState = EWS_CLEAR;
	Init();
}
CWeather::CWeather()
: eState( EWS_CLEAR ), bAutoChangeWeather( true ), timeNextCheck( 0 )
{
}
void CWeather::Init()
{
	SwitchAutomatic( true );
}
void CWeather::SwitchAutomatic( const bool bSwitchAutomatic )
{
	timeNextCheck = curTime + 1000* (SConsts::WEATHER_TURN_PERIOD + Random( 0, SConsts::WEATHER_TURN_PERIOD_RANDOM ));
	bAutoChangeWeather = bSwitchAutomatic;
}
void CWeather::Switch( const bool bActive )
{
	if ( bActive ) 
		On();
	else
		Off();
}
void CWeather::Off()
{
	if ( eState == EWS_BAD )
	{
		eState = EWS_FADE_OFF;
		timeNextCheck = curTime + SConsts::TIME_TO_WEATHER_FADE_OFF * 1000;
		updater.AddFeedBack( SAIFeedBack(EFB_BAD_WEATER, false) );
	}
}
void CWeather::On()
{
	timeNextCheck = curTime + 1000*( SConsts::WEATHER_TIME + Random( 0, SConsts::WEATHER_TIME_RANDOM ) );
	updater.AddFeedBack( SAIFeedBack(EFB_BAD_WEATER, true) );
	eState = EWS_BAD;
	theUnitCreation.BadWeatherStarted();
}
bool CWeather::IsActive() const 
{ 
	return eState != EWS_CLEAR;
}
void CWeather::Segment()
{
	switch( eState )
	{
	case EWS_CLEAR:
		if ( bAutoChangeWeather && timeNextCheck < curTime )
			On();

		break;
	case EWS_BAD:
		if ( bAutoChangeWeather && timeNextCheck < curTime )
			Off();

		break;
	case EWS_FADE_OFF:
		if ( timeNextCheck < curTime )
		{
			eState = EWS_CLEAR;
			timeNextCheck = curTime + 1000* (SConsts::WEATHER_TURN_PERIOD + Random( 0, SConsts::WEATHER_TURN_PERIOD_RANDOM ));
		}
		
		break;
	}
}
