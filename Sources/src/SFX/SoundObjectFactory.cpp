#include "StdAfx.h"

#include "SoundObjectFactory.h"

#include "SoundEngine.h"
#include "SampleSounds.h"
#include "SoundManager.h"
#include "StreamingSound.h"
static CSoundObjectFactory theSoundObjectFactory;
CSoundObjectFactory::CSoundObjectFactory()
{
	REGISTER_CLASS( this, SFX_SFX, CSoundEngine );
	REGISTER_CLASS( this, SFX_SAMPLE, CSoundSample );
	REGISTER_CLASS( this, SFX_SOUND_MANAGER, CSoundManager );
	REGISTER_CLASS( this, SFX_SOUND_2D, CSound2D );
	REGISTER_CLASS( this, SFX_SOUND_3D, CSound3D );
	REGISTER_CLASS( this, SFX_PLAY_LIST, CPlayList );
}
static SModuleDescriptor theModuleDescriptor( "Sound", SFX_SFX, 0x0100, &theSoundObjectFactory, 0 );
extern "C" BK_EXPORT const SModuleDescriptor* STDCALL GetModuleDescriptor()
{
	return &theModuleDescriptor;
}
