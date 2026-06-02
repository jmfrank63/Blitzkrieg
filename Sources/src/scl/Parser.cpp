#include "StdAfx.h"

#include <comdef.h>

#include "Parser.h"
CParser::CParser()
: parser( "GOLDParserEngine.GOLDParser" )
{
}
bool CParser::Init( const char *pszGrammarFileName )
{
	VARIANT_BOOL bResult;
	{
		bstr_t bstr = pszGrammarFileName;
		bResult = parser->LoadCompiledGrammar( bstr );  // v3.0 API: pass _bstr_t directly
	}

	VARIANT_BOOL rdc = TRUE;
	parser->PutTrimReductions( &rdc );
	return bResult != 0;
}
bool CParser::Parse( const char *pszFileName )
{
	{
		bstr_t bstr = pszFileName;
		parser->OpenFile( bstr, _variant_t(false) );
	}
	bool bDone = false;
	short nReductionCount = 0;
	while ( !bDone ) 
	{
		GPMessageConstants response = parser->Parse();

		switch ( response ) 
		{
			case gpMsgLexicalError:
				if ( ErrorLexical(parser->CurrentLineNumber()) == false )
					return false;
				break;
			case gpMsgSyntaxError:
				if ( ErrorSyntax(parser->CurrentLineNumber()) == false )
					return false;
				break;
			case gpMsgReduction:
			{
				_ReductionPtr reduction = parser->GetCurrentReduction();
				reduction->PutTag( &nReductionCount );
				++nReductionCount;
				break;
			}
			case gpMsgAccept:
				bDone = true;
				break;
			case gpMsgCommentError:
				ErrorComment( parser->CurrentLineNumber() );
				return false;
			case gpMsgTokenRead:
				break;
			case gpMsgInternalError:
				ErrorInternal( parser->CurrentLineNumber() );
				return false;
			case gpMsgNotLoadedError:
				ErrorNotLoaded( parser->CurrentLineNumber() );
				return false;
		}

	}  //while
	
	return DoneParsing( parser->GetCurrentReduction() );
}
