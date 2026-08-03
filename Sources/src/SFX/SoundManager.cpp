#include "StdAfx.h"

#include "SoundManager.h"

#include "SampleSounds.h"
#include "../Formats/fmtTerrain.h"
CSoundManager::~CSoundManager()
{
	share2.Clear();
	share3.Clear();
	pSFX = 0;
}
bool CSoundManager::Init() 
{ 
	share2.Init();
	share3.Init();
	share3.SetMinDistance( GetGlobalVar("Sound.MinDistance", 1)*fWorldCellSize/2.0f );
	pSFX = GetSingleton<ISFX>();
	return true;
}
int CSoundManager::operator&( IStructureSaver &ss )
{
	share2.Serialize( &ss );
	share3.Serialize( &ss );
	if ( (&ss)->IsReading() )
	{
		pSFX = GetSingleton<ISFX>();
	}
	return 0;
}
ISound* CSoundManager::GetSound3D( const char *pszName )
{
	CSoundSample *pSample = GetSample3D( pszName );
	if ( pSample == 0 )
		return 0;
	CSound3D *pSound = CreateObject<CSound3D>( SFX_SOUND_3D );
	if ( pSound == 0 )
		return 0;
	pSound->SetSample( pSample );
	return pSound;
}
ISound* CSoundManager::GetSound2D( const char *pszName )
{
	CSoundSample *pSample = GetSample2D( pszName );
	if ( pSample == 0 )
		return 0;
	CSound2D *pSound = CreateObject<CSound2D>( SFX_SOUND_2D );
	if ( pSound == 0 )
		return 0;
	pSound->SetSample( pSample );
	return pSound;
}
const char* CSoundManager::GetSoundName( ISound *pSound )
{
	if ( CSound2D *pSnd = dynamic_cast<CSound2D*>( pSound ) )
	{
		const std::string *pName = share2.GetKey( pSnd->GetSample() );
		return pName != 0 ? pName->c_str() : "";
	}
	else if ( CSound3D *pSnd = dynamic_cast<CSound3D*>( pSound ) )
	{
		const std::string *pName = share3.GetKey( pSnd->GetSample() );
		return pName != 0 ? pName->c_str() : "";
	}
	return "";
}
void CSoundManager::Clear( const ISharedManager::EClearMode eMode, const int nUsage, const int nAmount ) 
{ 
	if ( eMode == ISharedManager::CLEAR_ALL ) 
	{
		share2.Clear(); 
		share3.Clear(); 
	}
	else
	{
		share2.ClearUnreferencedResources(); 
		share3.ClearUnreferencedResources(); 
	}
}
