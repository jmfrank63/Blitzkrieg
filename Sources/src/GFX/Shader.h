#ifndef __SHADER_H__
#define __SHADER_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CShader
{
public:
	typedef std::pair<DWORD, DWORD> SShade;	// first - state, second - value
	typedef std::vector<SShade> CShadesList;
	struct SShadeValues
	{
		CShadesList rses;										// render states
		std::vector<CShadesList> tsses;			// texture stage states
		std::vector<CShadesList> samplers;	// sampler states
	};
private:
	//
	SShadeValues shadesToSet;							// shades to set in this shader
	SShadeValues shadesToRestore;					// shades to restore befor the other shader
	//
	void SetRS( const DWORD dwState, const DWORD dwValue, SShadeValues &shades )
	{
		shades.rses.push_back( SShade(dwState, dwValue) ); 
	}
	void SetTSS( const int nStage, const DWORD dwState, const DWORD dwValue, SShadeValues &shades )
	{
		if ( nStage >= shades.tsses.size() )
			shades.tsses.resize( nStage + 1 );
		shades.tsses[nStage].push_back( SShade(dwState, dwValue) );
	}
	void SetSS( const int nStage, const DWORD dwState, const DWORD dwValue, SShadeValues &shades )
	{
		if ( nStage >= shades.samplers.size() )
			shades.samplers.resize( nStage + 1 );
		shades.samplers[nStage].push_back( SShade(dwState, dwValue) );
	}
public:
	// setup shader
	void SetRenderState( const DWORD dwState, const DWORD dwValue ) { SetRS( dwState, dwValue, shadesToSet ); }
	void RestoreRenderState( const DWORD dwState, const DWORD dwValue )  { SetRS( dwState, dwValue, shadesToRestore ); }
	void SetTextureStageState( const int nStage, const DWORD dwState, const DWORD dwValue ) { SetTSS( nStage, dwState, dwValue, shadesToSet ); }
	void RestoreTextureStageState( const int nStage, const DWORD dwState, const DWORD dwValue ) { SetTSS( nStage, dwState, dwValue, shadesToRestore ); }
	void SetSamplerState( const int nStage, const DWORD dwState, const DWORD dwValue ) { SetSS( nStage, dwState, dwValue, shadesToSet ); }
	void RestoreSamplerState( const int nStage, const DWORD dwState, const DWORD dwValue ) { SetSS( nStage, dwState, dwValue, shadesToRestore ); }
	// retrieve data
	const SShadeValues& GetSetValues() const { return shadesToSet; }
	const SShadeValues& GetRestoreValues() const { return shadesToRestore; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SHADER_H__
