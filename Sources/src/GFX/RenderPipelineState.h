#ifndef __RENDERPIPELINESTATE_H__
#define __RENDERPIPELINESTATE_H__
#pragma once

class CRenderPipelineState
{
public:
	typedef std::pair<DWORD, DWORD> SStateValue; // first - state, second - value
	typedef std::vector<SStateValue> CStateValuesList;
	struct SStateValues
	{
		CStateValuesList rses;									// render states
		std::vector<CStateValuesList> tsses;		// texture stage states
		std::vector<CStateValuesList> samplers;	// sampler states
	};
private:
	SStateValues statesToSet;
	SStateValues statesToRestore;
	void SetRS( const DWORD dwState, const DWORD dwValue, SStateValues &states )
	{
		states.rses.push_back( SStateValue(dwState, dwValue) );
	}
	void SetTSS( const int nStage, const DWORD dwState, const DWORD dwValue, SStateValues &states )
	{
		if ( nStage >= states.tsses.size() )
			states.tsses.resize( nStage + 1 );
		states.tsses[nStage].push_back( SStateValue(dwState, dwValue) );
	}
	void SetSS( const int nStage, const DWORD dwState, const DWORD dwValue, SStateValues &states )
	{
		if ( nStage >= states.samplers.size() )
			states.samplers.resize( nStage + 1 );
		states.samplers[nStage].push_back( SStateValue(dwState, dwValue) );
	}
public:
	void SetRenderState( const DWORD dwState, const DWORD dwValue ) { SetRS( dwState, dwValue, statesToSet ); }
	void RestoreRenderState( const DWORD dwState, const DWORD dwValue ) { SetRS( dwState, dwValue, statesToRestore ); }
	void SetTextureStageState( const int nStage, const DWORD dwState, const DWORD dwValue ) { SetTSS( nStage, dwState, dwValue, statesToSet ); }
	void RestoreTextureStageState( const int nStage, const DWORD dwState, const DWORD dwValue ) { SetTSS( nStage, dwState, dwValue, statesToRestore ); }
	void SetSamplerState( const int nStage, const DWORD dwState, const DWORD dwValue ) { SetSS( nStage, dwState, dwValue, statesToSet ); }
	void RestoreSamplerState( const int nStage, const DWORD dwState, const DWORD dwValue ) { SetSS( nStage, dwState, dwValue, statesToRestore ); }
	const SStateValues& GetSetValues() const { return statesToSet; }
	const SStateValues& GetRestoreValues() const { return statesToRestore; }
};

class CMaterial
{
	std::string name;
	CRenderPipelineState pipelineState;
public:
	CMaterial() { }
	explicit CMaterial( const char *pszName ) : name( pszName ? pszName : "" ) { }
	const std::string& GetName() const { return name; }
	void SetName( const char *pszName ) { name = pszName ? pszName : ""; }
	CRenderPipelineState& GetPipelineState() { return pipelineState; }
	const CRenderPipelineState& GetPipelineState() const { return pipelineState; }
};

#endif // __RENDERPIPELINESTATE_H__
