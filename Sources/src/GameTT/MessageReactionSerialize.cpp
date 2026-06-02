#include "StdAfx.h"
#include "MessageReactionINternal.h"


int CMessageAtomReactionSetWindowTextFromGlobalVar::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &nWindowID );
	saver.Add( 2, &szTextKey );
	return 0;
}
int CMessageAtomReactionSetWindowText::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &nWindowID );
	saver.Add( 2, &szTextKey );
	return 0;
}
int CCustomMessageReaction::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	if ( saver.IsReading() )
	{
		Clear();
	}
	return 0;
}
int CMessageAtomReactionCustom::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szCustomReactionName );
	return 0;
}
int CMessageAtomReactionSetGlobalVar::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szVarName );
	saver.Add( 2, &szVarValue );
	return 0;
}
int CMessageAtomReactionRemoveGlobalVar::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szVarName );
	return 0;
}
int CMessageAtomReactionPause ::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &nPauseType );
	saver.Add( 2, &bPause );
	return 0;
}
int CMessageAtomReactionMessageToInput::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &nEventID );
	saver.Add( 2, &nParam );
	return 0;
}
int CMessageAtomReactionMessageToMainLoop::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &nCommandID );
	saver.Add( 2, &szParam );
	return 0;
}
int CMessageReaction::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &atomReactions );
	saver.Add( 2, &nCustomCheckType );
	saver.Add( 3, &customCheckParams );
	return 0;
}
int CMessageLink::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &messageReactions );
	return 0;
};
int CMessageLinkContainer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &messageLinks );
	saver.Add( 2, &pInterface );
	if ( saver.IsReading() )
		Init();

	return 0;
}
