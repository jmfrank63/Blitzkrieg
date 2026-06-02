
#include "stdafx.h"
#include "MessageReaction.h"


#include "CustomCheck.h"
#include "Window.h"
#include "IUIInternal.h"
BASIC_REGISTER_CLASS(IMessageReactionB2);
CMessageReactionB2::CMessageReactionB2( int TEST )
{
	pCheck = new CCheckRunScript(TEST);
	branches[0].push_back( new CARRemoveGlobalVar(TEST) );
	branches[0].push_back( new CARRemoveGlobalVar(TEST) );
	branches[1].push_back( new CARRemoveGlobalVar(TEST) );
	branches[1].push_back( new CARRemoveGlobalVar(TEST) );
}
bool CMessageReactionB2::Execute( interface IScreen *pScreen, class Script *pScript ) const
{
	bool bRes = true;
	if ( !commonBefore.empty() )
	{
		NStr::DebugTrace( "\t\t\tCOMMON_BEFORE\n" );
		bRes &= Execute( &commonBefore, pScreen, pScript );
	}

	int nCustomCheckReturn = 0;
	
	if ( pCheck )
	{
		nCustomCheckReturn = pCheck->Check( pScreen, pScript );

		CMessageSequences::const_iterator it = branches.find( nCustomCheckReturn );
		NStr::DebugTrace( "\t\t\tCustomCheck \t%d \n", nCustomCheckReturn );
		if ( !it->second.empty() )
			bRes &= Execute( &it->second, pScreen, pScript );
	}

	if ( !commonAfter.empty() )
	{
		NStr::DebugTrace( "\t\t\tCOMMON_AFTER\n" );
		bRes &= Execute( &commonAfter, pScreen, pScript );
	}
	return bRes;
}
int CMessageReactionB2::operator&( IStructureSaver &ss )
{
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
}
bool CMessageReactionB2::Execute( const CMessageSequence *pToExecute, interface IScreen *pScreen, class Script *pScript ) const
{
	if ( pToExecute->empty() ) 
		return false;
	for ( CMessageSequence::const_iterator reaction = pToExecute->begin(); reaction != pToExecute->end(); ++reaction )
	{
		if ( !(*reaction)->Execute( pScreen, pScript ) )
			return false;
	}
	return true;
}
bool CARSetGlobalVar::Execute( interface IScreen *pScreen, class Script *pScript ) const
{  
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	NStr::DebugTrace( "\t\t SetGlobalVar \tvarName =\t\"%s\", \tValue =\t\"%s\"\n", 
										szVarName.c_str(), 
										szVarValue.c_str() );
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	SetGlobalVar( szVarName.c_str(), szVarValue.c_str() ); 
	return true;
}
int CARSetGlobalVar::operator&( IStructureSaver &ss )
{
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
}
bool CARRemoveGlobalVar::Execute( interface IScreen *pScreen, class Script *pScript ) const
{  
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	NStr::DebugTrace( "\t\t RemoveGlabalVar \tvarName =\t\"%s\"\n", 
										szVarName.c_str() );
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	RemoveGlobalVar( szVarName.c_str() ); 
	return true;
}	
int CARRemoveGlobalVar::operator&( IStructureSaver &ss )
{
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
}
bool CARSendMessage::Execute( interface IScreen *pScreen, class Script *pScript ) const
{
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	NStr::DebugTrace( "\t\t SendMessage\tID =\t\"%s\tParam =\t%s\n", 
										szMessageID.c_str(), szParam.c_str() );
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	
	return dynamic_cast<CWindow*>( pScreen )->ProcessMessage( SBUIMessage(szMessageID, szParam, nParam) );
}
int CARSendMessage::operator&( IStructureSaver &ss )
{
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
}
