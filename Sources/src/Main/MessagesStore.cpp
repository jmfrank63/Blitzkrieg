#include "stdafx.h"

#include "MessagesStore.h"
#include "Messages.h"
IMultiplayerMessage* CMessagesStore::GetMessage()
{
	if ( messages.empty() )
		return 0;
	else
	{
		pTakenMessage = messages.front();
		messages.pop_front();

		return pTakenMessage;
	}
}
void CMessagesStore::AddMessage( IMultiplayerMessage *pMessage )
{
	messages.push_back( pMessage );
}
