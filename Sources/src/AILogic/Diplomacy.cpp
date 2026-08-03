#include "StdAfx.h"

#include "Diplomacy.h"
#include "Units.h"
#include "DifficultyLevel.h"

#include "../Main/ScenarioTracker.h"
CDiplomacy theDipl;

extern CDifficultyLevel theDifficultyLevel;
void CDiplomacy::Load( const std::vector<BYTE> &_playerParty )
{
	playerParty = _playerParty;
	isPlayerExist.clear();
	isPlayerExist.resize( playerParty.size() );

	IScenarioTracker *pScenarioTracker = GetSingleton<IScenarioTracker>();
	int nMyNumber = -1;
	for ( int i = 0; i < playerParty.size(); ++i )
	{
		IPlayerScenarioInfo *pPlayer = pScenarioTracker->GetPlayer( i );
		if ( pPlayer && pPlayer->GetDiplomacySide() != 2 )
			isPlayerExist[i] = 1;

		if ( pPlayer == pScenarioTracker->GetUserPlayer() )
			nMyNumber = i;
	}

	isPlayerExist[playerParty.size() - 1] = 1;

	NI_ASSERT_T( nMyNumber != -1, "Our player isn't found" );
	SetMyNumber( nMyNumber );
}
void CDiplomacy::SetPlayerNotExist( const int nPlayer )
{
	isPlayerExist[nPlayer] = 0;
}
void CDiplomacy::SetDiplomaciesForEditor( const std::vector<BYTE> &_playerParty )
{
	playerParty = _playerParty;
	isPlayerExist.clear();
	isPlayerExist.resize( playerParty.size(), 1 );
	SetMyNumber( 0 );
}
int CDiplomacy::operator&( IStructureSaver &ss ) 
{ 
	CSaverAccessor saver = &ss; 
	
	saver.Add( 1, &playerParty ); 
	saver.Add( 2, &nMyNumber ); 
	saver.Add( 3, &bNetGame ); 
	saver.Add( 4, &isPlayerExist );
	saver.Add( 5, &bEditorMode );

	return 0;
}
bool CDiplomacy::IsPlayerExist( const int nPlayer ) const
{
	if ( nPlayer >= GetNPlayers() )
		return false;
	else
		return isPlayerExist[nPlayer];
}
void CDiplomacy::SetNetGame( bool _bNetGame )
{ 
	bNetGame = _bNetGame;

	if ( bNetGame )
		theDifficultyLevel.SetLevel( 1 );
}
