#ifndef __CUTSCENES_HELPER_H__
#define __CUTSCENES_HELPER_H__
#pragma ONCE
#include "..\Main\ScenarioTracker.h"
namespace NCutScenes
{
inline void GetCutScenesList( std::list<std::string> &cutscenes )
{
	IUserProfile *pProfile = GetSingleton<IUserProfile>();
	for ( int i = 0; i < pProfile->GetNumCutScenes(); ++i )
		cutscenes.push_back( pProfile->GetCutScene(i) );
}
inline void AddCutScene( const std::string &_szCutSceneName )
{
	GetSingleton<IUserProfile>()->AddCutScene( _szCutSceneName );
}
};
#endif // __CUTSCENES_HELPER_H__