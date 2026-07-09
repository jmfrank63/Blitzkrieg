#ifndef __PARSER_H__
#define __PARSER_H__
#pragma once

#include <memory>
#include <string>
#include <vector>

#include <comutil.h>

#include "..\..\sdk\cpp-gpengine\include\CGTFile.h"
#include "..\..\sdk\cpp-gpengine\include\DFA.h"
#include "..\..\sdk\cpp-gpengine\include\LALR.h"
#include "..\..\sdk\cpp-gpengine\include\NonTerminal.h"
#include "..\..\sdk\cpp-gpengine\include\Terminal.h"

class CParserReduction;
class CParserToken;
class CParserRule;
class CParserSymbol;

template <class T>
class CParserPtr
{
	std::shared_ptr<T> ptr;
public:
	CParserPtr() {}
	explicit CParserPtr( const std::shared_ptr<T> &p ) : ptr( p ) {}
	T *operator->() const { return ptr.get(); }
	operator bool() const { return ptr.get() != 0; }
	T *get() const { return ptr.get(); }
};

typedef CParserPtr<CParserReduction> _ReductionPtr;
typedef CParserPtr<CParserToken> _TokenPtr;
typedef CParserPtr<CParserRule> _RulePtr;
typedef CParserPtr<CParserSymbol> _SymbolPtr;

struct SParserVariant
{
	int vt;
	_ReductionPtr pdispVal;
	_bstr_t bstrVal;
	long lVal;

	SParserVariant() : vt( VT_EMPTY ), lVal( 0 ) {}
	explicit SParserVariant( const _ReductionPtr &reduction ) : vt( VT_DISPATCH ), pdispVal( reduction ), lVal( 0 ) {}
	explicit SParserVariant( const wchar_t *text ) : vt( VT_BSTR ), bstrVal( text ? text : L"" ), lVal( 0 ) {}
	explicit SParserVariant( long value ) : vt( VT_I4 ), lVal( value ) {}
	operator long() const { return lVal; }
	operator _bstr_t() const { return bstrVal; }
};

#ifdef variant_t
#undef variant_t
#endif
#define variant_t SParserVariant
#define bstr_t _bstr_t

class CParserRule
{
	int tableIndex;
	_bstr_t text;
public:
	CParserRule( int index, const wchar_t *ruleText );
	int GetTableIndex() const { return tableIndex; }
	_bstr_t GetText() const { return text; }
};

class CParserSymbol
{
public:
	short TableIndex;
	explicit CParserSymbol( short tableIndex ) : TableIndex( tableIndex ) {}
};

class CParserToken
{
	short tableIndex;
	_bstr_t name;
	SParserVariant data;
	_SymbolPtr parentSymbol;
public:
	CParserToken( short symbolIndex, const wchar_t *symbolName, const SParserVariant &value );
	short GetTableIndex() const { return tableIndex; }
	SParserVariant GetData() const { return data; }
	_bstr_t GetName() const { return name; }
	_SymbolPtr GetParentSymbol() const { return parentSymbol; }

	short TableIndex;
	_bstr_t Name;
};

class CParserReduction
{
	std::vector<_TokenPtr> tokens;
public:
	CParserReduction( Symbol *symbol, const RuleTable *ruleTable, const SymbolTable *symbolTable );
	_TokenPtr GetTokens( short index ) const { return tokens[index]; }
	_RulePtr GetParentRule() const { return ParentRule; }
	short GetTag() const { return 0; }

	short TokenCount;
	_RulePtr ParentRule;
};

class CNativeGoldParser
{
	CGTFile grammar;
	std::unique_ptr<Symbol> rootSymbol;
	_ReductionPtr currentReduction;
	_TokenPtr currentToken;
	std::vector<_TokenPtr> expectedTokens;
	int currentLineNumber;
	bool trimReductions;

public:
	CNativeGoldParser();

	bool LoadCompiledGrammar( const char *grammarFileName );
	void PutTrimReductions( bool trim ) { trimReductions = trim; }
	bool ParseFile( const char *sourceFileName );

	_ReductionPtr GetCurrentReduction() const { return currentReduction; }
	_TokenPtr CurrentToken() const { return currentToken; }
	int CurrentLineNumber() const { return currentLineNumber; }
	short TokenCount() const { return static_cast<short>( expectedTokens.size() ); }
	_TokenPtr Tokens( short index ) const { return expectedTokens[index]; }
};

typedef CNativeGoldParser *_GOLDParserPtr;

class CParser
{
	CNativeGoldParser parser;
protected:
	bool Init( const char *pszGrammarFileName );
	virtual bool DoneParsing( _ReductionPtr reduction ) = 0;
	virtual bool ErrorLexical( const int nLineNumber ) = 0;
	virtual bool ErrorSyntax( const int nLineNumber ) = 0;
	virtual void ErrorComment( const int nLineNumber ) = 0;
	virtual void ErrorInternal( const int nLineNumber ) = 0;
	virtual void ErrorNotLoaded( const int nLineNumber ) = 0;
	_GOLDParserPtr GetParser() { return &parser; }
public:
	CParser();
	bool Parse( const char *pszFileName );
};

#endif // __PARSER_H__
