#ifndef __DIPLOMACY_H__
#define __DIPLOMACY_H__

#pragma ONCE
class CDiplomacy
{
	DECLARE_SERIALIZE;
	
	std::vector<BYTE> playerParty;
	int nMyNumber;

	std::vector<BYTE> isPlayerExist;

	bool bNetGame;
	bool bEditorMode;
public:
	CDiplomacy() : nMyNumber( 0 ), bNetGame( false ), bEditorMode( false ) { }
	
	void Load( const std::vector<BYTE> &playerParty );
	void SetDiplomaciesForEditor( const std::vector<BYTE> &playerParty );
	void SetNPlayers( const int nPlayers ) { playerParty.resize( nPlayers + 1 ); playerParty[nPlayers] = 2; }

	void InitForEditor()
	{
		playerParty.resize( 17 );
		isPlayerExist.resize( 17 );

		for ( int i = 0; i < 17; ++i )
		{
			playerParty[i] = 0;
			isPlayerExist[i] = true;
		}

		bEditorMode = true;
	}
	
	void Clear() { bEditorMode = false; }

	const int GetNPlayers() const { return playerParty.size(); }

	const EDiplomacyInfo GetDiplStatus( const BYTE a, const BYTE b ) const
	{ 
		if ( playerParty[a] == 2 || playerParty[b] == 2 )
			return EDI_NEUTRAL;
		else if ( playerParty[a] != playerParty[b] )
			return EDI_ENEMY;
		else
			return EDI_FRIEND;
	}

	const EDiplomacyInfo GetDiplStatusForParties( const BYTE nParty1, const BYTE nParty2 )
	{
		if ( nParty1 == 2 || nParty2 == 2 )
			return EDI_NEUTRAL;
		else
			if ( nParty1 != nParty2 )
				return EDI_ENEMY;
			else
				return EDI_FRIEND;
	}

	const BYTE GetNParty( const BYTE cPlayer ) const { if ( cPlayer == GetNeutralPlayer() ) return GetNeutralParty(); else return playerParty[cPlayer]; }
	const BYTE GetMyNumber() const { return nMyNumber; }
	const BYTE GetMyParty() const { return GetNParty( GetMyNumber() ); }
	const bool IsAIPlayer( const BYTE cPlayer ) const { return !bNetGame && cPlayer == 1; }
	const BYTE GetNeutralPlayer() const { return GetNPlayers() - 1; }
	int GetNeutralParty() const { return 2; }

	void SetParty( const BYTE nPlayer, const BYTE newParty ) { playerParty[nPlayer] = newParty; }
	
	void SetMyNumber( const int nNumber ) { nMyNumber = nNumber; }
	void SetNetGame( bool _bNetGame );
	
	bool IsNetGame() const { return bNetGame; }

	bool IsPlayerExist( const int nPlayer ) const;
	void SetPlayerNotExist( const int nPlayer );

	bool IsEditorMode() const { return bEditorMode; }
};
const int ANY_PARTY = EDI_FRIEND | EDI_ENEMY | EDI_NEUTRAL;
#endif // __DIPLOMACY_H__
