#ifndef __PLAYERPSKILL_H__
#define __PLAYERPSKILL_H__
#pragma ONCE
struct SPlayerRank
{
	std::string szCurrentRank;						// key to local rank name (короткое имя) ( leutenant, general )
	std::string szFullTextKey;						// key to full text (длинный текст описания полученного звания)
	std::string szRankPicture;						// rank picture resource (packed as if it is a medal)
	float fValue;													// 0 - just recieved current rank, 1 - near next rank.
	float fFormerValue;										// former rank value
	int nRankNumber;											//
	SPlayerRank()
		: fValue( 0 ), nRankNumber( 0 ), fFormerValue( 0 ) {  }
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &szFullTextKey );
		saver.Add( 2, &fValue );
		saver.Add( 3, &nRankNumber );
		saver.Add( 4, &szCurrentRank );
		saver.Add( 5, &fFormerValue );
		return 0;
	}
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "CurrentRank", &szCurrentRank );
		saver.Add( "Value", &fValue );
		saver.Add( "RankNumber", &nRankNumber );
		saver.Add( "FullTextKey", &szFullTextKey );
		saver.Add( "FormerValue", &fFormerValue );
		return 0;
	}
};
struct SPlayerSkill
{
	std::string szSkillName;							// key to local string (skill name)
	float fValue;													// 0 - no skill at all, 1 - full skill.
	float fFormerValue;										// before last mission

	SPlayerSkill() : fValue( 0 ), fFormerValue( 0 ) {  }
	SPlayerSkill( const char *pszSkillName, const float _fValue, const float _fFormerValue )
		: szSkillName( pszSkillName ), fValue( _fValue ), fFormerValue( _fFormerValue )
	{
		NormalizeValues( false );
	}
	void NormalizeValues( const bool bInitial );

	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &szSkillName );
		saver.Add( 2, &fValue );
		saver.Add( 3, &fFormerValue );
		return 0;
	}
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "SkillName", &szSkillName );
		saver.Add( "Value", &fValue );
		saver.Add( "FormerValue", &fFormerValue );
		return 0;
	}
};
#endif //__PLAYERPSKILL_H__