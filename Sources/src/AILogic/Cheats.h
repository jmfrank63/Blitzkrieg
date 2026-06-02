#ifndef __CHEATS_H__
#define __CHEATS_H__

#pragma ONCE
struct SCheats
{
	DECLARE_SERIALIZE;
private:
	bool bWarFog;
	int nPartyForWarFog;

	bool bLoadObjects;
	
	bool bTurnOffWarFog;
	bool bHistoryPlaying;
	std::vector<BYTE> immortals;
	std::vector<BYTE> firstShoot;

	bool bPasswordOK;
public:
	SCheats();

	void Init();
	void Clear() { Init(); }

	void SetWarFog( bool bWarFog );
	bool GetWarFog() const { return bWarFog; }

	void SetNPartyForWarFog( const int nPartyForWarFog, bool bUnconditionly );
	const int GetNPartyForWarFog() const { return nPartyForWarFog; }

	void SetLoadObjects( bool bLoadObjects );
	bool GetLoadObjects() const { return bLoadObjects; }

	void SetTurnOffWarFog( bool bTurnOffWarFog );
	bool GetTurnOffWarFog() const { return bTurnOffWarFog; }

	void SetImmortals( const int nParty, const BYTE cValue );
	BYTE GetImmortals( const int nParty ) const { return immortals[nParty]; }

	void SetFirstShoot( const int nParty, const BYTE cValue );
	BYTE GetFirstShoot( const int nParty ) const { return firstShoot[nParty]; }
	
	void SetHistoryPlaying( bool _bHistoryPlaying ) { bHistoryPlaying = _bHistoryPlaying; }
	bool IsHistoryPlaying() const { return bHistoryPlaying; }
	
	const unsigned long MakeCheckSum( const std::string &szPassword );
	void CheckPassword( const std::string &szPassword );
	
	bool IsPasswordOk() const { return bPasswordOK; }
};
#endif // __CHEATS_H__
