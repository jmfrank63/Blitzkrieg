#ifndef __INPUTBINDER_H__
#define __INPUTBINDER_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "InputAPI.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CBind;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** binds config struct
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SBindsConfig
{
	// one control power configuration
	struct SControlPower
	{
		std::string szControlName;					// control's name
		float fPower;												// power (multiplier) for this control
		//
		int operator&( IDataTree &ss )
		{
			CTreeAccessor saver = &ss;
			saver.Add( "Control", &szControlName );
			saver.Add( "Power", &fPower );
			return 0;
		}
		bool operator==( const SControlPower &p2 )
		{
			return szControlName == p2.szControlName;
		}
	};
	// bind section
	struct SBindSection 
	{
		// one command binding
		struct SCommandBind
		{
			std::string szName;								// command name
			std::string szBindType;						// bind type: 'event down', 'event up', 'slider plus', 'slider minus'
			std::vector<std::string> controls;// controls names
			//
			int operator&( IDataTree &ss )
			{
				CTreeAccessor saver = &ss;
				saver.Add( "Name", &szName );
				saver.Add( "Type", &szBindType );
				saver.Add( "Controls", &controls );
				return 0;
			}
		};
		//
		std::string szName;									// bind section name
		std::vector<SCommandBind> commands;	// commands binds
		//
		int operator&( IDataTree &ss )
		{
			CTreeAccessor saver = &ss;
			saver.Add( "Name", &szName );
			saver.Add( "Commands", &commands );
			return 0;
		}
	};
	//
	std::vector<SControlPower> powers;		// control powers
	std::vector<std::string> dblclks;			// double click controls
	std::vector<std::string> syscmds;			// system commands
	std::vector<SBindSection> sections;		// bind sections
	//
	void Clear() { powers.clear(); dblclks.clear(); syscmds.clear(); sections.clear(); }
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "Powers", &powers );
		saver.Add( "DoubleClicks", &dblclks );
		saver.Add( "SysCommands", &syscmds );
		saver.Add( "Sections", &sections );
		return 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** binder support structures: combo
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum ESetCompare
{
	SET_COMPARE_SUBSET,
	SET_COMPARE_SUPERSET,
	SET_COMPARE_EQUAL,
	SET_COMPARE_UNRELATED
};
class CCombo : public CTRefCount<IRefCount>
{
	typedef std::vector<const CControl*> CControlsPtrList;
	typedef std::list<CBind*> CBindsPtrList;
	typedef std::list< CPtr<CCombo> > CCombosList;
	struct SMapping
	{
		std::string szName;									// this mapping name
		CBindsPtrList binds;								// binds to notify about combo's state changes
		CCombosList suppressives;						// combos to suppress/unsuppress, if this combo formed/destroyed
		// suppress all 'suppressives'
		void Suppress( const int nAdd, const DWORD time );
	};
	typedef std::unordered_map<std::string, SMapping> CMappingsMap;
	//
	CMappingsMap mappings;								// all mapping for this combo
	SMapping *pMapping;										// current mapping
	CControlsPtrList controls;						// controls, which forms this combo
	EControlType eControlType;						// worst control in this combo controls
	int nSuppressCounter;									// number of combos, which suppressed this one
	bool bFormed;													// is combo formed or not?
	bool bLastNotifyFormed;								// was combo formed during last notify?
	float fPower;													// activation power (got from controls)
	int nBestParam;												//
	//
	bool IsBinded() const { return !pMapping->binds.empty(); }
	void NotifyBinds( const bool bFormedLocal, DWORD time, const bool bForced = false );
public:
	CCombo( const CControlsPtrList &_controls );
	virtual ~CCombo();
	// suppress this combo
	void Suppress( const int nSuppress, const DWORD time );
	// notify combo, that one of their controls have changed its state
	void NotifyControlStateChanged( const bool bActivated, const DWORD time, const int nParam );
	// change current mapping section
	void ChangeMappingSection( const std::string &szMapping );
	// check, is this combo formed?
	const bool IsFormed() const { return bFormed; }
	// is this combo completelly empty?
	const bool IsEmpty() const
	{
		for ( CMappingsMap::const_iterator it = mappings.begin(); it != mappings.end(); ++it )
		{
			if ( !it->second.binds.empty() ) 
				return false;
		}
		return true;
	}
	//
	const ESetCompare Compare( const std::vector<const CControl*> &controls ) const;
	const int GetNumControls() const { return controls.size(); }
	void AddSuppressive( CCombo *pCombo ) 
	{ 
		if ( std::find(pMapping->suppressives.begin(), pMapping->suppressives.end(), pCombo) == pMapping->suppressives.end() )
			pMapping->suppressives.push_back( pCombo );
	}
	void RemoveSuppressive( CCombo *pCombo, bool bFromAllMappings ) 
	{ 
		if ( bFromAllMappings ) 
		{
			for ( CMappingsMap::iterator it = mappings.begin(); it != mappings.end(); ++it )
				it->second.suppressives.remove( pCombo ); 
		}
		else
			pMapping->suppressives.remove( pCombo ); 
	}
	void AddBind( CBind *pBind );
	void RemoveBind( CBind *pBind ) { pMapping->binds.remove( pBind ); }
	//
	bool Visit( IInputVisitor *pVisitor )
	{
		if ( pMapping ) 
		{
			for ( CBindsPtrList::iterator it = pMapping->binds.begin(); it != pMapping->binds.end(); ++it )
			{
				if ( pVisitor->VisitBind(*it) == false )
					return false;
			}
		}
		return true;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** binder support structures: bind types
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CBind
{
protected:
	SCommand *pCommand;
public:
	CBind( SCommand *_pCommand ) : pCommand( _pCommand ) {  }
	virtual void NotifyComboStateChanged( const bool bFormed, const float fPower, const EControlType eControlType, const DWORD time, const int nParam ) = 0;
	virtual const EInputBindActivationType GetType() const = 0;
	//
	const SCommand* GetCommand() const { return pCommand; }
	bool Visit( IInputVisitor *pVisitor ) { return pVisitor->VisitCommand( pCommand ); }
};
class CBindEventForm : public CBind
{
public:
	CBindEventForm( SCommand *_pCommand ) : CBind( _pCommand ) {  }
	virtual void NotifyComboStateChanged( const bool bFormed, const float fPower, const EControlType eControlType, const DWORD time, const int nParam );
	virtual const EInputBindActivationType GetType() const { return INPUT_BIND_ACTIVATION_TYPE_EVENT_DOWN; }
};
class CBindEventDestroy : public CBind
{
public:
	CBindEventDestroy( SCommand *_pCommand ) : CBind( _pCommand ) {  }
	virtual void NotifyComboStateChanged( const bool bFormed, const float fPower, const EControlType eControlType, const DWORD time, const int nParam );
	virtual const EInputBindActivationType GetType() const { return INPUT_BIND_ACTIVATION_TYPE_EVENT_UP; }
};
class CBindSliderPlus : public CBind
{
public:
	CBindSliderPlus( SCommand *_pCommand ) : CBind( _pCommand ) {  }
	virtual void NotifyComboStateChanged( const bool bFormed, const float fPower, const EControlType eControlType, const DWORD time, const int nParam );
	virtual const EInputBindActivationType GetType() const { return INPUT_BIND_ACTIVATION_TYPE_SLIDER_PLUS; }
};
class CBindSliderMinus : public CBind
{
public:
	CBindSliderMinus( SCommand *_pCommand ) : CBind( _pCommand ) {  }
	virtual void NotifyComboStateChanged( const bool bFormed, const float fPower, const EControlType eControlType, const DWORD time, const int nParam );
	virtual const EInputBindActivationType GetType() const { return INPUT_BIND_ACTIVATION_TYPE_SLIDER_MINUS; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** binder support structures: command
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCommand
{
	std::stack<int> eventIDs;							// stack of all event IDs, which was assigned to this command
	int nEventID;													// event ID to send in the case of this command
	CAccumulator accumulator;							// value accumulator
	std::list<const CBind*> binds;				// all binds of this command
	class CInputBinder *pInput;						// input hook to send messages
	bool bSystem;													// is this command system?
	int nRegisterRefCount;								// how many 'users' uses this command
	// CRAP{ for test purposes only
	std::string szName;
	// CRAP}
	//
	SCommand() : nEventID( -1 ), pInput( 0 ), bSystem( false ), nRegisterRefCount( 0 ) {  }
	SCommand( class CInputBinder *_pInput ) : nEventID( -1 ), pInput( _pInput ), bSystem( false ), nRegisterRefCount( 0 ) {  }
	// registration/configuration
	void SetEventID( const int nID ) 
	{ 
		//NI_ASSERT_T( ((nEventID != nID) && (nRegisterRefCount > 0)) || (nRegisterRefCount == 0), NStr::Format("Changing event ID from %d to %d, but command already in use!", nID, nEventID) );
		nEventID = nID; 
	}
	const int Register( const bool bRegister ) 
	{ 
		if ( bRegister ) 
		{
			eventIDs.push( nEventID );
			++nRegisterRefCount;
		}
		else
		{
			if ( !eventIDs.empty() ) 
			{
				nEventID = eventIDs.top();
				eventIDs.pop();
			}
			--nRegisterRefCount;
		}
		return nRegisterRefCount; 
	}
	bool IsRegistered() const { return nRegisterRefCount > 0; }
	void SetSystem() { bSystem = true; }
	// activation
	void ActivateEvent( const DWORD time, const int nParam );
	void ActivateSlider( const bool bActivate, const float fPower, EControlType eControlType, const DWORD time, const int nParam );
	// slider data retrieving
	const float GetAccValue( const DWORD time ) const { return accumulator.Sample( time ); }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** main input binder class
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CInputBinder : public CInputAPI
{
	OBJECT_SERVICE_METHODS( CInputBinder );
	//
	typedef std::unordered_map<std::string, SCommand> CCommandsMap;
	CCommandsMap commands;
	SBindsConfig config;									// binds config (for serialization)
	std::string szCurrentMapping;					// current mapping section
	bool bMappingChanged;									// mapping was changed outside (by the program)
	//
	SCommand* GetCommand( const std::string &szName )
	{
		CCommandsMap::iterator pos = commands.find( szName );
		if ( pos != commands.end() ) 
			return &( pos->second );
		commands.insert( CCommandsMap::value_type(szName, SCommand(this)) );
		SCommand *pCommand = &( commands[szName] );
		pCommand->szName = szName;
		return pCommand;
	}
	//
	bool AddBindLocal( const SBindsConfig::SBindSection::SCommandBind &bind );
	// configuring
	void SetSystemCommand( const std::string &szName ) { GetCommand(szName)->SetSystem(); }
	//
	void TransformBind( SBindsConfig::SBindSection::SCommandBind *pRes, const IInputBind *pBind );
	void TransformBind( IInputBind *pRes, const SBindsConfig::SBindSection::SCommandBind *pBind );
public: 
	CInputBinder();
	virtual ~CInputBinder() {  }
	//
	virtual bool STDCALL SerializeConfig( IDataTree *pSS );
	virtual bool STDCALL IsChanged() const { return bMappingChanged; }
	virtual void STDCALL Repair( IDataTree *pSS, const bool bToDefault );
	// binds
	virtual void STDCALL AddBind( const IInputBind *pBind );
	virtual void STDCALL RemoveBind( const IInputBind *pBind );
	virtual void STDCALL SetBindSection( const char *pszSectionName );
	// sliders
	virtual IInputSlider* STDCALL CreateSlider( const char *pszName, const float fPower = 1.0f );
	// commands registration
	virtual void STDCALL RegisterCommand( const char *pszName, const int nEventID );
	virtual void STDCALL UnRegisterCommand( const char *pszName );
	//
	virtual int STDCALL operator&( IStructureSaver &ss );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __INPUTBINDER_H__
