
#include "StdAfx.h"
#include "IMessageReaction.h"
#include "MessageReactions.h"

#include "CustomCheck.h"
#include "MessageReaction.h"

namespace NMessageReactionScript
{
int SetIGlobalVar( struct lua_State *state )
{
	Script script( state );
	
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "SetIGlobalVar: the first parameter is not a string", return 0 );
	NI_ASSERT_SLOW_TF( script.IsNumber( 2 ), "SetIGlobalVar: the second parameter is not a number", return 0 );

	SetGlobalVar( script.GetObject( 1 ), int(script.GetObject( 2 )) );

	return 0;
}
int SetFGlobalVar( struct lua_State *state )
{
	Script script( state );
	
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "SetFGlobalVar: the first parameter is not a string", return 0 );
	NI_ASSERT_SLOW_TF( script.IsNumber( 2 ), "SetFGlobalVar: the second parameter is not a number", return 0 );

	SetGlobalVar( script.GetObject( 1 ), float(script.GetObject( 2 )) );

	return 0;
}
int SetSGlobalVar( struct lua_State *state )
{
	Script script( state );
	
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "SetSGlobalVar: the first parameter is not a string", return 0 );
	NI_ASSERT_SLOW_TF( script.IsString( 2 ), "SetSGlobalVar: the second parameter is not a number", return 0 );

	SetGlobalVar( script.GetObject( 1 ), (const char *)(script.GetObject( 2 )) );

	return 0;
}
int GetIGlobalVar( struct lua_State *state )
{
	Script script( state );

	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "GetIGlobalVar: the first parameter is not a string", return 1 );
	NI_ASSERT_SLOW_TF( script.IsNumber( 2 ), "GetIGlobalVar: the second parameter is not a number", return 1 );

	script.PushNumber( GetGlobalVar( script.GetObject(1), int(script.GetObject(2)) ) );

	return 1;
}
int GetFGlobalVar( struct lua_State *state )
{
	Script script( state );

	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "GetFGlobalVar: the first parameter is not a string", return 1 );
	NI_ASSERT_SLOW_TF( script.IsString( 2 ), "GetFGlobalVar: the second parameter is not a number", return 1 );

	script.PushNumber( GetGlobalVar( script.GetObject(1), float(script.GetObject(2)) ) );

	return 1;
}
int GetSGlobalVar( struct lua_State *state )
{
	Script script( state );

	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "GetSGlobalVar: the first parameter is not a string", return 1 );
	NI_ASSERT_SLOW_TF( script.IsString( 2 ), "GetSGlobalVar: the second parameter is not a string", return 1 );

	script.PushString( GetGlobalVar( script.GetObject(1), (const char*)(script.GetObject(2)) ) );

	return 1;
}
int ScriptErrorOut( struct lua_State *state )
{
	Script script( state );
	Script::Object obj = script.GetObject(script.GetTop());
	const std::string szError = NStr::Format( "Script error: %s", obj.GetString() );
	GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, szError.c_str(), 0xffff0000, true );
	NStr::DebugTrace( "%s\n", szError.c_str() );
	return 0;
}
static int Sqrt( struct lua_State *pState )
{
	Script script( pState );
	const float fValue = script.GetObject(1).GetNumber();
	script.PushNumber( ::sqrt( double( fValue ) ) );
	return 1;
}
static int OutputStringValue( struct lua_State *state )
{
	Script script(state);
	NI_ASSERT_T( script.GetTop() == 2, "Script function must have 2 arguments on the stack" );			//два аргумента
	std::string szStr = script.GetObject( -2 );
	int nValue = script.GetObject( -1 );
	NStr::DebugTrace( "****Debug LUA script: %s %s\n", szStr.c_str(), nValue );
	return 0;
}
Script::SRegFunction reglist[] =
{
	{ "_ERRORMESSAGE"			,	ScriptErrorOut			},
	{ "OutputStringValue"	, OutputStringValue		},
	{ "SetIGlobalVar"			,	SetIGlobalVar				},
	{ "SetFGlobalVar"			,	SetFGlobalVar				},
	{ "SetSGlobalVar"			,	SetSGlobalVar				},
	{ "GetIGlobalVar"			,	GetIGlobalVar				},
	{ "GetFGlobalVar"			,	GetFGlobalVar				},
	{ "GetSGlobalVar"			,	GetSGlobalVar				},
	{ "GetSGlobalVar"			,	GetSGlobalVar				},
	{ 0, 0 },
};
};
int CMessageReactions::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Reactions", &reactions );
	if ( saver.IsReading() )
	{
		std::string szScript;
		saver.Add( "Script", &szScript );
		
		bScriptPresent = false;
		if ( !szScript.empty() )
		{
			CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szScript.c_str(), STREAM_ACCESS_READ );
			NI_ASSERT_T( pStream != 0, NStr::Format( "Can't find script file \"Scenarios\\Scripts\\player_skills_recalc.lua\"" ) );
		
			script.Register( NMessageReactionScript::reglist );
			const int nSize = pStream->GetSize();
			std::vector<char> buffer( nSize + 10 );
			pStream->Read( &(buffer[0]), nSize );
			if ( script.DoBuffer( &(buffer[0]), nSize, "Script" ) == 0 ) 
			{
				bScriptPresent = true;
			}
		}
	}
	return 0;
}
int CMessageReactions::operator&( IStructureSaver &ss )
{
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
}
CMessageReactions::CMessageReactions( int TEST )
{
	reactions["ReactionKey1"] = new CMessageReactionB2(1);	
}
void CMessageReactions::Load( const std::string &szResourceName )
{

	/*
	{
		CPtr<IDataStream> pStream = CreateFileStream( "c:\\a7\\Data\\test_message_reaction.xml" , STREAM_ACCESS_WRITE );
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::WRITE );
		this->operator&( *pDT );
	}
	*/

	reactions.clear();

	CPtr<IDataStream> pStream;
	pStream = GetSingleton<IDataStorage>()->OpenStream( (szResourceName + ".xml").c_str(), STREAM_ACCESS_READ );

	NI_ASSERT_T( pStream != 0, NStr::Format( "cannot open stream \"%s\"", szResourceName.c_str() ));	
	if ( pStream )
	{
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::READ );
			
		this->operator&( *pDT );
	}
}
void CMessageReactions::Execute( const std::string &szReactionKey, interface IScreen *pScreen )
{
	CReactions::iterator it = reactions.find( szReactionKey );
	NI_ASSERT_T( it != reactions.end(), NStr::Format( "unregistered reaction \"%s\"", szReactionKey.c_str() ) );
	if ( it != reactions.end() )
		it->second->Execute( pScreen, bScriptPresent ? &script : 0 );
}
void CMessageReactions::Register( const std::string &szReactionKey, IMessageReactionB2 *pReaction )
{
	reactions[szReactionKey] = pReaction;
}