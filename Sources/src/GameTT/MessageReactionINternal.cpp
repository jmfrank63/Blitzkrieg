#include "StdAfx.h"
#include "MessageReactionINternal.h"
#include "../Main/iMain.h"
#include "../Input/Input.h"
#include "../Misc/TypeConvertor.h"
#include "iMission.h"
#include "../UI/UIMessages.h"
#include "MissionInterfaceEscapeMenu.h"
#include "../Common/PauseGame.h"
#include "../Main/iMainCommands.h"
#include "../Common/World.h"
#include "../Main/TextSystem.h"
#include "../Common/InterfaceScreenBase.h"
#include "WorldClient.h"

bool CMessageAtomReactionRemoveGlobalVar::Execute() 
{  
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	NStr::DebugTrace( "\t\t RemoveGlabalVar \tvarName =\t\"%s\"\n", 
										szVarName.c_str() );
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	RemoveGlobalVar( szVarName.c_str() ); 
	return true;
}	
bool CMessageAtomReactionSetGlobalVar::Execute() 
{  
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	NStr::DebugTrace( "\t\t SetGlobalVar \tvarName =\t\"%s\", \tValue =\t\"%s\"\n", 
										szVarName.c_str(), 
										szVarValue.c_str() );
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	SetGlobalVar( szVarName.c_str(), szVarValue.c_str() ); 
	return true;
}
bool CMessageAtomReactionSetWindowTextFromGlobalVar::Execute() 
{ 
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	NStr::DebugTrace( "\t\t SetWindowTextFromGlobalVar \twnd =\t\"%s\"\t(%d), \ttext =\t\"%s\"\n", 
										szWindowID.c_str(), 
										nWindowID, 
										NStr::ToAscii( std::wstring( reinterpret_cast<const wchar_t*>( GetGlobalWVar( szTextKey.c_str(), reinterpret_cast<const WORD*>( L"" ) ) ) ) ).c_str() );
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	
	GetSingleton<IMessageLinkContainer>()->SetWindowText( nWindowID, GetGlobalWVar(szTextKey.c_str(), reinterpret_cast<const WORD*>( L"" )) );
	return true;
}
bool CMessageAtomReactionSetWindowText::Execute() 
{ 
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	NStr::DebugTrace( "\t\t SetWindowText \twnd =\t\"%s\"\t(%d), \ttext =\t\"%s\"\n", 
										szWindowID.c_str(), 
										nWindowID, 
										szTextKey.c_str() );
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	ITextManager * pTM = GetSingleton<ITextManager>();
	IText * pText = pTM->GetString( szTextKey.c_str() );
	if ( pText )
	{
		GetSingleton<IMessageLinkContainer>()->SetWindowText( nWindowID, pText->GetString() );
	}
	return true;
}
bool CMessageAtomReactionCustom::Execute()
{
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	NStr::DebugTrace( "\t\t ReactionCustom \tname =\t\"%s\"\n",
										szCustomReactionName.c_str() );
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	GetSingleton<IMessageLinkContainer>()->CustomReaction( szCustomReactionName );
	return true;
}
bool CMessageAtomReactionMessageToMainLoop::Execute() 
{ 
	#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	NStr::DebugTrace( "\t\t MessageToMainLoop \tmsg =\t\"%s\"\t(%d), \tparam =\t\"%s\"\n", 
										szCommandID.c_str(), 
										nCommandID, 
										szParam.c_str() );
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	GetSingleton<IMainLoop>()->Command( nCommandID, szParam.empty() ? 0 : szParam.c_str() ); 
	return true;
}
bool CMessageAtomReactionMessageToInput::Execute()
{
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	NStr::DebugTrace( "\t\t MessageToInput \tmsg =\t\"%s\"\t(0x%x), \tparam =\t\"%s\"\t(0x%x)\n", 
										szEventID.c_str(), 
										nEventID, 
										szParam.c_str(),
										nParam );
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	GetSingleton<IInput>()->AddMessage( SGameMessage( nEventID, nParam ) ); 
	return true;
}
bool CMessageAtomReactionPause::Execute() 
{ 
	if ( GetGlobalVar( "MultiplayerGame", 0 ) == 0 )
	{
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		NStr::DebugTrace( "\t\t ReactionPause \ttype =\t\"%s\"\t(%d), \tbPause = \t%d\n", 
											szPauseType.c_str(), 
											nPauseType, 
											bPause );
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		GetSingleton<IMainLoop>()->Pause( bPause, nPauseType );
	}
	return true;
}
bool CMessageReaction::Execute()
{
	int nCustomCheckReturn = 0;
	
	if ( nCustomCheckType )
		nCustomCheckReturn =  GetSingleton<IMessageLinkContainer>()->CustomCheck( nCustomCheckType, customCheckParams );

	CMessageSequences::iterator chosen = atomReactions.find( nCustomCheckReturn );
	if ( chosen == atomReactions.end() )
		chosen = atomReactions.find( ECCR_DEFAULT );

	NI_ASSERT_T( chosen != atomReactions.end(), NStr::Format( "unlnown custom check return %d", nCustomCheckReturn ) );

	bool bRes = true;
	CMessageSequences::iterator common = atomReactions.find(ECCR_COMMON);
	CMessageSequences::iterator commonAfter = atomReactions.find(ECCR_COMMON_AFTER);
	if ( common != atomReactions.end() )
	{
		NStr::DebugTrace( "\t\t\tCOMMON\n" );
		bRes &= Execute( &common->second );
	}
	if ( chosen != atomReactions.end() )
	{
		NStr::DebugTrace( "\t\t\tCustomCheck \t%d \n", nCustomCheckReturn );
		bRes &= Execute( &chosen->second );
	}
	if ( commonAfter != atomReactions.end() )
	{
		NStr::DebugTrace( "\t\t\tCOMMON_AFTER\n" );
		bRes &= Execute( &commonAfter->second );
	}
	return bRes;
}
bool CMessageReaction::Execute( CMessageSequence *pToExecute )
{
	if ( pToExecute->empty() ) 
		return false;
	for ( CMessageSequence::iterator reaction = pToExecute->begin(); reaction != pToExecute->end(); ++reaction )
	{
		if ( !(*reaction)->Execute() )
			return false;
	}
	return true;
}
CMessageReaction::CMessageReaction( const SMessageReactionForLoad &loaded, IMessageLinkContainer *pHelpers )
{
	ILoadHelper * pAtomReactionsLH =  pHelpers->GetLoadHelper( ELH_ATOM_REACTION_TYPE );
	ILoadHelper * pCustomCheckLH = pHelpers->GetLoadHelper( ELH_ATOM_CUSTOM_CHECK_KEY );
	ILoadHelper * pCustomCheckReturnLH = pHelpers->GetLoadHelper( ELH_ATOM_CUSTOM_CHECK_RETURN );

#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	szCustomCheckType = loaded.customCheck.first ;
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	nCustomCheckType = pCustomCheckLH->Get( loaded.customCheck.first );
	customCheckParams = loaded.customCheck.second;
	NI_ASSERT_T( customCheckParams.size() < 8, NStr::Format( "to many global vars to check %d eg. ECCR_COMMON will not work", customCheckParams.size() ) );

	for ( CAtomReactionSequencesForLoad::const_iterator it = loaded.atomReactions.begin(); it != loaded.atomReactions.end(); ++it )
	{
		const CAtomReactionsSequenceForLoad &loadedAtomSequence = it->second;
		const int nCustomCheckReturn = pCustomCheckReturnLH->Get( it->first );

		NI_ASSERT_T( !loadedAtomSequence.empty(), NStr::Format( "no atom reactions to perform for action \"%s\"", it->first.c_str() ) );
		
		atomReactions[nCustomCheckReturn].clear();

		for ( int nAtomReaction = 0; nAtomReaction < loadedAtomSequence.size(); ++nAtomReaction )
		{
			const SMessageAtomReactionForLoad &loadedAtom = loadedAtomSequence[nAtomReaction];

			const int nType = pAtomReactionsLH->Get( loadedAtom.szType );
			IMessageReaction * pReaction = 0;
			switch( nType )
			{
			case EMART_PAUSE_GAME:
				pReaction = new CMessageAtomReactionPause( loadedAtom, pHelpers );
				break;
			case EMART_MESSAGE_TO_INPUT:
				pReaction = new CMessageAtomReactionMessageToInput( loadedAtom, pHelpers );
				break;
			case EMART_MESSAGE_TO_MAINLOOP:
				pReaction = new CMessageAtomReactionMessageToMainLoop( loadedAtom, pHelpers  );
				break;
			case EMART_SET_GLOBAL_VAR:
				pReaction = new CMessageAtomReactionSetGlobalVar( loadedAtom );
				break;
			case EMART_REMOVE_GLOBAL_VAR:
				pReaction = new CMessageAtomReactionRemoveGlobalVar( loadedAtom );
				break;
			case EMART_CUSTOM_REACTION:
				pReaction = new CMessageAtomReactionCustom( loadedAtom );
				break;
			case EMART_NOP:
				pReaction = new CMessageAtomReactionNOP();
				break;
			case EMART_NOP_DONT_PROCESSED:
				break;
			case EMART_SET_TEXT_TO_WINDOW:
				pReaction = new CMessageAtomReactionSetWindowText( loadedAtom, pHelpers );
				break;
			case EMART_SET_TEXT_TO_WINDOW_FROM_GLOBALVAR:
				pReaction = new CMessageAtomReactionSetWindowTextFromGlobalVar( loadedAtom, pHelpers );
				break;
			default:
				NI_ASSERT_T( false, NStr::Format( "unknown atom reaction type =%d, gon from string \"%s\"", nType, loadedAtom.szType.c_str() ) );
			}
			if ( pReaction )
				atomReactions[nCustomCheckReturn].push_back( pReaction );
		}
	}
}
IMessageReaction* CMessageLink::Configure( const int nMessageID, const int nParam )
{
	CMessageReactions::iterator it = messageReactions.find( CIncomingMessage(nMessageID,nParam) );
	if ( it != messageReactions.end() )
	{
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		NStr::DebugTrace( "+-------------------------------------------------------+\n" );
		NStr::DebugTrace( "IncomingMess \t\"%s\"\t(0x%x), \tParam \t\"%s\"\t(0x%x)\n", 
											messagesForDebug[it->first.first].c_str(),
											it->first.first,
											parametersForDebug[it->first.second].c_str(),
											it->first.second );
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		return it->second;
	}
	return 0;
}	
void CMessageLink::Load( const std::string &szFileName, IMessageLinkContainer *pHelpers )
{
	ILoadHelper * pIncomingMessageIdLH = pHelpers->GetLoadHelper( ELH_INCOMING_MESSAGE_ID );
	ILoadHelper * pIncomingMessageNParam = pHelpers->GetLoadHelper( ELH_INCOMING_MESSAGE_NPARAM );

	std::vector<SMessageReactionForLoad> forLoad;

	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szFileName.c_str() , STREAM_ACCESS_READ );
	CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
	tree.Add( "Commands", &forLoad );

	for ( int i = 0; i < forLoad.size(); ++i )
	{
		CMessageReaction *pReaction = new CMessageReaction( forLoad[i], pHelpers  );
		
		const int nMessageID = pIncomingMessageIdLH->Get( forLoad[i].incomingMessage.first );
		const int nMessageNParam = pIncomingMessageNParam->Get( forLoad[i].incomingMessage.second );

#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		messagesForDebug[nMessageID] = forLoad[i].incomingMessage.first;
		parametersForDebug[nMessageNParam] = forLoad[i].incomingMessage.second;
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		messageReactions[CIncomingMessage(nMessageID,nMessageNParam)] = pReaction;
	}

	/*
	std::vector<SMessageReactionForLoad> forLoad;
	CPtr<IDataStream> pStream = CreateFileStream( "c:\\a7\\Data\\test.xml" , STREAM_ACCESS_WRITE );
	CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::WRITE );

	SMessageAtomReactionForLoad atomForLoad;
	atomForLoad.szParam1 = "Param1";
	atomForLoad.szParam2 = "Param2";
	atomForLoad.szType = "Type";

	SMessageReactionForLoad tmpReaction;
	tmpReaction.customCheck.first = "CustomCheck";
	tmpReaction.customCheck.second.push_back( "CustomCheckParam1" );
	tmpReaction.customCheck.second.push_back( "CustomCheckParam2" );
	tmpReaction.incomingMessage = SMessageReactionForLoad::SIncomingMessageForLoad( "IncomingMessageID", "MessageParam" );
	tmpReaction.atomReactions["SampleKey1"].push_back( atomForLoad );
	tmpReaction.atomReactions["SampleKey2"].push_back( atomForLoad );

	forLoad.push_back( tmpReaction );
	forLoad.push_back( tmpReaction );
	
	tree.Add( "Commands", &forLoad );
	*/
}
void CMessageLinkContainer::Clear()
{
	messageLinks.clear();
	loadHelpers.clear();
}
ILoadHelper * CMessageLinkContainer::GetLoadHelper( const int /*ELoadHelperID*/nLoadHelperID )
{
	CLoadHelpers::iterator it = loadHelpers.find( nLoadHelperID );
	NI_ASSERT_T( it != loadHelpers.end(), NStr::Format( "cannot find load helper %d", nLoadHelperID ) );
	return it->second;
}
IMessageLink * CMessageLinkContainer::GetMessageLink( const enum EMessageLink eLinkID )
{
	CMessageLinks::iterator it = messageLinks.find( eLinkID );
	NI_ASSERT_T( it != messageLinks.end(), NStr::Format( "unregistered messages link id = %d", eLinkID ) );
	return it->second;
}
void CMessageLinkContainer::RegisterMessageLink ( IMessageLink *pMessageLink, const enum EMessageLink eLinkID )
{
	NI_ASSERT_T( messageLinks.find( eLinkID ) == messageLinks.end(), NStr::Format( "warning, overrite link with id =%d", eLinkID ));
	messageLinks[ eLinkID ] = pMessageLink;
}
void CMessageLinkContainer::LoadMessageLink( const std::string &szFile, const enum EMessageLink eLinkID  )
{
	CMessageLink * pLink = new CMessageLink;
	pLink->Load( szFile, this );
	RegisterMessageLink( pLink, eLinkID );
}
void CMessageLinkContainer::Init()
{
	{
	CPtr<CLoadHelper> pMessageLH = new CLoadHelper;
	STRING_ENUM_ADD_PTR( pMessageLH, WCB_YOU_WIN )
	STRING_ENUM_ADD_PTR( pMessageLH, WCB_YOU_LOOSE )
	STRING_ENUM_ADD_PTR( pMessageLH, WCB_DRAW )
	STRING_ENUM_ADD_PTR( pMessageLH, MC_ENTER_CHAT_MODE )
	STRING_ENUM_ADD_PTR( pMessageLH, MC_LOCAL_PLAYER_OUT_OF_SYNC )
	STRING_ENUM_ADD_PTR( pMessageLH, MC_CHECK_ENABLE_WINDOW )
	STRING_ENUM_ADD_PTR( pMessageLH, MC_SHOW_ESCAPE_MENU )
	STRING_ENUM_ADD_PTR( pMessageLH, MC_SHOW_HELP_SCREEN )
	STRING_ENUM_ADD_PTR( pMessageLH, MC_SHOW_OBJECTIVES )
	STRING_ENUM_ADD_PTR( pMessageLH, UI_SET_STATE_MESSAGE						)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NEXT_STATE_MESSAGE						)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_ENABLE_WINDOW_FORCE )
	STRING_ENUM_ADD_PTR( pMessageLH, UI_DISABLE_WINDOW_FORCE )
	STRING_ENUM_ADD_PTR( pMessageLH, UI_SET_ANIMATION_TIME						)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_BLINK_WINDOW									)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_HIDE_WINDOW_FORCE						)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_SHOW_WINDOW_FORCE						)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_MODAL_FLAG_FORCE_REMOVE			)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_MODAL_FLAG_FORCE_SET					)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_STATE_CHANGED_MESSAGE	)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_POSITION_CHANGED			)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_WINDOW_CLICKED				)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_SELECTION_CHANGED			)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_BAR_EXPAND						)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_LIST_DOUBLE_CLICK			)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_ANIMATION_FINISHED		)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_EDIT_BOX_RETURN				)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_EDIT_BOX_ESCAPE				)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_EDIT_BOX_TEXT_CHANGED	)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_BREAK_MESSAGE_BOX						)
	STRING_ENUM_ADD_PTR( pMessageLH, MC_SHOW_SINGLE_OBJECTIVE )
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_SAVE										)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_LOAD										)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_OPTIONS								)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_HELP										)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_OBJECTIVES							)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_END_MISSION						)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_RETURN_TO_GAME					)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_WIN_RETURN_TO_GAME)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_SP_LOOSE_RETURN_TO_GAME			)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_MP_LOOSE_RETURN_TO_GAME				)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_SP_END_WIN_MISSION )
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_MP_END_WIN_MISSION)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_WIN_WIN_MISSION		)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_RESTART_MISSION				)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_EXIT_TO_MAINMENU				)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_EXIT_TO_WINDOWS				)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_EXIT_TO_ROOT_ESCAPE		)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_OBJECTIVES_BACK )
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_HELP_BACK )
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_SINGLE_OBJECTIVE_BACK )
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_WIN_BACK_TO_MISSION )
	STRING_ENUM_ADD_PTR( pMessageLH, MISSION_BUTTON_SINGLE_OBJECTIVE )
	STRING_ENUM_ADD_PTR( pMessageLH, CMD_QUIT_TO_MAINMENU )
	STRING_ENUM_ADD_PTR( pMessageLH, CMD_RESTART_MISSION )
	STRING_ENUM_ADD_PTR( pMessageLH, UI_ENABLE_WINDOW_FORCE)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_DISABLE_WINDOW_FORCE)
	STRING_ENUM_ADD_PTR( pMessageLH, MC_CHECK_ENABLE_WINDOW )
	STRING_ENUM_ADD_PTR( pMessageLH, MC_MP_LAG_STARTED )
	STRING_ENUM_ADD_PTR( pMessageLH, UI_HIDE_WINDOW_FORCE )
	STRING_ENUM_ADD_PTR( pMessageLH, UI_SHOW_WINDOW_FORCE )
	STRING_ENUM_ADD_PTR( pMessageLH, UI_MODAL_FLAG_FORCE_SET )
	STRING_ENUM_ADD_PTR( pMessageLH, UI_MODAL_FLAG_FORCE_REMOVE )
	STRING_ENUM_ADD_PTR( pMessageLH, MC_SHOW_ESCAPE_MENU )
	STRING_ENUM_ADD_PTR( pMessageLH, MC_SHOW_HELP_SCREEN )
	STRING_ENUM_ADD_PTR( pMessageLH, MC_SHOW_OBJECTIVES )
	STRING_ENUM_ADD_PTR( pMessageLH, UI_SET_STATE_MESSAGE						)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NEXT_STATE_MESSAGE						)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_SHOW_WINDOW									)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_ENABLE_WINDOW								)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_SET_MODAL_FLAG								)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_SET_ANIMATION_TIME						)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_BLINK_WINDOW									)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_HIDE_WINDOW_FORCE						)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_SHOW_WINDOW_FORCE						)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_MODAL_FLAG_FORCE_REMOVE			)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_MODAL_FLAG_FORCE_SET					)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_STATE_CHANGED_MESSAGE	)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_POSITION_CHANGED			)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_WINDOW_CLICKED				)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_SELECTION_CHANGED			)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_BAR_EXPAND						)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_LIST_DOUBLE_CLICK			)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_ANIMATION_FINISHED		)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_EDIT_BOX_RETURN				)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_EDIT_BOX_ESCAPE				)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_NOTIFY_EDIT_BOX_TEXT_CHANGED	)
	STRING_ENUM_ADD_PTR( pMessageLH, UI_BREAK_MESSAGE_BOX						)
	STRING_ENUM_ADD_PTR( pMessageLH, SP_ESCAPE_MENU_MAIN );
	STRING_ENUM_ADD_PTR( pMessageLH, MP_ESCAPE_MENU_MAIN );
	STRING_ENUM_ADD_PTR( pMessageLH, SP_END_MISSION												)
	STRING_ENUM_ADD_PTR( pMessageLH, MP_END_MISSION												)
	STRING_ENUM_ADD_PTR( pMessageLH, ESCAPE_WIN_MISSION										)
	STRING_ENUM_ADD_PTR( pMessageLH, SP_FAIL_MISSION												)
	STRING_ENUM_ADD_PTR( pMessageLH, MP_FAIL_MISSION												)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_SAVE										)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_LOAD										)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_OPTIONS									)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_HELP										)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_OBJECTIVES							)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_END_MISSION							)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_RETURN_TO_GAME					)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_SP_END_WIN_MISSION )
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_MP_END_WIN_MISSION)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_WIN_WIN_MISSION		)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_RESTART_MISSION					)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_EXIT_TO_MAINMENU				)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_EXIT_TO_WINDOWS					)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_ESCAPE_EXIT_TO_ROOT_ESCAPE			)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_WIN_RETURN_TO_GAME)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_SP_LOOSE_RETURN_TO_GAME			)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_MP_LOOSE_RETURN_TO_GAME				)
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_MP_TIMEOUT_CALL )
	STRING_ENUM_ADD_PTR( pMessageLH, BUTTON_MP_TIMEOUT_CANCEL )
	STRING_ENUM_ADD_PTR( pMessageLH, STATIC_MP_FAIL												)
	STRING_ENUM_ADD_PTR( pMessageLH, STATIC_SP_FAIL												)
	STRING_ENUM_ADD_PTR( pMessageLH, STATIC_SP_WIN )
	STRING_ENUM_ADD_PTR( pMessageLH, MISSION_HELP_DIALOG )
	STRING_ENUM_ADD_PTR( pMessageLH, MISSION_OBJECTIVES_DIALOG )
	STRING_ENUM_ADD_PTR( pMessageLH, MISSION_SINGLE_OBJECTIVE_DIALOG )
	STRING_ENUM_ADD_PTR( pMessageLH, MISSION_COMMAND_SAVE_MISSION		)
	STRING_ENUM_ADD_PTR( pMessageLH, MISSION_COMMAND_LOAD_MISSION		)
	STRING_ENUM_ADD_PTR( pMessageLH, MAIN_COMMAND_SAVE );
	STRING_ENUM_ADD_PTR( pMessageLH, MAIN_COMMAND_LOAD );
	STRING_ENUM_ADD_PTR( pMessageLH, MISSION_COMMAND_OPTIONSSETTINGS	)
	STRING_ENUM_ADD_PTR( pMessageLH, WCC_OBJECTIVES_CLOSED		)
	STRING_ENUM_ADD_PTR( pMessageLH, WCC_SHOW_LAST_OBJECTIVE	)
	STRING_ENUM_ADD_PTR( pMessageLH, WCC_HIDE_OBJECTVE_WINDOW )
	STRING_ENUM_ADD_PTR( pMessageLH, MC_MANY_PLAYER_OUT_OF_SYNC )

	loadHelpers[ELH_MESSAGE_TO_MAINLOOP_PARAM1] = pMessageLH;
	loadHelpers[ELH_INCOMING_MESSAGE_ID] = pMessageLH;
	loadHelpers[ELH_INCOMING_MESSAGE_NPARAM] = pMessageLH;
	loadHelpers[ELH_MESSAGE_TO_INPUT_PARAM1] = pMessageLH;
	loadHelpers[ELH_MESSAGE_TO_INPUT_PARAM2] = pMessageLH;
	loadHelpers[ELH_MESSAGE_TO_MAINLOOP_PARAM1] = pMessageLH;
	}

	{
	CPtr<CLoadHelper> pReactionTypesLH = new CLoadHelper;
	STRING_ENUM_ADD_PTR( pReactionTypesLH, EMART_PAUSE_GAME )
	STRING_ENUM_ADD_PTR( pReactionTypesLH, EMART_MESSAGE_TO_INPUT )
	STRING_ENUM_ADD_PTR( pReactionTypesLH, EMART_MESSAGE_TO_MAINLOOP )
	STRING_ENUM_ADD_PTR( pReactionTypesLH, EMART_SET_GLOBAL_VAR )
	STRING_ENUM_ADD_PTR( pReactionTypesLH, EMART_REMOVE_GLOBAL_VAR )
	STRING_ENUM_ADD_PTR( pReactionTypesLH, EMART_CUSTOM_REACTION )
	STRING_ENUM_ADD_PTR( pReactionTypesLH, EMART_NOP )
	STRING_ENUM_ADD_PTR( pReactionTypesLH, EMART_SET_TEXT_TO_WINDOW )
	STRING_ENUM_ADD_PTR( pReactionTypesLH, EMART_NOP_DONT_PROCESSED )
	STRING_ENUM_ADD_PTR( pReactionTypesLH, EMART_SET_TEXT_TO_WINDOW_FROM_GLOBALVAR )
	loadHelpers[ELH_ATOM_REACTION_TYPE] = pReactionTypesLH;
	}

	{
	CPtr<CLoadHelper> pCustomCheckKeyLH = new CLoadHelper;
	(*pCustomCheckKeyLH)[""] =  0;			// specific action (default)
	STRING_ENUM_ADD_PTR( pCustomCheckKeyLH, ECCT_BOOL_GLOBAL_VARS_ENUM )
	STRING_ENUM_ADD_PTR( pCustomCheckKeyLH, ECCT_BOOL_GLOBAL_VAR_FIRST )
	loadHelpers[ELH_ATOM_CUSTOM_CHECK_KEY] = pCustomCheckKeyLH;
	}

	{
	CPtr<CLoadHelper> pCustomCheckReturnLH = new CLoadHelper;
	STRING_ENUM_ADD_PTR( pCustomCheckReturnLH, ECCR_COMMON )
	STRING_ENUM_ADD_PTR( pCustomCheckReturnLH, ECCR_COMMON_AFTER )
	STRING_ENUM_ADD_PTR( pCustomCheckReturnLH, ECCR_DEFAULT );
	loadHelpers[ELH_ATOM_CUSTOM_CHECK_RETURN] = pCustomCheckReturnLH;
	}

	{
		CPtr<CLoadHelper> pPauseLH = new CLoadHelper;
		STRING_ENUM_ADD_PTR( pPauseLH, PAUSE_TYPE_NO_PAUSE )
		STRING_ENUM_ADD_PTR( pPauseLH, PAUSE_TYPE_USER_PAUSE					)
		STRING_ENUM_ADD_PTR( pPauseLH, PAUSE_TYPE_INACTIVE						)
		STRING_ENUM_ADD_PTR( pPauseLH, PAUSE_TYPE_PREMISSION					)
		STRING_ENUM_ADD_PTR( pPauseLH, PAUSE_TYPE_MENU								)
		STRING_ENUM_ADD_PTR( pPauseLH, PAUSE_TYPE_MP_NO_SEGMENT_DATA	)
		STRING_ENUM_ADD_PTR( pPauseLH, PAUSE_TYPE_MP_LAGG						)
		STRING_ENUM_ADD_PTR( pPauseLH, PAUSE_TYPE_MP_TIMEOUT				)	
		STRING_ENUM_ADD_PTR( pPauseLH, PAUSE_TYPE_MP_LOADING				)	
		STRING_ENUM_ADD_PTR( pPauseLH, PAUSE_TYPE_NO_CONTROL				)	
		loadHelpers[ELH_PAUSE_TYPE] = pPauseLH;
	}
	
	LoadMessageLink( "UI\\escapemenu\\EscapeMenuReactions.xml", EML_ESCAPE_MENU );
	customReactions.Init();
}
void CMessageLinkContainer::SetInterface( class CInterfaceScreenBase *_pInterface )
{
	pInterface = _pInterface;
}
bool CMessageLinkContainer::ProcessMessage( const SGameMessage &msg )
{
	IMessageReaction * pReaction = 0;
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	if ( msg.nEventID == MC_SHOW_ESCAPE_MENU )
	{
		Clear();
		Init();
		/*SetGlobalVar( "History.Playing", 1 );
		SetGlobalWVar( "Multiplayer.Side0.Name", L"GERMANS" );
		SetGlobalWVar( "Multiplayer.Side1.Name", L"USSR" );
		SetGlobalVar( "temp.Multiplayer.Win.Partyname", 0 );*/

	}
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	for ( CMessageLinks::iterator it = messageLinks.begin(); !pReaction && it != messageLinks.end(); ++it )
	{
		pReaction = it->second->Configure( msg.nEventID, msg.nParam );
	}
	if ( pReaction )
	{
		const bool bRes = pReaction->Execute();

#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		NStr::DebugTrace( "+-------------------------------------------------------+\n" );
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
		
		return bRes;
	}
	return false;
}
void CMessageLinkContainer::SetWindowText( const int nElementID, const WORD *pszText)
{
	pInterface->SetWindowText( nElementID, pszText );
}
void CMessageLinkContainer::CustomReaction( const std::string &szCustomReactionName )
{
	customReactions.LaunchReaction( szCustomReactionName, pInterface );
}
int CMessageLinkContainer::CustomCheck( const int nCustomCheckKey, const CCustomCheckParams &checkParams  )
{
	switch( nCustomCheckKey )
	{
	case ECCT_BOOL_GLOBAL_VARS_ENUM:
		{
			int nResult = 0;
			for ( int i = 0; i < checkParams.size(); ++i )
			{
				nResult |= ( GetGlobalVar( checkParams[i].c_str(), 0 ) << i );
			}
			return nResult;
		}
		break;
	case ECCT_BOOL_GLOBAL_VAR_FIRST:
		{
			int nResult = 0;
			for ( int i = 0; i < checkParams.size(); ++i )
			{
				if( GetGlobalVar( checkParams[i].c_str(), 0 ) != 0 )
					return i+1;
			}
			return 0;
		}
		break;
		
	}
	return 0;
}
