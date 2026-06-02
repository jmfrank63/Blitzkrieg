#include "stdafx.h"

#include "DifficultyLevel.h"
#include "Diplomacy.h"
CDifficultyLevel theDifficultyLevel;
extern CDiplomacy theDipl;
void CDifficultyLevel::Init()
{
	levelsNames.resize( 3 );
	levelsNames[0] = "Easy";
	levelsNames[1] = "Normal";
	levelsNames[2] = "Hard";

	coeffNames.resize( EM_MAX_NUM );
	coeffNames[0] = "Silhouette";
	coeffNames[1] = "Piercing";
	coeffNames[2] = "Damage";
	coeffNames[3] = "RotateSpeed";
	coeffNames[4] = "Dispersion";

	partiesNames.resize( 2 );
	partiesNames[0] = "Friends";
	partiesNames[1] = "Enemies";
	
	coeff.resize( 3 );
	coeff[0].SetSizes( EM_MAX_NUM, levelsNames.size() );
	coeff[1].SetSizes( EM_MAX_NUM, levelsNames.size() );
	coeff[2].SetSizes( EM_MAX_NUM, levelsNames.size() );

	coeff[2].Set( 1.0f );

	CTableAccessor constsTbl = NDB::OpenDataTable( "consts.xml" );

	for ( int nLevel = 0; nLevel < levelsNames.size(); ++nLevel )
	{
		for ( int nParty = 0; nParty < 2; ++nParty )
		{
			for ( int nCoeff = 0; nCoeff < coeffNames.size(); ++nCoeff )
			{
				const std::string szEntryName = "Levels." + levelsNames[nLevel] + "." + partiesNames[nParty] + "." + coeffNames[nCoeff];
				coeff[nParty][nLevel][nCoeff] = constsTbl.GetFloat( "AI", szEntryName.c_str(), 1.0f );
			}
		}
	}
	
	if ( theDipl.IsNetGame() )
		nLevel = 1;

	nCheatLevel = 255;
}
void CDifficultyLevel::SetLevel( const int _nLevel )
{ 
	nLevel = _nLevel;
}
int CDifficultyLevel::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	if ( saver.IsReading() )
		Init();
	saver.Add( 1, &nLevel );

	return 0;
}
