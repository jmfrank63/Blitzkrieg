#include "StdAfx.h"
#include "../Platform/Paths.h"

#include "iMain.h"

#include "../StreamIO/OptionSystem.h"
#include "../GFX/GFX.H"
#include "../GFX/GFXHelper.h"
#include "../SFX/SFX.h"
#include "../Input/Input.h"
#include "../Anim/Animation.h"
#include "../Scene/Scene.h"
#include "../Scene/Terrain.h"
#include "../Scene/PFX.h"
#include "../AILogic/AILogic.h"
#include "../AILogic/DifficultyLevel.h"
#include "../UI/UI.h"
#include "../Image/Image.h"
#include "../Formats/fmtTerrain.h"
#include "../Main/TextSystem.h"
#include "../UI/MaskSystem.h"
#include "../Main/Transceiver.h"
#include "../GameTT/AckManager.h"
#include "../Main/ScenarioTracker.h"
#include "../GameTT/MultiplayerCommandManager.h"
#include "../Common/PauseGame.h"
#include "../Main/CommandsHistoryInterface.h"
#include "../GameTT/MessageReaction.h"
namespace NMain
{
	bool bInitialized = false;
};
inline float MakeGammaValue( const variant_t &var ) { return ( float(var) - 50.0f ) / 50.0f; }
bool STDCALL NMain::SwitchGame( bool bOn )
{
	if ( bOn ) 
	{
		if ( NMain::IsInitialized() )
		{
			if ( IMainLoop *pML = GetSingleton<IMainLoop>() )
				pML->Pause( false, PAUSE_TYPE_INACTIVE );
			variant_t vtBrightness = 0.0f, vtContrast = 0.0f, vtGamma = 0.0f;
			GetSingleton<IOptionSystem>()->Get( "GFX.Gamma.Brightness", &vtBrightness );
			GetSingleton<IOptionSystem>()->Get( "GFX.Gamma.Contrast", &vtContrast );
			GetSingleton<IOptionSystem>()->Get( "GFX.Gamma.Gamma", &vtGamma );
			SetGammaCorrection( MakeGammaValue(vtBrightness), MakeGammaValue(vtContrast), 
				                  MakeGammaValue(vtGamma), GetSingleton<IGFX>(), true );
			GetSingleton<ICursor>()->Acquire( true );
			return true;
		}
	}
	else
	{
		if ( NMain::IsInitialized() )
		{
			if ( IMainLoop *pML = GetSingleton<IMainLoop>() )
				pML->Pause( true, PAUSE_TYPE_INACTIVE );
			SetGammaCorrection( 0, 0, 0, GetSingleton<IGFX>(), true );
			GetSingleton<ICursor>()->Acquire( false );
			return true;
		}
	}
	return false;
}
bool STDCALL NMain::Initialize( HWND hWnd3D, HWND nWndInput, HWND hWndSound, bool bGame )
{
	GetSLS()->AddFactory( GetMainObjectFactory() );
	{
		IFilesInspector *pFI = CreateObject<IFilesInspector>( MAIN_FILES_INSPECTOR );
		RegisterSingleton( IFilesInspector::tidTypeID, pFI );
	}
	{
		const SModuleDescriptor *pDesc = NMain::GetModuleDesc( IMAGE_IMAGE );
		if ( pDesc == 0 || pDesc->pFactory == 0 )
			return false;
		CPtr<IImageProcessor> pIP = CreateObject<IImageProcessor>( pDesc->pFactory, IMAGE_PROCESSOR );
		RegisterSingleton( IImageProcessor::tidTypeID, pIP );
	}
	{
		IGameTimer *pTimer = CreateObject<IGameTimer>( MAIN_GAME_TIMER );//CreateGameTimer();
		RegisterSingleton( IGameTimer::tidTypeID, pTimer );
		pTimer->Init();
	}
	{
		const SModuleDescriptor *pDesc = NMain::GetModuleDesc( INPUT_INPUT );
		if ( pDesc == 0 || pDesc->pFactory == 0 )
			return false;
		CPtr<IInput> pInput = CreateObject<IInput>( pDesc->pFactory, INPUT_INPUT );
		RegisterSingleton( IInput::tidTypeID, pInput );
		pInput->Init();
	}
	{
		const SModuleDescriptor *pDesc = NMain::GetModuleDesc( GFX_GFX );
		if ( pDesc == 0 || pDesc->pFactory == 0 )
			return false;
		IObjectFactory *pFactory = pDesc->pFactory;
		CPtr<IGFX> pGFX = CreateObject<IGFX>( pFactory, GFX_GFX );
		if ( pGFX->Init(0, hWnd3D) != true )
			return false;
		RegisterSingleton( IGFX::tidTypeID, pGFX );	// register GFX to singleton
		CPtr<ITextureManager> pTM = CreateObject<ITextureManager>( pFactory, GFX_TEXTURE_MANAGER );
		RegisterSingleton( ITextureManager::tidTypeID, pTM );	// register texture manager to singleton
		pTM->Init();
		CPtr<IMeshManager> pMM = CreateObject<IMeshManager>( pFactory, GFX_MESH_MANAGER );
		RegisterSingleton( IMeshManager::tidTypeID, pMM );	// register mesh manager to singleton
		pMM->Init();
		CPtr<IFontManager> pFM = CreateObject<IFontManager>( pFactory, GFX_FONT_MANAGER );
		RegisterSingleton( IFontManager::tidTypeID, pFM );	// register font manager to singleton
		pFM->Init();
	}
	{
		const SModuleDescriptor *pDesc = NMain::GetModuleDesc( SFX_SFX );
		if ( pDesc == 0 || pDesc->pFactory == 0 )
			return false;
		IObjectFactory *pFactory = pDesc->pFactory;
		CPtr<ISFX> pSFX = CreateObject<ISFX>( pFactory, SFX_SFX );
		RegisterSingleton( ISFX::tidTypeID, pSFX );	// register GFX to singleton
		// Driver 0 with a real output type. SFX_OUTPUT_NO makes InitDevice
		// return before it opens any device, which silenced the game on every
		// platform; the value only selects among the Windows driver backends,
		// and the portable backend picks the native one regardless.
		pSFX->Init( 0, SFX_OUTPUT_DSOUND, 44100, 32 );
		pSFX->SetDistanceFactor( fWorldCellSize / 2.0f );
		pSFX->SetRolloffFactor( GetGlobalVar("Sound.RolloffFactor", 1.0f) );
		CPtr<ISoundManager> pSM = CreateObject<ISoundManager>( pFactory, SFX_SOUND_MANAGER );
		RegisterSingleton( ISoundManager::tidTypeID, pSM );	// register mesh manager to singleton
		pSM->Init();
	}
	{
		const SModuleDescriptor *pDesc = NMain::GetModuleDesc( ANIM_ANIM );
		if ( pDesc == 0 || pDesc->pFactory == 0 )
			return false;
		IObjectFactory *pFactory = pDesc->pFactory;
		CPtr<IAnimationManager> pAM = CreateObject<IAnimationManager>( pFactory, ANIM_ANIMATION_MANAGER );
		RegisterSingleton( IAnimationManager::tidTypeID, pAM ); // register animation manager to singleton
		pAM->Init();
	}
	{
		const SModuleDescriptor *pDesc = NMain::GetModuleDesc( SCENE_SCENE );
		if ( pDesc == 0 || pDesc->pFactory == 0 )
			return false;
		IObjectFactory *pFactory = pDesc->pFactory;
		CPtr<ICamera> pCamera = CreateObject<ICamera>( pFactory, SCENE_CAMERA );
		CTableAccessor constsTable = NDB::OpenDataTable( "consts.xml" );
		if ( (IDataTable*)constsTable != 0 )
			pCamera->Init( GetSingletonGlobal() );
		else
			NStr::DebugTrace( "Initialize: consts.xml is unavailable, camera uses default constants\n" );
		RegisterSingleton( ICamera::tidTypeID, pCamera ); // register camera to singleton
		CPtr<ICursor> pCursor = CreateObject<ICursor>( pFactory, SCENE_CURSOR );
		RegisterSingleton( ICursor::tidTypeID, pCursor ); // register cursor to singleton
		pCursor->Init( GetSingletonGlobal() );
		pCursor->SetPos( 0, 0 );
		CPtr<IParticleManager> pPM = CreateObject<IParticleManager>( pFactory, PFX_MANAGER );
		RegisterSingleton( IParticleManager::tidTypeID, pPM );	// register ParticleManger to singleton
		pPM->Init();
	}
	{
		const SModuleDescriptor *pDesc = NMain::GetModuleDesc( AI_AI );
		if ( pDesc == 0 || pDesc->pFactory == 0 )
			return false;
		IObjectFactory *pFactory = pDesc->pFactory;
		CPtr<IAILogic> pAILogic = CreateObject<IAILogic>( pFactory, AI_LOGIC );
		RegisterSingleton( IAILogic::tidTypeID, pAILogic );
		const_cast<CDifficultyLevel*>( pAILogic->GetDifficultyLevel() )->Init();
		
		CPtr<IAIEditor> pAIEditor = CreateObject<IAIEditor>( pFactory, AI_EDITOR );
		RegisterSingleton( IAIEditor::tidTypeID, pAIEditor );
	}
	{
		CPtr<ITextManager> pTextMan = CreateObject<ITextManager>( TEXT_MANAGER );
		RegisterSingleton( ITextManager::tidTypeID, pTextMan );
		pTextMan->Init();
		pTextMan->AddTextFile( "textes\\strings.txt" );
		pTextMan->AddTextFile( "textes\\tooltips.txt" );
		pTextMan->AddTextFile( "textes\\acks.txt" );
	}
	{
		const SModuleDescriptor *pDesc = NMain::GetModuleDesc( SCENE_SCENE );
		if ( pDesc == 0 || pDesc->pFactory == 0 )
			return false;
		IObjectFactory *pFactory = pDesc->pFactory;
		CPtr<IScene> pScene = CreateObject<IScene>( pFactory, SCENE_SCENE );
		pScene->Init( GetSingletonGlobal() );
		RegisterSingleton( IScene::tidTypeID, pScene ); // register scene graph to singleton
		CPtr<IVisObjBuilder> pVOB = CreateObject<IVisObjBuilder>( pFactory, SCENE_VISOBJ_BUILDER );
		pVOB->Init( GetSingletonGlobal() );
		RegisterSingleton( IVisObjBuilder::tidTypeID, pVOB );
	}
	{
		const SModuleDescriptor *pDesc = NMain::GetModuleDesc( UI_BASE_VALUE );
		if ( pDesc == 0 || pDesc->pFactory == 0 )
			return false;
		CPtr<IMaskManager> pMM = CreateObject<IMaskManager>( MASK_MANAGER );
		RegisterSingleton( IMaskManager::tidTypeID, pMM );
		pMM->Init();
	}
	
	{		
		ICommandsHistory *pHistory = CreateObject<ICommandsHistory>( MAIN_COMMANDS_HISTORY_INTERNAL );
		RegisterSingleton( ICommandsHistory::tidTypeID, pHistory );
	}
	{
		ITransceiver *pTransceiver = CreateObject<ITransceiver>( MAIN_SP_TRANSCEIVER );
		pTransceiver->Init( GetSingletonGlobal(), -1 );
		RegisterSingleton( ITransceiver::tidTypeID, pTransceiver );
	}
	{
		IClientAckManager *pAckMan = CreateObject<IClientAckManager>( IClientAckManager::tidTypeID );
		pAckMan->Init();
		RegisterSingleton( IClientAckManager::tidTypeID, pAckMan );
		IMPToUICommandManager *pMPToUI = CreateObject<IMPToUICommandManager>( IMPToUICommandManager::tidTypeID ); 
		RegisterSingleton( IMPToUICommandManager::tidTypeID, pMPToUI );
		
		IMessageLinkContainer * pMessageLink = CreateObject<IMessageLinkContainer>( IMessageLinkContainer::tidTypeID );
		pMessageLink->Init();
		RegisterSingleton( IMessageLinkContainer::tidTypeID, pMessageLink );
	}
	{
		IScenarioTracker *pST = CreateObject<IScenarioTracker>( MAIN_SCENARIO_TRACKER );
		RegisterSingleton( IScenarioTracker::tidTypeID, pST );
		IUserProfile *pUserProfile = CreateObject<IUserProfile>( MAIN_USER_PROFILE );
		RegisterSingleton( IUserProfile::tidTypeID, pUserProfile );
	}
	bInitialized = true;
	return true;
}
bool STDCALL NMain::IsInitialized()
{
	return NMain::bInitialized;
}
bool STDCALL NMain::Finalize()
{
	ISingleton *pSingleton = GetSingletonGlobal();
	if ( pSingleton )
	{
		if ( ISFX *pSFX = GetSingleton<ISFX>( pSingleton ) )
			pSFX->Done();
		if ( ITransceiver *pTransceiver = GetSingleton<ITransceiver>( pSingleton ) )
			pTransceiver->Done();
	}

	UnloadAllModules();
	return false;
}
bool STDCALL NMain::CanLaunch()
{
	return NPlatform::Paths::Initialize();
}
