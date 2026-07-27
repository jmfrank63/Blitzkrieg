
#include "stdafx.h"
#include "MultiplayerCommandManagerInternal.h"
#include "WorldClient.h"
BASIC_REGISTER_CLASS(IMPToUICommandManager);
int CMPToUICommandManager::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	if ( saver.IsReading() )
		Clear();
	saver.Add( 1, &eConnectionType );

	return 0;
}
void CMPToUICommandManager::AddCommandToUI( const SToUICommand &cmd )
{
	commandsToUI.push_back( cmd );
}
void CMPToUICommandManager::AddNotificationFromUI( const SFromUINotification &notify )
{
	notificationsFromUI.push_back( notify );
}
bool CMPToUICommandManager::GetCommandToUI( SToUICommand *pCmd )
{
	if ( !pCmd || commandsToUI.empty() ) return false;
	*pCmd = *commandsToUI.begin();
	commandsToUI.pop_front();
	return true;
}
bool CMPToUICommandManager::GetNotificationFromUI( SFromUINotification *pNotify )
{
	if ( !pNotify || notificationsFromUI.empty() ) return false;
	*pNotify = *notificationsFromUI.begin();
	notificationsFromUI.pop_front();
	return true;
}
bool CMPToUICommandManager::PeekCommandToUI( SToUICommand *pCmd )
{
	if ( !pCmd || commandsToUI.empty() ) return false;
	*pCmd = *commandsToUI.begin();

	return true;
}
bool CMPToUICommandManager::PeekNotificationFromUI( SFromUINotification *pNotify )
{
	if ( !pNotify || notificationsFromUI.empty() ) return false;
	*pNotify = *notificationsFromUI.begin();

	return true;
}
void CMPToUICommandManager::InitUISide()
{
	notificationsFromUI.clear();
	chatMessagesFromUI.clear();
}
SChatMessage* CMPToUICommandManager::PeekChatMessageToUI()
{
	if ( chatMessagesToUI.empty() )
		return 0;
	else
	{
		pTakenMessage = chatMessagesToUI.front();
		return pTakenMessage;
	}
}
SChatMessage* CMPToUICommandManager::GetChatMessageFromUI()
{
	if ( chatMessagesFromUI.empty() )
		return 0;
	else
	{
		pTakenMessage = chatMessagesFromUI.front();
		chatMessagesFromUI.pop_front();
		return pTakenMessage;
	}
}
SChatMessage* CMPToUICommandManager::GetChatMessageToUI()
{
	if ( chatMessagesToUI.empty() )
		return 0;
	else
	{
		pTakenMessage = chatMessagesToUI.front();
		chatMessagesToUI.pop_front();
		return pTakenMessage;
	}
}
void CMPToUICommandManager::AddChatMessageToUI( SChatMessage *pMessage )
{
	chatMessagesToUI.push_back( pMessage );
}
void CMPToUICommandManager::AddChatMessageFromUI( SChatMessage *pMessage )
{
	chatMessagesFromUI.push_back( pMessage );
}
void CMPToUICommandManager::DelayedNotification( const SFromUINotification &notify )
{
	delayedNotificaion = notify;
}
void CMPToUICommandManager::SendDelayedNotification()
{
	NI_ASSERT_T( delayedNotificaion.eNotifyID != EUTMN_UNITIALIZED, "NO delayed notification, trying to send" );
	AddNotificationFromUI( delayedNotificaion );
	delayedNotificaion.Clear();
}
