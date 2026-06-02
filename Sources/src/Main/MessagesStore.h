#ifndef __MESSAGES_STORE_H__
#define __MESSAGES_STORE_H__
#pragma ONCE
interface IMultiplayerMessage;
class CMessagesStore
{
	std::list< CPtr<IMultiplayerMessage> > messages;
	CPtr<IMultiplayerMessage> pTakenMessage;
public:
	CMessagesStore() { }

	void AddMessage( IMultiplayerMessage *pMessage );
	interface IMultiplayerMessage* GetMessage();
};
#endif // __MESSAGES_STORE_H__
