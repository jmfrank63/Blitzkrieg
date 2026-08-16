#include "StdAfx.h"

#include "SceneInternal.h"
#include "SoundScene.h"
bool CScene::IsSoundFinished( const WORD wID )
{
	return pSoundScene->IsFinished( wID );
}
void CScene::SetSoundPos( const WORD wID, const CVec3 &vPos )
{
	pSoundScene->SetSoundPos( wID, vPos );
}
WORD CScene::AddSound( 	const char * pszName,
												const CVec3 &vPos,
												const ESoundMixType eMixType,
												const ESoundAddMode eAddMode,
												const ESoundCombatType eCombatType,
												const int nMinRadius,
												const int nMaxRadius,
												const unsigned int nTimeAfterStart )
{
	return pSoundScene->AddSound( pszName, vPos, eMixType, eAddMode, eCombatType, nMinRadius, nMaxRadius, nTimeAfterStart );
}
WORD CScene::AddSoundToMap( const char *pszName, const CVec3 &vPos )
{
	return pSoundScene->AddSoundToMap( pszName, vPos );
}
void CScene::RemoveSoundFromMap( const WORD	wInstanceID )
{
	pSoundScene->RemoveSoundFromMap( wInstanceID );
}
void CScene::RemoveSound( const WORD wID )
{
	pSoundScene->RemoveSound( wID );
}
int CScene::RemoveOrphanLoopedSounds( const WORD *pOwnedIDs, int nOwnedIDs )
{
	std::vector<WORD> ids( pOwnedIDs, pOwnedIDs + nOwnedIDs );
	if ( wAmbientID != 0 )
		ids.push_back( wAmbientID );		// weather loop, owned by the scene itself
	return pSoundScene->RemoveOrphanLoopedSounds( ids.empty() ? 0 : &ids[0], ids.size() );
}
void CScene::UpdateSound( interface ICamera *pCamera )
{
	pSoundScene->Update( pCamera );
}
void CScene::CombatNotify()
{
	pSoundScene->CombatNotify();
}
void CScene::SetSoundSceneMode( const enum ESoundSceneMode eSoundSceneMode )
{
	pSoundScene->SetMode( eSoundSceneMode );
}