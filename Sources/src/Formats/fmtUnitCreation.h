#if !defined(__FMT__UNIT_CREATION__H__)
#define __FMT__UNIT_CREATION__H__
#pragma ONCE

struct SUCAircraft
{
	std::string szName;										//имя самолета в Objecst.
	int nFormationSize;										//сколько самолетов за 1 вызов
	int nPlanes;													// сколько самолетов у игрока

	SUCAircraft() : nFormationSize( 1 ), nPlanes( 1 ) {}
	SUCAircraft( const SUCAircraft &rUCAircraft )
		: szName( rUCAircraft.szName ),
			nFormationSize( rUCAircraft.nFormationSize ),
			nPlanes( rUCAircraft.nPlanes ) {}
	SUCAircraft& operator=( const SUCAircraft &rUCAircraft )
	{
		if ( &rUCAircraft != this )
		{
			szName = rUCAircraft.szName;
			nFormationSize = rUCAircraft.nFormationSize;
			nPlanes = rUCAircraft.nPlanes;
		}
		return *this;
	}

	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;

		saver.Add( "Name", &szName );
		saver.Add( "FormationSize", &nFormationSize );
		saver.Add( "Planes", &nPlanes );
		
		return 0;
	}
	
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;

		saver.Add( 1, &szName );
		saver.Add( 2, &nFormationSize );
		saver.Add( 3, &nPlanes );
		
		return 0;
	}
};

struct SUCAviation
{
	enum AIRCRAFT_TYPE
	{
		AT_SCOUT				= 0,
		AT_FIGHTER			= 1,
		AT_PARADROPER		= 2,
		AT_BOMBER				= 3,
		AT_BATTLEPLANE	= 4,
		AT_COUNT				= 5,
	};
	
	std::vector<SUCAircraft> aircrafts;		//Самолеты по типам
	std::string szParadropSquadName;			//Парашютисты
	int nParadropSquadCount;							//Количество парашютистов
	int	nRelaxTime;												//промежуток времени между вызовами ( сек )
	std::list<CVec3> vAppearPoints;				//точки возможного появления (координата аэродрома)

	SUCAviation()
		:	nParadropSquadCount( 1 ), nRelaxTime( 30 ) { Validate(); }
	SUCAviation( const SUCAviation &rUCAviation )
		: aircrafts( rUCAviation.aircrafts ),
			szParadropSquadName( rUCAviation.szParadropSquadName ),
			nParadropSquadCount( rUCAviation.nParadropSquadCount ),
			nRelaxTime( rUCAviation.nRelaxTime ),
			vAppearPoints( rUCAviation.vAppearPoints ) { Validate(); }
	SUCAviation& operator=( const SUCAviation &rUCAviation )
	{
		if ( &rUCAviation != this )
		{
			aircrafts = rUCAviation.aircrafts;
			szParadropSquadName = rUCAviation.szParadropSquadName;
			nParadropSquadCount = rUCAviation.nParadropSquadCount;
			nRelaxTime = rUCAviation.nRelaxTime;
			vAppearPoints = rUCAviation.vAppearPoints;

			Validate();
		}
		return *this;
	}

	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;

		saver.Add( "Aircrafts", &aircrafts );
		saver.Add( "ParadropSoldierName", &szParadropSquadName );
		saver.Add( "ParadropSoldierCount", &nParadropSquadCount );
		saver.Add( "RelaxTime", &nRelaxTime );
		saver.Add( "AppearPoints", &vAppearPoints );
		
		if ( saver.IsReading() )
		{
			Validate();
		}
		
		return 0;
	}

	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;

		saver.Add( 1, &aircrafts );
		saver.Add( 2, &szParadropSquadName );
		saver.Add( 3, &nRelaxTime );
		saver.Add( 4, &vAppearPoints );
		saver.Add( 5, &nParadropSquadCount );
		
		if ( saver.IsReading() )
		{
			Validate();
		}

		return 0;
	}

	void Validate()
	{
		while ( aircrafts.size() < AT_COUNT )
		{
			aircrafts.push_back( SUCAircraft() );
		}
	}
};
struct SUnitCreation
{
	SUCAviation aviation;											//самолеты
	std::string szPartyName;									// имя страны
	
	SUnitCreation() {}
	SUnitCreation( const SUnitCreation &rUnitCreation )
		: aviation( rUnitCreation.aviation ), szPartyName( rUnitCreation.szPartyName ) {}
	SUnitCreation& operator=( const SUnitCreation &rUnitCreation )
	{
		if ( &rUnitCreation != this )
		{
			aviation = rUnitCreation.aviation;
			szPartyName = rUnitCreation.szPartyName;
		}
		return *this;
	}

	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;

		saver.Add( "Aviation", &aviation );
		saver.Add( "PartyName", &szPartyName );
		return 0;
	}

	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;

		saver.Add( 1, &aviation );
		saver.Add( 7, &szPartyName );

		return 0;
	}
};
struct SUnitCreationInfo
{
	enum UNIT_TYPE
	{
		UT_PLAYER		= 0,
		UT_ENEMY		= 1,
		UT_NEUTRAL	= 2,
		UT_COUNT		= 3,
	};

	static const char* DEFAULT_AIRCRAFT_NAME[SUCAviation::AT_COUNT];
	static const char* DEFAULT_PARADROP_SOLDIER_NAME;
	static const char* DEFAULT_PARTY_NAME;
	static const int   DEFAULT_RELAX_TIME;

	std::vector<SUnitCreation> units;

	SUnitCreationInfo()	{ Validate(); }
	SUnitCreationInfo( const SUnitCreationInfo &rUnitCreationInfo )
		: units( rUnitCreationInfo.units ) { Validate(); }
	SUnitCreationInfo& operator=( const SUnitCreationInfo &rUnitCreationInfo )
	{
		if ( &rUnitCreationInfo != this )
		{
			units = rUnitCreationInfo.units;
			Validate();
		}
		return *this;
	}

	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;

		saver.Add( "Units", &units );
		if ( saver.IsReading() )
		{
			Validate();
		}
		
		return 0;
	}

	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;

		saver.Add( 1, &units );
		if ( saver.IsReading() )
		{
			Validate();
		}

		return 0;
	}

	void Validate();
};

#endif // __FMT__UNIT_CREATION__H__

/**
struct SUCMine
{
	std::string szAntiPersonnelName;			//Противотехотная мина
	std::string szAntiTankName;						//Противотанковая мина

	SUCMine() {}
	SUCMine( const std::string &rszAntiPersonnelName, const std::string &rszAntiTankName )
		: szAntiPersonnelName( rszAntiPersonnelName ), szAntiTankName( rszAntiTankName ) {}
	SUCMine( const SUCMine &rUCMine )
		: szAntiPersonnelName( rUCMine.szAntiPersonnelName ), szAntiTankName( rUCMine.szAntiTankName ) {}
	SUCMine& operator=( const SUCMine &rUCMine )
	{
		if ( &rUCMine != this )
		{
			szAntiPersonnelName = rUCMine.szAntiPersonnelName;
			szAntiTankName = rUCMine.szAntiTankName;
		}
		return *this;
	}

	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;

		saver.Add( "AntiPersonnel", &szAntiPersonnelName );
		saver.Add( "AntiTank", &szAntiTankName );
		
		return 0;
	}
	
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;

		saver.Add( 1, &szAntiPersonnelName );
		saver.Add( 2, &szAntiTankName );
		
		return 0;
	}
};
/**/
