#ifndef __MESSAGEREACTIONINTERNAL_H__
#define __MESSAGEREACTIONINTERNAL_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
#include "MessageReaction.h"
#include "CustomMessageReaction.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EMessageAtomicReactionType
{
	EMART_PAUSE_GAME											= 1,
	EMART_MESSAGE_TO_INPUT								= 2,
	EMART_MESSAGE_TO_MAINLOOP							= 3,
	EMART_SET_GLOBAL_VAR									= 4,
	EMART_REMOVE_GLOBAL_VAR								= 5,
	EMART_CUSTOM_REACTION									= 6,	
	EMART_NOP															= 7,	// no operation, message is processed
	EMART_SET_TEXT_TO_WINDOW							= 8,
	EMART_NOP_DONT_PROCESSED							= 9,
	EMART_SET_TEXT_TO_WINDOW_FROM_GLOBALVAR = 10,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum ECustomCheckType
{
	ECCT_BOOL_GLOBAL_VARS_ENUM							= 1, // GlobalVar1 << 0 | GlobalVar2 << 1 | ....
	ECCT_BOOL_GLOBAL_VAR_FIRST							= 2, // the number of global var is set to true
	
	//ECCT_MULTIPLAYER_AND_ESC_MENU_SHOWN		= 2,

};
enum ECustomCheckReturn
{
	ECCR_COMMON														= 1<<8 | 1,
	ECCR_COMMON_AFTER											= 1<<8 | 1<<1,
	ECCR_DEFAULT													= 1<<8 | 1<<2,
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPairHash
{
	int operator()( const std::pair<int,int> &incomingPair ) const
	{
		return incomingPair.first + incomingPair.second;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// STRUCTURES FOR LOAD
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMessageAtomReactionForLoad
{
	std::string szType;
	std::string szParam1;
	std::string szParam2;

	int operator&( IDataTree &ss )
	{
		CTreeAccessor tree = &ss;
		tree.Add( "Type", &szType );
		tree.Add( "Param1", &szParam1 );
		tree.Add( "Param2", &szParam2 );
		return 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::vector<SMessageAtomReactionForLoad> CAtomReactionsSequenceForLoad;
// for every return there is vector of reactions
typedef std::unordered_map<std::string/*CustomCheckReturn*/, CAtomReactionsSequenceForLoad > CAtomReactionSequencesForLoad;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMessageReactionForLoad
{
	// incoming message: message ID and message nParam
	typedef std::pair<std::string/*messageID*/, std::string /*messageParam*/> SIncomingMessageForLoad;
	SIncomingMessageForLoad incomingMessage;

	// custom check: Custom Check Name and Custom Check Params
	typedef std::vector<std::string> CCustomCheckParamsForLoad;
	typedef std::pair<std::string/*custom check name*/, CCustomCheckParamsForLoad> CCustomCheckForLoad;
	CCustomCheckForLoad customCheck;
	CAtomReactionSequencesForLoad atomReactions;

	int operator&( IDataTree &ss )
	{
		CTreeAccessor tree = &ss;
		tree.Add( "IncomingMessage", &incomingMessage );
		tree.Add( "CustomCheck", &customCheck );
		tree.Add( "Reactions", &atomReactions );
		return 0;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CLoadHelper 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CLoadHelper : public ILoadHelper, public std::unordered_map<std::string,int>
{
	OBJECT_COMPLETE_METHODS( CLoadHelper );
public:
	virtual int STDCALL Get( const std::string &szLoaded )
	{
		const_iterator it = find( szLoaded );
		if ( it != end() )
			return it->second;
		return NStr::ToInt( szLoaded );
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Atom EMART_SET_TEXT_TO_WINDOW_FROM_GLOBALVAR
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMessageAtomReactionSetWindowTextFromGlobalVar : public IMessageReaction
{
	OBJECT_COMPLETE_METHODS( CMessageAtomReactionSetWindowTextFromGlobalVar );
	DECLARE_SERIALIZE;
	int nWindowID;
	std::string szTextKey;
	
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	std::string szWindowID;
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)

public:
	CMessageAtomReactionSetWindowTextFromGlobalVar () {  }
	// create and transform reaction
	CMessageAtomReactionSetWindowTextFromGlobalVar ( const SMessageAtomReactionForLoad &reaction, IMessageLinkContainer *pHelperContainer )
	{
		ILoadHelper * pHelper = pHelperContainer->GetLoadHelper( ELH_MESSAGE_TO_INPUT_PARAM2 );
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		szWindowID = reaction.szParam1;
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		nWindowID = pHelper->Get( reaction.szParam1 );
		szTextKey = reaction.szParam2;
	}
	virtual bool STDCALL Execute() ;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Atom EMART_SET_TEXT_TO_WINDOW
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMessageAtomReactionSetWindowText : public IMessageReaction
{
	OBJECT_COMPLETE_METHODS( CMessageAtomReactionSetWindowText );
	DECLARE_SERIALIZE;
	int nWindowID;
	std::string szTextKey;
	
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	std::string szWindowID;
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)

public:
	CMessageAtomReactionSetWindowText () {  }
	// create and transform reaction
	CMessageAtomReactionSetWindowText ( const SMessageAtomReactionForLoad &reaction, IMessageLinkContainer *pHelperContainer )
	{
		ILoadHelper * pHelper = pHelperContainer->GetLoadHelper( ELH_MESSAGE_TO_INPUT_PARAM2 );
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		szWindowID = reaction.szParam1;
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		nWindowID = pHelper->Get( reaction.szParam1 );
		szTextKey = reaction.szParam2;
	}
	virtual bool STDCALL Execute() ;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Atom EMART_SET_GLOBAL_VAR
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMessageAtomReactionSetGlobalVar : public IMessageReaction
{
	OBJECT_COMPLETE_METHODS( CMessageAtomReactionSetGlobalVar );
	DECLARE_SERIALIZE;
	std::string szVarName;
	std::string szVarValue;
public:
	CMessageAtomReactionSetGlobalVar () {  }
	// create and transform reaction
	CMessageAtomReactionSetGlobalVar( const SMessageAtomReactionForLoad &reaction )
	{
		szVarName = reaction.szParam1;
		szVarValue = reaction.szParam2;
	}
	virtual bool STDCALL Execute();
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Atom EMART_REMOVE_GLOBAL_VAR
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMessageAtomReactionRemoveGlobalVar : public IMessageReaction
{
	OBJECT_COMPLETE_METHODS( CMessageAtomReactionRemoveGlobalVar );
	DECLARE_SERIALIZE;
	std::string szVarName;
public:
	CMessageAtomReactionRemoveGlobalVar() {  }
	// create and transform reaction
	CMessageAtomReactionRemoveGlobalVar( const SMessageAtomReactionForLoad &reaction )
	{
		szVarName = reaction.szParam1;
	}
	virtual bool STDCALL Execute();
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Atom EMART_PAUSE_GAME
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMessageAtomReactionPause : public IMessageReaction
{
	OBJECT_COMPLETE_METHODS( CMessageAtomReactionPause );
	DECLARE_SERIALIZE;
	int nPauseType;
	bool bPause;
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	std::string szPauseType;
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
public:
	CMessageAtomReactionPause() {  }
	// create and transform reaction
	CMessageAtomReactionPause( const SMessageAtomReactionForLoad &reaction, IMessageLinkContainer *pHelperContainer )
	{
		ILoadHelper * pHelper = pHelperContainer->GetLoadHelper( ELH_PAUSE_TYPE );
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		szPauseType = reaction.szParam1;
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		nPauseType = pHelper->Get( reaction.szParam1 );
		bPause = NStr::ToInt( reaction.szParam2 );
	}
	virtual bool STDCALL Execute();
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Atom EMART_CUSTOM_REACTION
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMessageAtomReactionCustom : public IMessageReaction
{
	OBJECT_COMPLETE_METHODS( CMessageAtomReactionCustom );
	DECLARE_SERIALIZE;
	std::string szCustomReactionName;
public:
	CMessageAtomReactionCustom() {  }
	// create and transform reaction
	CMessageAtomReactionCustom( const SMessageAtomReactionForLoad &reaction )
	{
		szCustomReactionName = reaction.szParam1;
	}
	virtual bool STDCALL Execute();
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Atom EMART_NOP
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMessageAtomReactionNOP : public IMessageReaction
{
	OBJECT_COMPLETE_METHODS( CMessageAtomReactionNOP );
public:
	CMessageAtomReactionNOP() {  }
	virtual bool STDCALL Execute() { return true; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Atom EMART_MESSAGE_TO_INPUT
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMessageAtomReactionMessageToInput : public IMessageReaction
{
	OBJECT_COMPLETE_METHODS( CMessageAtomReactionMessageToInput );
	DECLARE_SERIALIZE;
	int nEventID;
	int nParam;

#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	std::string szEventID;
	std::string szParam;
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
public:
	CMessageAtomReactionMessageToInput() {  }
	CMessageAtomReactionMessageToInput( const SMessageAtomReactionForLoad &reaction, IMessageLinkContainer *pHelperContainer )
	{
		ILoadHelper * pHelper = pHelperContainer->GetLoadHelper( ELH_MESSAGE_TO_INPUT_PARAM1 );
		ILoadHelper * pHelper2 = pHelperContainer->GetLoadHelper( ELH_MESSAGE_TO_INPUT_PARAM2 );
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		szEventID = reaction.szParam1 ;
		szParam = reaction.szParam2;
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		nEventID = pHelper->Get( reaction.szParam1 );
		nParam = pHelper2->Get( reaction.szParam2 );
	}
	virtual bool STDCALL Execute();
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Atom EMART_MESSAGE_TO_MAINLOOP
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMessageAtomReactionMessageToMainLoop : public IMessageReaction
{
	OBJECT_COMPLETE_METHODS( CMessageAtomReactionMessageToMainLoop );
	DECLARE_SERIALIZE;

	int nCommandID;
	std::string szParam;

#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	std::string szCommandID;
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
public:
	CMessageAtomReactionMessageToMainLoop() {  }
	CMessageAtomReactionMessageToMainLoop( const SMessageAtomReactionForLoad &reaction, IMessageLinkContainer *pHelperContainer  )
	{
		ILoadHelper * pHelper = pHelperContainer->GetLoadHelper( ELH_MESSAGE_TO_MAINLOOP_PARAM1 );
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		szCommandID = reaction.szParam1;
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		nCommandID = pHelper->Get( reaction.szParam1 );
		szParam = reaction.szParam2;
	}
	virtual bool STDCALL Execute();
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMessageReaction : public IMessageReaction
{
	OBJECT_COMPLETE_METHODS( CMessageReaction );
	DECLARE_SERIALIZE;

	typedef std::vector< CPtr<IMessageReaction> > CMessageSequence;
	typedef std::unordered_map<int/*custom check return*/, CMessageSequence> CMessageSequences;

	int nCustomCheckType;
	CCustomCheckParams customCheckParams;
	CMessageSequences atomReactions;

#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	std::string szCustomCheckType;
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)

	bool Execute( CMessageSequence *pToExecute );
public:
	CMessageReaction() {  }
	CMessageReaction( const SMessageReactionForLoad &loaded, IMessageLinkContainer *pHelpers );
	virtual bool STDCALL Execute(); 
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// message link. contains several reactions on specific message
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMessageLink : public IMessageLink
{
	OBJECT_COMPLETE_METHODS( CMessageLink );
	DECLARE_SERIALIZE;

	typedef std::pair<int/*nIncomingMessageID*/,int/*nParam*/> CIncomingMessage;
	typedef std::unordered_map< CIncomingMessage, CPtr<CMessageReaction>, SPairHash > CMessageReactions;
	CMessageReactions messageReactions;

#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	std::unordered_map<int, std::string> messagesForDebug;
	std::unordered_map<int, std::string> parametersForDebug;
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	
public:
	CMessageLink() {  }

	void Load( const std::string &szFileName, IMessageLinkContainer *pHelpers );
	virtual IMessageReaction* STDCALL Configure( const int nMessageID, const int nParam );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// contains all messages links
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMessageLinkContainer : public IMessageLinkContainer 
{
	OBJECT_COMPLETE_METHODS( CMessageLinkContainer );
	DECLARE_SERIALIZE;

	CPtr<CInterfaceScreenBase> pInterface;		// for temprorary storage

	typedef std::unordered_map<int/*eLinkID*/, CObj<IMessageLink> > CMessageLinks;
	CMessageLinks messageLinks;

	CCustomMessageReaction customReactions;
	// for simplyfing work ( work with ints instead of strings )
	typedef std::unordered_map<int/*ELoadHelperID*/, CObj<ILoadHelper> > CLoadHelpers;
	CLoadHelpers loadHelpers;
public:

	CMessageLinkContainer() {  }

	virtual void STDCALL SetInterface( class CInterfaceScreenBase * pInterface );
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );

	virtual void Clear();
	virtual void Init();
	
	virtual IMessageLink * STDCALL GetMessageLink( const enum EMessageLink eLinkID );
	virtual void STDCALL RegisterMessageLink ( IMessageLink *pMessageLink, const enum EMessageLink eLinkID );
	virtual void STDCALL LoadMessageLink( const std::string &szFile, const enum EMessageLink eLinkID  );

	virtual ILoadHelper * STDCALL GetLoadHelper( const int /*ELoadHelperID*/nLoadHelperID );

	virtual int STDCALL CustomCheck( const int nCustomCheckKey, const CCustomCheckParams &checkParams  );
	virtual void STDCALL CustomReaction( const std::string &szCustomReactionName );
	virtual void STDCALL SetWindowText( const int nElementID, const WORD *pszText );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __MESSAGEREACTIONINTERNAL_H__
