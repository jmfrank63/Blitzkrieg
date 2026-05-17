// MessageReactions.h: interface for the CMessageReactions class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MESSAGEREACTIONS_H__E9435A45_ACAE_4421_9CBE_B4BE9882459B__INCLUDED_)
#define AFX_MESSAGEREACTIONS_H__E9435A45_ACAE_4421_9CBE_B4BE9882459B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\LuaLib\Script.h"

interface IMessageReactionB2;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// class contains message reactions
// that may be launched.
// Message Reaction is a set basic actions 
class CMessageReactions 
{
	DECLARE_SERIALIZE
	
	typedef std::unordered_map<std::string,CObj<IMessageReactionB2> > CReactions;
	CReactions reactions;

	// script that does all complex checks and complex behaviour
	Script script;
	bool bScriptPresent;														// if true then script is present

public:
	CMessageReactions() {  }
	CMessageReactions( int TEST );
	void Execute( const std::string &szReactionKey, interface IScreen *pScreen );
	void Load( const std::string &szResourceName );
	int operator&( IDataTree &ss );
	void Register( const std::string &szReactionKey, IMessageReactionB2 *pReaction );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // !defined(AFX_MESSAGEREACTIONS_H__E9435A45_ACAE_4421_9CBE_B4BE9882459B__INCLUDED_)
