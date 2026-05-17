// Reaction.h: interface for the CReaction class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_REACTION_H__C9D8977B_A116_4A9B_93A3_8EBE426CA74D__INCLUDED_)
#define AFX_REACTION_H__C9D8977B_A116_4A9B_93A3_8EBE426CA74D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

interface IMessageReactionB2;
interface ICustomCheck;

#include "IMessageReaction.h"
#include "..\LuaLib\Script.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 1 check (branches) and sequience of atim reactions for each branch
class CMessageReactionB2 : public IMessageReactionB2
{
	OBJECT_COMPLETE_METHODS( CMessageReactionB2 );
	DECLARE_SERIALIZE;

	typedef std::vector< CPtr<IMessageReactionB2> > CMessageSequence;
	typedef std::unordered_map<int/*custom check return*/, CMessageSequence> CMessageSequences;

	CPtr<ICustomCheck> pCheck;
	CMessageSequences branches;
	CMessageSequence commonBefore;					// always run and before any branch
	CMessageSequence commonAfter;						// always run and after any branch

	bool Execute( const CMessageSequence *pToExecute, interface IScreen *pScreen, class Script *pScript ) const;
public:
	CMessageReactionB2() {  }
	CMessageReactionB2( int TEST );
	virtual bool STDCALL Execute( interface IScreen *pScreen, class Script *pScript ) const;
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "Check", &pCheck );
		saver.Add( "Branches", &branches );
		saver.Add( "CommonBefore", &commonBefore );
		saver.Add( "CommonAfter", &commonAfter );
		return 0;
	}
	void AddCommonBefore( IMessageReactionB2 *pReaction ) { commonBefore.push_back( pReaction ); }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Atom EMART_SET_GLOBAL_VAR
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CARSetGlobalVar : public IMessageReactionB2
{
	OBJECT_COMPLETE_METHODS( CARSetGlobalVar );
	DECLARE_SERIALIZE;
	std::string szVarName;
	std::string szVarValue;
public:
	CARSetGlobalVar () {  }
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "Param1", &szVarName );
		saver.Add( "Param2", &szVarValue );
		return 0;
	}
	virtual bool STDCALL Execute( interface IScreen *pScreen, class Script *pScript ) const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Atom EMART_REMOVE_GLOBAL_VAR
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CARRemoveGlobalVar : public IMessageReactionB2
{
	OBJECT_COMPLETE_METHODS( CARRemoveGlobalVar );
	DECLARE_SERIALIZE;
	std::string szVarName;
public:
	CARRemoveGlobalVar() {  }
	CARRemoveGlobalVar( int TEST )
	{
		szVarName = "Multiplayer.NumPlayers";
	}
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "Param1", &szVarName );
		return 0;
	}
	virtual bool STDCALL Execute( interface IScreen *pScreen, class Script *pScript ) const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CARSendMessage : public IMessageReactionB2 
{
	OBJECT_COMPLETE_METHODS( CARSendMessage );
	DECLARE_SERIALIZE;
	std::string szMessageID;
	std::string szParam;
	int nParam;
public:
	CARSendMessage( const std::string &_szMessageID, const std::string &_szParam, const int _nParam )
		: szMessageID( _szMessageID ), szParam( _szParam ), nParam( _nParam )
	{
	}

	CARSendMessage() {  }

	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "MessageID", &szMessageID );
		saver.Add( "StringParam", &szParam );
		saver.Add( "IntParam", &nParam );
		return 0;
	}
	virtual bool STDCALL Execute( interface IScreen *pScreen, class Script *pScript ) const;	
};

#endif // !defined(AFX_REACTION_H__C9D8977B_A116_4A9B_93A3_8EBE426CA74D__INCLUDED_)
