#include "StdAfx.h"

#include "OptionsConvert.h"
#include "OptionSystemInternal.h"

#include "..\Main\TextSystem.h"

#include "..\GFX\GFX.h"
#include "..\GFX\GFXHelper.h"
#include "..\SFX\SFX.h"
#include "..\Scene\Scene.h"
#include "..\Scene\PFX.h"
#include "..\GameTT\UIOptions.h"

#include "..\AILogic\AILogic.h"
#include "..\Input\Input.h"
#include "..\Input\InputTypes.h"
#include "..\Main\iMainCommands.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** options system
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
COptionSystem::COptionSystem()
{
	ChangeSerialize( "*", true );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FillOnOff( std::vector<SOptionDropListValue> *pDroplist )
{
	SOptionDropListValue val;
	val.szProgName = "ON";
	pDroplist->push_back( val );

	val.szProgName = "OFF";
	pDroplist->push_back( val );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FillDifficulty( std::vector<SOptionDropListValue> *pDroplist )
{
	SOptionDropListValue val;

	val.szProgName = "Easy";
	pDroplist->push_back( val );

	val.szProgName = "Normal";
	pDroplist->push_back( val );

	val.szProgName = "Hard";
	pDroplist->push_back( val );

	val.szProgName = "Ironman";
	pDroplist->push_back( val );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SetWeather( const variant_t &var )
{
	const std::string szOnOff = (const char*)bstr_t(var);
	if ( szOnOff == "ON" )
		RemoveGlobalVar( "Options.NoWeather" );
	else
		SetGlobalVar( "Options.NoWeather", 1 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SetMouseSensitivity( const variant_t &var )
{
	const int nSensitivity = short( var ); // 0 - 100
	const float fSense = 0.5f + 2.0f * float( nSensitivity ) / 100.0f;
	GetSingleton<ICursor>()->SetSensitivity( fSense );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SetMultipleMarkers( const variant_t &var )
{
	const std::string szOnOff = (const char*)bstr_t(var);
		
	if ( szOnOff == "ON" )
		RemoveGlobalVar( "Options.NoMultipleMarkers" );
	else
		SetGlobalVar( "Options.NoMultipleMarkers", 1 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SetDifficulty( const variant_t &var )
{
	RemoveGlobalVar( "Options.MissionSave.Disabled" );
	const std::string szProgName = (const char*)bstr_t(var);

	if ( szProgName == "Easy" )
	{
		GetSingleton<IAILogic>()->SetDifficultyLevel( 0 );
	}
	else if ( szProgName == "Normal" )
	{
		GetSingleton<IAILogic>()->SetDifficultyLevel( 1 );
	}
	else if ( szProgName == "Hard" )
	{
		GetSingleton<IAILogic>()->SetDifficultyLevel( 2 );
	}
	else if ( szProgName == "Ironman" )
	{
		GetSingleton<IAILogic>()->SetDifficultyLevel( 2 );
		SetGlobalVar( "Options.MissionSave.Disabled", 1 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GetGameSpeed( std::vector<SOptionDropListValue> *pDroplist )
{
	SOptionDropListValue val;
	val.szProgName = "VerySlow";
	pDroplist->push_back( val );
	
	val.szProgName = "Slow";
	pDroplist->push_back( val );

	val.szProgName = "Normal";
	pDroplist->push_back( val );

	val.szProgName = "Fast";
	pDroplist->push_back( val );

	val.szProgName = "VeryFast";
	pDroplist->push_back( val );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SetGameSpeed( const variant_t &var )
{
	if ( GetGlobalVar("MultiplayerGame", 0) == 1 ) 
	{
		const std::string szSpeed = (const char*)bstr_t(var);
		const int nSpeed = NOptionsConvert::GetSpeed( szSpeed );
		GetSingleton<IGameTimer>()->SetSpeed( nSpeed );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SOptionDesc* COptionSystem::GetDesc( const std::string &szVarName ) const
{
	if ( const SOption *pOpt = GetVar(szVarName) )
	{
		descriptor.szName = szVarName;
		descriptor.szDivision = szVarName.substr( 0, szVarName.find('.') );
		descriptor.nDataType = pOpt->vt;
		descriptor.nEditorType = pOpt->nEditorType;
		descriptor.dwFlags = pOpt->dwFlags;
		descriptor.defaultValue = pOpt->defaultValue;
		descriptor.bInstantApply = pOpt->bInstantApply;
		return &descriptor;
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool COptionSystem::Set( const std::string &szVarName, const variant_t &_var )
{
	variant_t var = _var;

	bool bRetVal = false;
	//CRAP{ long string cutting (Game Spy server name only)
	if ( var.vt == VT_BSTR )
	{
		// cut string for allowable lenght
		std::wstring szStr = (const wchar_t*)bstr_t(var);
		if ( szStr.size() > 12 )
		{
			szStr.resize( 8 );
			var = bstr_t(szStr.c_str());
		}
	}
	//CRAP}

	//CRAP{ FOR LOCAL PLAYER NAME
	if ( szVarName == "GamePlay.PlayerName" )
	{
		std::wstring szPlayerName = (const wchar_t*)bstr_t(var);
		if ( szPlayerName.empty() )
		{
			IText * pT = GetSingleton<ITextManager>()->GetDialog( "Textes\\PlayerName" );
			if ( pT )
				szPlayerName = reinterpret_cast<const wchar_t*>(pT->GetString());
			CBase::Set( szVarName, variant_t(szPlayerName.c_str()) );
		}
		else 
			bRetVal = CBase::Set( szVarName, var );
	}
	else
		bRetVal = CBase::Set( szVarName, var );
	//CRAP}
	
	InnerSet( szVarName, var );

	//during sound options we must play sound
	if ( const SOption *pOpt = GetVar(szVarName) )
	{
		if ( pOpt->szAction == "SetSFXVolume" )
			GetSingleton<IScene>()->AddSound( "Sounds\\Settings\\soundsample", VNULL3, SFX_INTERFACE, SAM_ADD_N_FORGET );
	}
	return bRetVal;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void COptionSystem::InnerSet( const std::string &szVarName, const variant_t &var )
{
	//
	if ( const SOption *pOpt = GetVar(szVarName) )
	{
		
		if ( pOpt->szAction == "SetGameSpeed" )
		{
			SetGameSpeed( var );
		}
		else if ( pOpt->szAction == "SetMusicVolume" )
		{
				ISFX * pSFX = GetSingleton<ISFX>();
				short nVolume = short(var) * GetGlobalVar( "Sound.StreamMasterVolume", 1.0f );
				pSFX->SetStreamMasterVolume( nVolume / 100.0f );
				SetGlobalVar( ("Options." + szVarName).c_str(), float(var) );
			}
		else if ( pOpt->szAction == "SetSFXVolume" )
		{
				ISFX * pSFX = GetSingleton<ISFX>();
				short nVolume = short(var) * GetGlobalVar( "Sound.SFXMasterVolume", 1.0f );
				pSFX->SetSFXMasterVolume( nVolume / 100.0f );
				SetGlobalVar( ("Options." + szVarName).c_str(), float(var) );
			}
		else if ( pOpt->szAction == "SetDifficulty" )
		{
			SetDifficulty( var );	
			SetGlobalVar( ("Options." + szVarName).c_str(), (const char*)(_bstr_t)(var) );
		}
		else if ( pOpt->szAction == "SetBlood" )
		{
			if ( std::string((const char*)bstr_t(var)) == "ON" )
				SetGlobalVar( ("Options." + szVarName).c_str(), 1 );
			else
				SetGlobalVar( ("Options." + szVarName).c_str(), 0 );
			// to reload blood settings
		}
		else if ( pOpt->szAction == "SetMouseSensitivity" )
		{
			SetMouseSensitivity( var );
		}
		else if ( pOpt->szAction == "SetHWCursor" ) 
		{
			if ( std::string((const char*)bstr_t(var)) == "OFF" ) 
			{
				GetSingleton<IInput>()->SetDeviceEmulationStatus( DEVICE_TYPE_MOUSE, false );
				GetSingleton<ICursor>()->SetUpdateMode( ICursor::UPDATE_MODE_INPUT );
				SetGlobalVar( ("Options." + szVarName).c_str(), 0 );
			}
			else
			{
				GetSingleton<IInput>()->SetDeviceEmulationStatus( DEVICE_TYPE_MOUSE, true );
				GetSingleton<ICursor>()->SetUpdateMode( ICursor::UPDATE_MODE_WINDOWS );
				SetGlobalVar( ("Options." + szVarName).c_str(), 1 );
			}
		}
		else if ( pOpt->szAction == "SetVideoMode" )
		{
			std::vector<std::string> strings;
			NStr::SplitString( (const char*)bstr_t(var), strings, 'x' );
			NI_ASSERT_T( strings.size() == 3, NStr::Format("Wrong video mode set (%s)", (const char*)bstr_t(var)) );
			if ( strings.size() == 3 ) 
			{
				SetGlobalVar( "GFX.Mode.Mission.SizeX", NStr::ToInt(strings[0]) );
				SetGlobalVar( "GFX.Mode.Mission.SizeY", NStr::ToInt(strings[1]) );
				SetGlobalVar( "GFX.Mode.Mission.BPP", NStr::ToInt(strings[2]) );
			}
		}
		else if ( pOpt->szAction == "SetGammaCorrection" )
		{
			variant_t var;
			const float fBrightness = ((Get( "GFX.Gamma.Brightness", &var ) == true ? float(var) : 50.0f) - 50.0f ) / 50.0f;
			const float fContrast = ((Get( "GFX.Gamma.Contrast", &var ) == true ? float(var) : 50.0f) - 50.0f ) / 50.0f;
			const float fGamma = ((Get( "GFX.Gamma.Gamma", &var ) == true ? float(var) : 50.0f) - 50.0f ) / 50.0f;
			//
			SetGammaCorrection( fBrightness, fContrast, fGamma, GetSingleton<IGFX>(), GetGlobalVar("GFX.Caps.Gamma.Calibrate", 0) != 0 );
		}
		else if ( pOpt->szAction == "SetTextureQuality" ) 
		{
			const std::string szVal = (const char*)(_bstr_t)(var);
			if ( szVal == "High" ) 
				GetSingleton<ITextureManager>()->SetQuality( ITextureManager::TEXTURE_QUALITY_HIGH );
			else if ( szVal == "Compressed" )
				GetSingleton<ITextureManager>()->SetQuality( ITextureManager::TEXTURE_QUALITY_COMPRESSED );
			else if ( szVal == "Low" )
				GetSingleton<ITextureManager>()->SetQuality( ITextureManager::TEXTURE_QUALITY_LOW );
		}
		else if ( pOpt->szAction == "SetWeather" )
			SetWeather( var );
		else if ( pOpt->szAction == "SetMultipleMarkers" )
			SetMultipleMarkers( var );
		else if ( pOpt->szAction == "SetParticlesDensity" ) 
		{
			const float fValue = float( (long)(*pOpt) ) / 100.0f;
			GetSingleton<IParticleManager>()->SetQuality( fValue );
			GetSingleton<IScene>()->SetWeatherQuality( fValue );
			SetGlobalVar( ("Options." + szVarName).c_str(), (const char*)(_bstr_t)(var) );
		}
		else if ( pOpt->szAction == "SetOptBuffers" )
		{
			GetSingleton<IGFX>()->SetOptimizedBuffers( std::string((const char*)bstr_t(var)) == "OFF" ? false : true );
		}
		else if ( var.vt == VT_BSTR )
		{
			SetGlobalVar( ("Options." + szVarName).c_str(), (const char*)(_bstr_t)(var) );
		}
		else 
		{
			SetGlobalVar( ("Options." + szVarName).c_str(), int(var) );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const std::vector<SOptionDropListValue>& COptionSystem::GetDropValues( const std::string &szVarName ) const
{
	droplist.clear();
	if ( const SOption *pOpt = GetVar(szVarName) )
	{
		if ( pOpt->nEditorType == EOET_CLICK_SWITCHES )
		{
			if ( !pOpt->szActionFill.empty() ) 
			{
				// CRAP{ some hardcoded actions - think about change it :)
				if ( pOpt->szActionFill == "GetGameSpeed" )
				{
					GetGameSpeed( &droplist );
				}
				else if ( pOpt->szActionFill == "GetDifficulty" )
				{
					FillDifficulty( &droplist );
				}
				else if ( pOpt->szActionFill == "GetOnOff" )
				{
					FillOnOff( &droplist );
				}
				else if ( pOpt->szActionFill == "GetVideoModes" ) 
				{
					const SGFXDisplayMode *pMode = GetSingleton<IGFX>()->GetDisplayModes();
					while ( pMode->nBPP != 0 ) 
					{
						SOptionDropListValue val;
						val.szProgName = NStr::Format( "%dx%dx%d", pMode->nWidth, pMode->nHeight, pMode->nBPP );
						droplist.push_back( val );

						++pMode;
					}
				}
				else if ( pOpt->szActionFill == "GetTextureQuality" ) 
				{
					ITextManager *pTM = GetSingleton<ITextManager>();
					const int nMaxTextureQuality = GetGlobalVar( "GFX.Limit.TextureQuality", 100 );
					// low quality
					if ( (GetGlobalVar("GFX.Caps.Texture.Format.ARGB0565", 0) != 0) &&
						   (GetGlobalVar("GFX.Caps.Texture.Format.ARGB1555", 0) != 0) &&
							 (GetGlobalVar("GFX.Caps.Texture.Format.ARGB4444", 0) != 0) &&
							 (nMaxTextureQuality >= 0) )
					{
						SOptionDropListValue val;
						val.szProgName = "Low";
						droplist.push_back( val );
					}
					// compressed quality
					if ( (GetGlobalVar("GFX.Caps.Texture.Format.DXT", 0) != 0) && (nMaxTextureQuality >= 1) )
					{
						SOptionDropListValue val;
						val.szProgName = "Compressed";
						droplist.push_back( val );
					}
					// high quality
					if ( (GetGlobalVar("GFX.Caps.Texture.Format.ARGB8888", 0) != 0) && (nMaxTextureQuality >= 2) )
					{
						SOptionDropListValue val;
						val.szProgName = "High";
						droplist.push_back( val );
					}
				}
				// CRAP}
			}
		}
	}
	return droplist;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void COptionSystem::Init()
{
	for ( CPtr<IOptionSystemIterator> pIter = CreateIterator(); !pIter->IsEnd(); pIter->Next() )
	{
		const SOptionDesc * pDesc = pIter->GetDesc();
		variant_t var;
		if ( Get( pDesc->szName, &var ) ) 
			InnerSet( pDesc->szName, var );	//set current
		else
			InnerSet( pDesc->szName, pDesc->defaultValue ); // or set default
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// serialize to configuration file
bool COptionSystem::SerializeConfig( IDataTree *pSS )
{
	CTreeAccessor saver = pSS;
	saver.Add( "Options", static_cast<CBase*>(this) );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void COptionSystem::Repair( IDataTree *pSS, const bool bToDefault )
{
	if ( bToDefault )
	{
		SerializeConfig( pSS );
	}
	else
	{
		CPtr<COptionSystem> pTmpOptions = new COptionSystem;
		pTmpOptions->Repair( pSS, true );

		variant_t dummy;
		
		for ( CPtr<IOptionSystemIterator> pIterator = pTmpOptions->CreateIterator(); !pIterator->IsEnd(); pIterator->Next() )
		{
			const SOptionDesc *pDesc = pIterator->GetDesc();
			const bool bRet = Get( pDesc->szName, &dummy );
			
			//CRAP{ FOR LOCAL PLAYER'S NAME
			if ( pDesc->szName == "GamePlay.PlayerName" )
			{
				const std::wstring szPlayerName = (const wchar_t*)bstr_t(dummy);
				if ( szPlayerName.empty() )
				{
					SetVar( pDesc->szName, pTmpOptions->GetVar( pDesc->szName ) );
					Set( pDesc->szName, pTmpOptions->GetVar( pDesc->szName ) );
				}
			}
			//CRAP}
			else  if ( !bRet )
			{
					SetVar( pDesc->szName, pTmpOptions->GetVar( pDesc->szName ) );
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// begin to iterate through all variables
IOptionSystemIterator* COptionSystem::CreateIterator( const DWORD dwMask )
{
	return new COptionSystemIterator( this, dwMask );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** options system iterator
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
COptionSystemIterator::COptionSystemIterator( COptionSystem *_pOS, const DWORD dwMask )
: CBase( _pOS, COptionMaskAccepter(dwMask) ), pOS( _pOS )
{  
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SOptionDesc* COptionSystemIterator::GetDesc() const 
{ 
	return GetVS()->GetDesc( GetIt()->first ); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const std::vector<SOptionDropListValue>& COptionSystemIterator::GetDropValues() const
{
	return GetVS()->GetDropValues( GetIt()->first ); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** option system sorter
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SOptionSortCmp
{
	const bool operator()( const COptionSystem::CVarsMap::const_iterator &opt1, const COptionSystem::CVarsMap::const_iterator &opt2 ) const
	{
		if ( opt1->second.nOrder == opt2->second.nOrder ) 
			return opt1->first < opt2->first;
		else
			return opt1->second.nOrder < opt2->second.nOrder;
	}
};
void SOptionSorter::Sort( std::list<COptionSystem::CVarsMap::const_iterator> &vals )
{
	vals.sort( SOptionSortCmp() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
