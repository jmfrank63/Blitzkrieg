#include "StdAfx.h"
namespace NMain
{
void ReadAndSetSunlightValue( CTableAccessor &table, const std::string &szSeason, const char *pszValue )
{
	SetGlobalVar( ("Scene.SunLight." + szSeason + "." + pszValue).c_str(), 
		            table.GetFloat("Scene", ("SunLight." + szSeason + "." + pszValue).c_str(), 1.0f) );
}
void ReadAndSetSunlight( CTableAccessor &table, const std::string &szSeason )
{
	ReadAndSetSunlightValue( table, szSeason, "Ambient.A" );
	ReadAndSetSunlightValue( table, szSeason, "Ambient.R" );
	ReadAndSetSunlightValue( table, szSeason, "Ambient.G" );
	ReadAndSetSunlightValue( table, szSeason, "Ambient.B" );

	ReadAndSetSunlightValue( table, szSeason, "Diffuse.A" );
	ReadAndSetSunlightValue( table, szSeason, "Diffuse.R" );
	ReadAndSetSunlightValue( table, szSeason, "Diffuse.G" );
	ReadAndSetSunlightValue( table, szSeason, "Diffuse.B" );

	ReadAndSetSunlightValue( table, szSeason, "Direction.X" );
	ReadAndSetSunlightValue( table, szSeason, "Direction.Y" );
	ReadAndSetSunlightValue( table, szSeason, "Direction.Z" );
}
void ReadAndSetColorValue( CTableAccessor &table, const std::string &szSeason, const char *pszValue )
{
	const DWORD dwA = table.GetInt( "Scene", ("Colors." + szSeason + "." + pszValue + ".A").c_str(), 255 );
	const DWORD dwR = table.GetInt( "Scene", ("Colors." + szSeason + "." + pszValue + ".R").c_str(), 255 );
	const DWORD dwG = table.GetInt( "Scene", ("Colors." + szSeason + "." + pszValue + ".G").c_str(), 255 );
	const DWORD dwB = table.GetInt( "Scene", ("Colors." + szSeason + "." + pszValue + ".B").c_str(), 255 );
	const DWORD dwColor = ( dwA << 24 ) | ( dwR << 16 ) | ( dwG << 8 ) | dwB;
	SetGlobalVar( ("Scene.Colors." + szSeason + "." + pszValue + ".Color").c_str(), int(dwColor) );
}
void ReadAndSetColors( CTableAccessor &table, const std::string &szSeason )
{
	ReadAndSetColorValue( table, szSeason, "Text.Objectives.Received" );
	ReadAndSetColorValue( table, szSeason, "Text.Objectives.Completed" );
	ReadAndSetColorValue( table, szSeason, "Text.Objectives.Failed" );
	ReadAndSetColorValue( table, szSeason, "Text.Information" );
	ReadAndSetColorValue( table, szSeason, "Text.Chat" );
	ReadAndSetColorValue( table, szSeason, "Text.ChatAllies" );
	ReadAndSetColorValue( table, szSeason, "Text.Default" );
	ReadAndSetColorValue( table, szSeason, "FlashFire" );
	ReadAndSetColorValue( table, szSeason, "FlashExplode" );
	ReadAndSetColorValue( table, szSeason, "LevelUp" );
	ReadAndSetColorValue( table, "ToolTip", "Mission" );
	ReadAndSetColorValue( table, "ToolTip", "InterMission" );
	ReadAndSetColorValue( table, "ObjMap", "InterMission" );
	ReadAndSetColorValue( table, szSeason, "Markup.Arrow" );
	ReadAndSetColorValue( table, szSeason, "Markup.Circle" );
}
DWORD GetColorValue( CTableAccessor &table, const std::string &szName, DWORD dwDefault )
{
	const DWORD dwA = table.GetInt( "Scene", (szName + ".A").c_str(), dwDefault >> 24 );
	const DWORD dwR = table.GetInt( "Scene", (szName + ".R").c_str(), (dwDefault & 0x00FF0000) >> 16 );
	const DWORD dwG = table.GetInt( "Scene", (szName + ".G").c_str(), (dwDefault & 0x0000FF00) >> 8 );
	const DWORD dwB = table.GetInt( "Scene", (szName + ".B").c_str(), dwDefault & 0x000000FF );
	return ( dwA << 24 ) | ( dwR << 16 ) | ( dwG << 8 ) | dwB;
}
void ProcessPlayerColor( CTableAccessor &table, const std::string &szName, DWORD dwDefault )
{
	SetGlobalVar( ("Scene.PlayerColors." + szName).c_str(), int(GetColorValue(table, "PlayerColors." + szName, dwDefault)) );	
}
void SetupGlobalVarConsts( CTableAccessor &table )
{
	SetGlobalVar( "Scenario.Reincarnation", table.GetFloat("Scenario", "Reincarnation", 0.5f) );
	SetGlobalVar( "Sound.Listener.Distance", table.GetFloat("Sound", "ListenerDistance", 0) );
	SetGlobalVar( "Sound.RolloffFactor", table.GetFloat("Sound", "RolloffFactor", 1.0f) );
	SetGlobalVar( "Sound.MinDistance", table.GetFloat("Sound", "MinDistance", 1.0f) );
	SetGlobalVar( "Sound.SFXMasterVolume", table.GetFloat("Sound", "SFXMasterVolume", 1.0f) );
	SetGlobalVar( "Sound.StreamMasterVolume", table.GetFloat("Sound", "StreamMasterVolume", 1.0f) );
	SetGlobalVar( "Sound.VideoStreamMasterVolume", table.GetFloat("Sound", "VideoStreamMasterVolume", 1.0f) );
	SetGlobalVar( "Sound.EnableSFX", table.GetInt("Sound", "EnableSFX", 1) );
	SetGlobalVar( "Sound.EnableStream", table.GetFloat("Sound", "EnableStream", 1) );
	SetGlobalVar( "Sound.TimeToFade", table.GetInt("Sound", "TimeToFade", 5000) );
	SetGlobalVar( "Scene.Haze.Enable", table.GetInt("Scene", "Haze.Enable", 1) );
	SetGlobalVar( "Scene.Haze.TopColor.A", table.GetInt("Scene", "Haze.TopColor.A", 36 ) );
	SetGlobalVar( "Scene.Haze.TopColor.R", table.GetInt("Scene", "Haze.TopColor.R", 0  ) );
	SetGlobalVar( "Scene.Haze.TopColor.G", table.GetInt("Scene", "Haze.TopColor.G", 174) );
	SetGlobalVar( "Scene.Haze.TopColor.B", table.GetInt("Scene", "Haze.TopColor.B", 242) );
	SetGlobalVar( "Scene.Haze.BottomColor.A", table.GetInt("Scene", "Haze.BottomColor.A", 36 ) );
	SetGlobalVar( "Scene.Haze.BottomColor.R", table.GetInt("Scene", "Haze.BottomColor.R", 0  ) );
	SetGlobalVar( "Scene.Haze.BottomColor.G", table.GetInt("Scene", "Haze.BottomColor.G", 174) );
	SetGlobalVar( "Scene.Haze.BottomColor.B", table.GetInt("Scene", "Haze.BottomColor.B", 242) );
	SetGlobalVar( "Scene.Haze.Height", table.GetFloat("Scene", "Haze.Height", 1.0f/3.0f) );
	SetGlobalVar( "Scene.GunTrace.Color.A", table.GetInt("Scene", "GunTrace.Color.A", 255 ) );
	SetGlobalVar( "Scene.GunTrace.Color.R", table.GetInt("Scene", "GunTrace.Color.R", 255 ) );
	SetGlobalVar( "Scene.GunTrace.Color.G", table.GetInt("Scene", "GunTrace.Color.G", 64  ) );
	SetGlobalVar( "Scene.GunTrace.Color.B", table.GetInt("Scene", "GunTrace.Color.B", 0   ) );
	SetGlobalVar( "Scene.GunTrace.ProbabilityCoeff", table.GetFloat("Scene", "GunTrace.ProbabilityCoeff", 1.0f ) );
	SetGlobalVar( "Scene.GunTrace.SpeedCoeff", table.GetFloat("Scene", "GunTrace.SpeedCoeff", 1.0f ) );
	SetGlobalVar( "Scene.GunTrace.Length", table.GetFloat("Scene", "GunTrace.Length", 0.33f ) );
	SetGlobalVar( "Scene.FadeOut.Time", table.GetInt("Scene", "FadeOutTime", 1000) );
	ProcessPlayerColor( table, "Player", 0xff000000 );
	ProcessPlayerColor( table, "Neutral", 0xff808080 );
	for ( int i = 1; i <= 15; i++ )
	{
		ProcessPlayerColor( table, NStr::Format( "Allied%d", i ), 0xff00ffff );
		ProcessPlayerColor( table, NStr::Format( "Enemy%d", i ), 0xffff0000 );
	}
	SetGlobalVar( "AI.Weather.TimeToFadeOff", table.GetFloat("AI", "Weather.TimeToFadeOff", 5.0f) );
	SetGlobalVar( "Scene.Weather.Rain.Height", table.GetFloat("Scene", "Weather.Rain.Height", 300.0f) );
	SetGlobalVar( "Scene.Weather.Rain.Density", table.GetInt("Scene", "Weather.Rain.Density", 1000) );
	SetGlobalVar( "Scene.Weather.Rain.Direction.x", table.GetFloat("Scene", "Weather.Rain.Direction.x", 0.01f) );
	SetGlobalVar( "Scene.Weather.Rain.Direction.y", table.GetFloat("Scene", "Weather.Rain.Direction.y", 0.01f) );
	SetGlobalVar( "Scene.Weather.Rain.Direction.z", table.GetFloat("Scene", "Weather.Rain.Direction.z", -0.7f) );
	SetGlobalVar( "Scene.Weather.Rain.TopColor", int(GetColorValue(table, "Weather.Rain.TopColor", 0x20404060)) );
	SetGlobalVar( "Scene.Weather.Rain.BottomColor", int(GetColorValue(table, "Weather.Rain.BottomColor", 0x40404060)) );
	SetGlobalVar( "Scene.Weather.Snow.MinDensity", table.GetInt("Scene", "Weather.Snow.MinDensity", 300) );
	SetGlobalVar( "Scene.Weather.Snow.MaxDensity", table.GetInt("Scene", "Weather.Snow.MaxDensity", 3000) );
	SetGlobalVar( "Scene.Weather.Snow.Height", table.GetFloat("Scene", "Weather.Snow.Height", 500.0f) );
	SetGlobalVar( "Scene.Weather.Snow.FallingSpeed", table.GetFloat("Scene", "Weather.Snow.FallingSpeed", 0.05f) );
	SetGlobalVar( "Scene.Weather.Snow.Amplitude", table.GetFloat("Scene", "Weather.Snow.Amplitude", 0.05f) );
	SetGlobalVar( "Scene.Weather.Snow.Frequency", table.GetFloat("Scene", "Weather.Snow.Frequency", 0.003f) );
	SetGlobalVar( "Scene.Weather.Snow.Color", int(GetColorValue(table, "Weather.Snow.Color", 0xffffffff)) );
	SetGlobalVar( "Scene.Weather.Sand.Height", table.GetFloat("Scene", "Weather.Sand.Height", 300.0f) );
	SetGlobalVar( "Scene.Weather.Sand.Density", table.GetInt("Scene", "Weather.Sand.Density", 2000) );
	SetGlobalVar( "Scene.Weather.Sand.ConeRadius", table.GetFloat("Scene", "Weather.Sand.ConeRadius", 70.0f) );
	SetGlobalVar( "Scene.Weather.Sand.Amplitude", table.GetFloat("Scene", "Weather.Sand.Amplitude", 0.01f) );
	SetGlobalVar( "Scene.Weather.Sand.Frequency", table.GetFloat("Scene", "Weather.Sand.Frequency", 0.001f) );
	SetGlobalVar( "Scene.Weather.Sand.Speed", table.GetFloat("Scene", "Weather.Sand.Speed", 10.0f) );
	SetGlobalVar( "Scene.Weather.Sand.ConeSpeed", table.GetFloat("Scene", "Weather.Sand.ConeSpeed", 0.1f) );
	SetGlobalVar( "Scene.Weather.Sand.Wind.x", table.GetFloat("Scene", "Weather.Sand.Wind.x", -0.01f) );
	SetGlobalVar( "Scene.Weather.Sand.Wind.y", table.GetFloat("Scene", "Weather.Sand.Wind.y", -0.01f) );
	SetGlobalVar( "Scene.Weather.Sand.Wind.z", table.GetFloat("Scene", "Weather.Sand.Wind.z", 0.0f) );
	SetGlobalVar( "Scene.ToolTipTime.Show", table.GetInt("Scene", "ToolTipTime.Show", 1000) );
	SetGlobalVar( "Scene.ToolTipTime.Hide", table.GetInt("Scene", "ToolTipTime.Hide", 10000) );
	SetGlobalVar( "Scene.InfantryIdle.Interval", table.GetInt("Scene", "InfantryIdle.Interval", 10000) );
	SetGlobalVar( "Scene.InfantryIdle.Random", table.GetInt("Scene", "InfantryIdle.Random", 5000) );
	ReadAndSetSunlight( table, "Summer" );
	ReadAndSetSunlight( table, "Winter" );
	ReadAndSetSunlight( table, "Africa" );
	ReadAndSetColors( table, "Summer" );
	ReadAndSetColors( table, "Winter" );
	ReadAndSetColors( table, "Africa" );
	SetGlobalVar( "World.FPSAveragePeriod", table.GetInt( "World", "FPSAveragePeriod", 5000 ) );
	SetGlobalVar( "World.MinRotateRadius", table.GetFloat( "World", "MinRotateRadius", 30.0f ) );
	SetGlobalVar( "World.ReincarnateProbability", table.GetFloat("World", "ReincarnateProbability", 0.5f) );
	SetGlobalVar( "World.Actions.User.Priority", table.GetString("World", "Actions.User.Priority", "").c_str() );
	SetGlobalVar( "World.Actions.Special.Priority", table.GetString("World", "Actions.Special.Priority", "").c_str() );
	SetGlobalVar( "UI.TextAnimTime", table.GetInt("UI", "TextAnimTime", 5000) );
	SetGlobalVar( "InterMissionStreamSound", table.GetString( "UI", "InterMissionStreamSound", "" ).c_str() );
	SetGlobalVar( "CreditsStreamSound", table.GetString( "UI", "CreditsStreamSound", "" ).c_str() );
	SetGlobalVar( "BlinkTime", table.GetInt("UI", "BlinkTime", 2000) );
	SetGlobalVar( "BlinkSubTime", table.GetInt("UI", "BlinkSubTime", 200) );
	SetGlobalVar( "BlinkColor0", table.GetInt("UI", "BlinkColor0", 0xffff0000 ) );
	SetGlobalVar( "BlinkColor1", table.GetInt("UI", "BlinkColor1", 0xffff0000 ) );
}
};
