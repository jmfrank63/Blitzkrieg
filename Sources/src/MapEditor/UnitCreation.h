#if !defined(__UnitCreationInfo__MANIPULATOR__)
#define __UnitCreationInfo__MANIPULATOR__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\Formats\FmtMap.h"
#include "..\Misc\Manipulator.h"

#include "..\AILogic\UnitCreation.h"

class CUCHelper
{
	static const char DEFAULT_AVIATION_NAME[];
	static const char DEFAULT_SQUADS_NAME[];
	static const char DEFAULT_UNITS_NAME[];
	bool isInitialized;

public:
	std::list<std::string> aircraftsList;
	std::list<std::string> squadsList;
	std::list<std::string> partyList;

	std::vector<CUnitCreation::SPartyDependentInfo> partyDependentInfo;

	CUCHelper() : isInitialized( false ) {}
	
	bool IsInitialized(){ return isInitialized; }
	void Initialize();
};

class CMutableUCAircraft : public SUCAircraft
{
	void MutateTo() {}
	void MutateFrom() {}

public:
	friend class CUCAircraftManipulator;

	CMutableUCAircraft() { MutateTo(); }
	CMutableUCAircraft( const SUCAircraft &rUCAircraft )
		:	SUCAircraft( rUCAircraft ) { MutateTo(); }
	SUCAircraft& Mutate() { MutateFrom(); return *this; }
	
	virtual IManipulator* GetManipulator();
};

class CMutableUCAviation : public SUCAviation
{
	static const char* AIRCRAFT_TYPE_NAMES[SUCAviation::AT_COUNT];
	std::vector<CMutableUCAircraft> mutableAircrafts;
	void MutateTo()
	{
		mutableAircrafts.clear();
		for ( std::vector<SUCAircraft>::const_iterator aircraftIterator = aircrafts.begin(); aircraftIterator != aircrafts.end(); ++aircraftIterator )
		{
			mutableAircrafts.push_back( CMutableUCAircraft( *aircraftIterator ) );
		}
	}
	void MutateFrom()
	{
		aircrafts.clear();
		for ( std::vector<CMutableUCAircraft>::iterator mutableAircraftIterator = mutableAircrafts.begin(); mutableAircraftIterator != mutableAircrafts.end(); ++mutableAircraftIterator )
		{
			aircrafts.push_back( mutableAircraftIterator->Mutate() );
		}
	}

public:	
	friend class CUCAviationManipulator;

	CMutableUCAviation() { MutateTo(); }
	CMutableUCAviation( const SUCAviation &rUCAviation )
		:	SUCAviation( rUCAviation ) { MutateTo(); }
	SUCAviation& Mutate() { MutateFrom(); return *this; }

	virtual IManipulator* GetManipulator();
};

class CMutableUnitCreation : public SUnitCreation
{
	CMutableUCAviation mutableAviation;
	void MutateTo()
	{
		mutableAviation = CMutableUCAviation( aviation ); 
	}
	void MutateFrom()
	{
		aviation = mutableAviation.Mutate();
	}

public:
	friend class CUnitCreationManipulator;
	friend class CTemplateEditorFrame;

	CMutableUnitCreation() { MutateTo(); }
	CMutableUnitCreation( const SUnitCreation &rUnitCreation )
		:	SUnitCreation( rUnitCreation ) { MutateTo(); }
	SUnitCreation& Mutate() { MutateFrom(); return *this; }
	
	virtual IManipulator* GetManipulator();
};

class CMutableUnitCreationInfo : public SUnitCreationInfo
{
	static const char* UNIT_TYPE_NAMES[3];
	std::vector<CMutableUnitCreation> mutableUnits;

	void MutateTo()
	{
		mutableUnits.clear();
		for ( std::vector<SUnitCreation>::const_iterator unitCreationIterator = units.begin(); unitCreationIterator != units.end(); ++unitCreationIterator )
		{
			mutableUnits.push_back( CMutableUnitCreation( *unitCreationIterator ) );
		}
	}
	void MutateFrom()
	{
		units.clear();
		for ( std::vector<CMutableUnitCreation>::iterator mutabelUnitCreationIterator = mutableUnits.begin(); mutabelUnitCreationIterator != mutableUnits.end(); ++mutabelUnitCreationIterator )
		{
			units.push_back( mutabelUnitCreationIterator->Mutate() );
		}
	}

public:
	friend class CUnitManipulator;
	friend class CUnitCreationInfoManipulator;
	friend class CTemplateEditorFrame;

	CMutableUnitCreationInfo() { MutateTo(); }
	CMutableUnitCreationInfo( const SUnitCreationInfo &rUnitCreationInfo )
		:	SUnitCreationInfo( rUnitCreationInfo ) { MutateTo(); }
	SUnitCreationInfo& Mutate() { MutateFrom(); return *this; }

	virtual IManipulator* GetManipulator();
	void Resize( int nNewSize )
	{
		if ( mutableUnits.size() != nNewSize )
		{
			mutableUnits.resize( nNewSize );
		}
		if ( units.size() != nNewSize )
		{
			units.resize( nNewSize );
		}
	}

	void MutableValidate();
};

class CUCAircraftManipulator : public CManipulator
{
	OBJECT_MINIMAL_METHODS( CUCAircraftManipulator );
	CMutableUCAircraft *pMutableObject;

public:
	CUCAircraftManipulator();

	void SetName( const variant_t &value );		
	void GetName( variant_t *pValue, int nIndex = -1 );	

	void SetFormationSize( const variant_t &value );		
	void GetFormationSize( variant_t *pValue, int nIndex = -1 );	

	void SetCount( const variant_t &value );		
	void GetCount( variant_t *pValue, int nIndex = -1 );	

	inline void SetObject( CMutableUCAircraft *_pMutableObject ) { pMutableObject = _pMutableObject; }
};

class CUCAviationManipulator : public CManipulator
{
	OBJECT_MINIMAL_METHODS( CUCAviationManipulator );
	CMutableUCAviation *pMutableObject;

public:
	CUCAviationManipulator();

	void GetAircraft00( variant_t *pValue, int nIndex = -1 );
	void GetAircraft01( variant_t *pValue, int nIndex = -1 );
	void GetAircraft02( variant_t *pValue, int nIndex = -1 );
	void GetAircraft03( variant_t *pValue, int nIndex = -1 );
	void GetAircraft04( variant_t *pValue, int nIndex = -1 );

	void SetParadropSquadName( const variant_t &value );		
	void GetParadropSquadName( variant_t *pValue, int nIndex = -1 );	

	void SetParadropSquadCount( const variant_t &value );		
	void GetParadropSquadCount( variant_t *pValue, int nIndex = -1 );	

	void SetRelaxTime( const variant_t &value );		
	void GetRelaxTime( variant_t *pValue, int nIndex = -1 );	

	void SetAppearPoints( const variant_t &value );		
	void GetAppearPoints( variant_t *pValue, int nIndex = -1 );	

	inline void SetObject( CMutableUCAviation *_pMutableObject ) { pMutableObject = _pMutableObject; }
};

class CUnitCreationManipulator : public CManipulator
{
	OBJECT_MINIMAL_METHODS( CUnitCreationManipulator );
	CMutableUnitCreation *pMutableObject;

public:
	CUnitCreationManipulator();
	void GetAviation( variant_t *pValue, int nIndex = -1 );	

	void SetPartyName( const variant_t &value );		
	void GetPartyName( variant_t *pValue, int nIndex = -1 );	

	inline void SetObject( CMutableUnitCreation *_pMutableObject ) { pMutableObject = _pMutableObject; }
};

class CUnitCreationInfoManipulator : public CManipulator
{
	OBJECT_MINIMAL_METHODS( CUnitCreationInfoManipulator );
	CMutableUnitCreationInfo *pMutableObject;

public:
	CUnitCreationInfoManipulator();
	void GetUnits( variant_t *pValue, int nIndex = -1 );	
	inline void SetObject( CMutableUnitCreationInfo *_pMutableObject ) { pMutableObject = _pMutableObject; }
};
#endif // !defined(__UnitCreationInfo__MANIPULATOR__)
