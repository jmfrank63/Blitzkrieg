#ifndef __MESSAGEREACTION_H__
#define __MESSAGEREACTION_H__
#pragma ONCE
#include "iMission.h"
interface IMessageReaction : public IRefCount
{
	virtual bool STDCALL Execute() = 0;
};
interface IMessageLink : public IRefCount
{
	virtual IMessageReaction*  STDCALL Configure( const int nMessageID, const int nParam )  = 0;
};
enum EMessageLink
{
	EML_ESCAPE_MENU,

};
interface ILoadHelper : public IRefCount
{
	virtual int STDCALL Get( const std::string &szLoaded ) = 0;
};
enum ELoadHelperID
{
	ELH_ATOM_REACTION_TYPE,								// Commands->Reactions->Type
	ELH_INCOMING_MESSAGE_ID,							// Commands->IncomingMessage->first
	ELH_INCOMING_MESSAGE_NPARAM,					// Commands->IncomingMessage->second
	ELH_MESSAGE_TO_INPUT_PARAM1,					// Commands->Reactions->Param1 (if type == EMART_MESSAGE_TO_INPUT )
	ELH_MESSAGE_TO_INPUT_PARAM2,						// Commands->Reactions->Param2 (if type == EMART_MESSAGE_TO_INPUT )
	ELH_MESSAGE_TO_MAINLOOP_PARAM1,				// Commands->Reactions->Param1 (if type == EMART_MESSAGE_TO_MAINLOOP )
	ELH_ATOM_CUSTOM_CHECK_KEY,						// Commands->CustomCheck
	ELH_ATOM_CUSTOM_CHECK_RETURN,					// Commands->CustomCheckReturn
	ELH_PAUSE_TYPE												// pause type
};
typedef std::vector<std::string> CCustomCheckParams;
interface IMessageLinkContainer : public IRefCount
{
	enum { tidTypeID = GAMETT_MESSAGELINK_CONTAINER };

	virtual void STDCALL SetInterface( class CInterfaceScreenBase * pInterface ) = 0;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg ) = 0;

	virtual void Init() = 0;
	virtual void Clear() = 0;
	virtual IMessageLink * STDCALL GetMessageLink( const enum EMessageLink eLinkID ) = 0;
	
	virtual void STDCALL RegisterMessageLink ( IMessageLink *pMessageLink, const enum EMessageLink eLinkID ) = 0;
	virtual void STDCALL LoadMessageLink( const std::string &szFile, const enum EMessageLink eLinkID  ) = 0;
	
	virtual ILoadHelper * STDCALL GetLoadHelper( const int /*ELoadHelperID*/nLoadHelperID ) = 0;
	
	virtual int STDCALL CustomCheck( const int nCustomCheckKey, const CCustomCheckParams &checkParams ) = 0;

	virtual void STDCALL CustomReaction( const std::string &szCustomReactionName ) = 0;
	
	virtual void STDCALL SetWindowText( const int nElementID, const WORD *pszText ) = 0;
};
#endif // __MESSAGEREACTION_H__
