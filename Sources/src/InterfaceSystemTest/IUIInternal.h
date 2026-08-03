#ifndef _IBackground_h_Included_
#define _IBackground_h_Included_

#include "Interface.h"
#include "../GameTT/iMission.h"
enum EUIStateManipulatorID
{
	EUISM_PS_MOVETO = 1,
	EUISM_RUN_REACTION = 2,
	EUISM_PS_MOVETO_COMMAND = EUISM_PS_MOVETO,
	EUISM_RUN_REACTION_COMMAND = EUISM_RUN_REACTION
};
struct SBUIMessage
{
	std::string szMessageID;							// message ID.
	std::string szParam;									// string parameter
	int nParam;														// int parameter
	
	SBUIMessage() {  }
	SBUIMessage( const std::string &_szMessageID ) : szMessageID( _szMessageID ), nParam( 0 ) { }
	SBUIMessage( const std::string &_szMessageID, const std::string &_szParam ) 
		: szMessageID( _szMessageID ), szParam( _szParam ), nParam( 0 ) { }
	SBUIMessage( const std::string &_szMessageID, const std::string &_szParam, const int _nParam ) 
		: szMessageID( _szMessageID ), szParam( _szParam ), nParam( _nParam ) { }
	
	int operator&( IStructureSaver &ss )
	{
		NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
		return 0;
	}
};
struct SUIStateCommand
{
	int nCommandID;													// command ID

	std::string szParam1;										// 1st string parameter
	std::string szParam2;										// 2nd string parameter 

	CVec2 vParam;														// vector parameter
	WORD dwParam1;													// 1st generic parameter

	SUIStateCommand() {  }

	SUIStateCommand( int TEST )
	{
		nCommandID = EUISM_RUN_REACTION_COMMAND;													// command ID
		szParam1 = "ReactionKey1";												// window on what to perform this command sequence

		vParam = VNULL2;														// vector parameter
		dwParam1 = 0;													// 1st generic parameter
	}

	SUIStateCommand( const int _nCommandID, const std::string &_szParam1, const std::string &_szParam2, const CVec2 &_vParam, const DWORD _dwParam1 )
		: nCommandID( _nCommandID ), szParam1( _szParam1 ), szParam2( _szParam2 ), vParam( _vParam ), dwParam1( _dwParam1 )
	{
		}

	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "ID", &nCommandID );
		saver.Add( "String1", &szParam1 );
		saver.Add( "String2", &szParam2 );
		saver.Add( "Vector", &vParam );
		saver.Add( "DWORD", &dwParam1 );
		return 0;
	}

	int operator&( IStructureSaver &ss )
	{
		NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
		return 0;
	}
};
struct SUIStateMoveToCommand : public SUIStateCommand 
{
	IManipulator * GetManipulator() { return 0; }
};
struct SUICommandSequence
{
	DECLARE_SERIALIZE;
public:
	typedef std::vector<SUIStateCommand> CCmds;
	CCmds cmds;
	bool bReversable;													// this effect can be undone
	int operator&( IDataTree &ss );
};

interface IEffectorCommand : public IRefCount
{
	virtual interface IUIEffector * STDCALL Configure( const struct SUIStateCommand &cmd, interface IScreen *pScreen  ) = 0;
};
template <class TEffector, int NInterfaceTypeID>
class CEffectorCommandBase : public IEffectorCommand
{
protected:
	virtual ~CEffectorCommandBase() {  }
public:
	virtual IUIEffector * STDCALL Configure( const struct SUIStateCommand &cmd, interface IScreen *pScreen )
	{
		TEffector *pEffector = CreateObject<TEffector>( NInterfaceTypeID );
		pEffector->Configure( cmd, pScreen );
		return pEffector;
	}
};

#endif //_IBackground_h_Included_
