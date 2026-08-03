
#include "StdAfx.h"
#include "CustomCheck.h"

#include "..\LuaLib\Script.h"

int CCheckRunScript::operator&( IStructureSaver &ss )
{
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
}

CCheckRunScript::CCheckRunScript( int TEST )
{
	szScriptFunction = "return ScriptCheckFunction()";
}
int CCheckRunScript::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Param", &szScriptFunction );
	return 0;
}
int CCheckRunScript::Check( interface IScreen *pScreen, class Script *pScript ) const
{
	NI_ASSERT_T( pScript != 0, NStr::Format( "CCheckRunScript function = \"%s\" but don't have script loaded", szScriptFunction.c_str() ) );
	if ( pScript )
	{
		const int oldtop = pScript->GetTop();
		pScript->DoString( szScriptFunction.c_str() );
		const int nNumRetArgs = pScript->GetTop();
		NI_ASSERT_T( nNumRetArgs == 1, NStr::Format( "script returned %d results instead of 1 required", nNumRetArgs ) );
		Script::Object obj = pScript->GetObject( 1 );
		NI_ASSERT_T( obj.IsNumber(), "returned not a number" );
		const int nResult = obj.GetNumber();
		pScript->SetTop( oldtop );
		return nResult;
	}
	return 0;
}