#ifndef __WEATHER_H__
#define __WEATHER_H__
#pragma ONCE
class CWeather
{
	DECLARE_SERIALIZE;

	enum EWeatherState
	{
		EWS_CLEAR,
		EWS_BAD,
		EWS_FADE_OFF,
	};

	EWeatherState eState;
	bool bAutoChangeWeather;							// from AI, not from script
	NTimer::STime timeNextCheck;

	void Off();
	void On();
	
public:
	CWeather();
	void Init();

	void Clear();
	
	bool IsActive() const;
	void Segment();
	
	void Switch( const bool bActive );
	
	void SwitchAutomatic( const bool bSwitchAutomatic );
};
#endif // __WEATHER_H__
