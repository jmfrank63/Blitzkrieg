#include "StdAfx.h"

#include <comdef.h>

#include "Parser.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CParser::CParser()
: parser( "GOLDParserEngine.GOLDParser" )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CParser::Init( const char *pszGrammarFileName )
{
	// load grammar
	VARIANT_BOOL bResult;
	{
		bstr_t bstr = pszGrammarFileName;
		bResult = parser->LoadCompiledGrammar( bstr );  // v3.0 API: pass _bstr_t directly
	}

	VARIANT_BOOL rdc = TRUE;
	parser->PutTrimReductions( &rdc );
	//
	return bResult != 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CParser::Parse( const char *pszFileName )
{
	// open file to parse
	{
		bstr_t bstr = pszFileName;
		// v3.0 API: OpenFile takes _bstr_t and a variant for encoding detection
		// false = don't detect encoding from BOM, use default
		parser->OpenFile( bstr, _variant_t(false) );
	}
	//
	bool bDone = false;
	short nReductionCount = 0;
	// parse
	while ( !bDone ) 
	{
		GPMessageConstants response = parser->Parse();

		switch ( response ) 
		{
			case gpMsgLexicalError:
				// Place code here to handle a illegal or unrecognized token 
				// To recover, pop the token from the stack: Parser.PopInputToken
				if ( ErrorLexical(parser->CurrentLineNumber()) == false )
					return false;
				break;
			case gpMsgSyntaxError:
				// This is a syntax error: the source has produced a token that was
				// not expected by the LALR State Machine. The expected tokens are stored
				// into the Tokens() list. To recover, push one of the
				// expected tokens onto the parser's input queue (the first in this case):
				// You should limit the number of times this type of recovery can take place.
				if ( ErrorSyntax(parser->CurrentLineNumber()) == false )
					return false;
				// parser->PushInputToken( &(parser->Tokens(0)) );
				break;
			case gpMsgReduction:
			{
				// This message is returned when a rule was reduced by the parse engine.
				// The CurrentReduction property is assigned a Reduction object
				// containing the rule and its related tokens. You can reassign this
				// property to your own customized class. If this is not the case,
				// this message can be ignored and the Reduction object will be used
				// to store the parse tree.
				_ReductionPtr reduction = parser->GetCurrentReduction();
				reduction->PutTag( &nReductionCount );
				++nReductionCount;
				//parser.CurrentReduction = //Object you created to store the rule
				break;
			}
			case gpMsgAccept:
				// The program was accepted by the parsing engine
				bDone = true;
				break;
			case gpMsgCommentError:
				// The end of the input was reached while reading a comment.
				// This is caused by a comment that was not terminated
				ErrorComment( parser->CurrentLineNumber() );
				return false;
			case gpMsgTokenRead:
				// A token was read by the parser. The Token Object can be accessed through
				// the CurrentToken() property:  Parser.CurrentToken
				break;
			case gpMsgInternalError:
				// Something horrid happened inside the parser. You cannot recover
				ErrorInternal( parser->CurrentLineNumber() );
				return false;
			case gpMsgNotLoadedError:
				// Load the Compiled Grammar Table file first.
				ErrorNotLoaded( parser->CurrentLineNumber() );
				return false;
		}

	}  //while
	
	return DoneParsing( parser->GetCurrentReduction() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
