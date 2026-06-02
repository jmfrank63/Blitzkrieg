#ifndef __PARSER_H__
#define __PARSER_H__
#pragma once
#import "GOLD Parser Engine.dll"
using namespace GOLDParserEngine;
class CParser
{
	_GOLDParserPtr parser;
protected:
	bool Init( const char *pszGrammarFileName );
	virtual bool DoneParsing( _ReductionPtr reduction ) = 0;
	virtual bool ErrorLexical( const int nLineNumber ) = 0;
	virtual bool ErrorSyntax( const int nLineNumber ) = 0;
	virtual void ErrorComment( const int nLineNumber ) = 0;
	virtual void ErrorInternal( const int nLineNumber ) = 0;
	virtual void ErrorNotLoaded( const int nLineNumber ) = 0;
	_GOLDParserPtr GetParser() { return parser; }
public:
	CParser();
	bool Parse( const char *pszFileName );
};
#endif // __PARSER_H__
