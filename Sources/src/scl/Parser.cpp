#include "StdAfx.h"

#include <fstream>
#include <iterator>

#include "Parser.h"

namespace
{
std::wstring ToWide( const std::string &text )
{
	return std::wstring( text.begin(), text.end() );
}

std::wstring BuildRuleText( const RuleStruct &rule, const SymbolTable *symbolTable )
{
	std::wstring text = symbolTable->symbols[rule.symbolIndex].name;
	text += L" ::= ";
	for ( size_t i = 0; i < rule.symbols.size(); ++i )
	{
		if ( i != 0 )
			text += L" ";
		text += symbolTable->symbols[rule.symbols[i]].name;
	}
	return text;
}

_ReductionPtr MakeReduction( Symbol *symbol, const RuleTable *ruleTable, const SymbolTable *symbolTable )
{
	return _ReductionPtr( std::make_shared<CParserReduction>( symbol, ruleTable, symbolTable ) );
}
}

CParserRule::CParserRule( int index, const wchar_t *ruleText )
: tableIndex( index )
, text( ruleText ? ruleText : L"" )
{
}

CParserToken::CParserToken( short symbolIndex, const wchar_t *symbolName, const SParserVariant &value )
: tableIndex( symbolIndex )
, name( symbolName ? symbolName : L"" )
, data( value )
, parentSymbol( std::make_shared<CParserSymbol>( symbolIndex ) )
, TableIndex( symbolIndex )
, Name( symbolName ? symbolName : L"" )
{
}

CParserReduction::CParserReduction( Symbol *symbol, const RuleTable *ruleTable, const SymbolTable *symbolTable )
: TokenCount( 0 )
{
	NonTerminal *nonTerminal = static_cast<NonTerminal *>( symbol );
	const int ruleIndex = nonTerminal->ruleIndex;
	const std::wstring ruleText = BuildRuleText( ruleTable->rules[ruleIndex], symbolTable );
	ParentRule = _RulePtr( std::make_shared<CParserRule>( ruleIndex, ruleText.c_str() ) );

	for ( size_t i = 0; i < nonTerminal->children.size(); ++i )
	{
		Symbol *child = nonTerminal->children[i];
		const short symbolIndex = static_cast<short>( child->symbolIndex );
		const wchar_t *symbolName = symbolTable->symbols[symbolIndex].name.c_str();

		if ( child->type == NON_TERMINAL )
			tokens.push_back( _TokenPtr( std::make_shared<CParserToken>( symbolIndex, symbolName, SParserVariant( MakeReduction( child, ruleTable, symbolTable ) ) ) ) );
		else
			tokens.push_back( _TokenPtr( std::make_shared<CParserToken>( symbolIndex, symbolName, SParserVariant( static_cast<Terminal *>( child )->image.c_str() ) ) ) );
	}

	TokenCount = static_cast<short>( tokens.size() );
}

CNativeGoldParser::CNativeGoldParser()
: currentLineNumber( 0 )
, trimReductions( true )
{
}

bool CNativeGoldParser::LoadCompiledGrammar( const char *grammarFileName )
{
	return grammar.load( const_cast<char *>( grammarFileName ) );
}

bool CNativeGoldParser::ParseFile( const char *sourceFileName )
{
	currentReduction = _ReductionPtr();
	currentToken = _TokenPtr();
	expectedTokens.clear();
	currentLineNumber = 0;
	rootSymbol.reset();

	std::ifstream input( sourceFileName, std::ios::binary );
	if ( !input )
		return false;

	std::string text( (std::istreambuf_iterator<char>( input )), std::istreambuf_iterator<char>() );
	DFA *dfa = grammar.getScanner();
	if ( !dfa->scan( const_cast<char *>( text.c_str() ) ) )
	{
		ErrorTable *errors = dfa->getErrors();
		if ( errors && !errors->errors.empty() )
		{
			GPError *error = errors->errors.front();
			currentLineNumber = error->line;
			currentToken = _TokenPtr( std::make_shared<CParserToken>( -1, L"(Error)", SParserVariant( error->lastTerminal.image.c_str() ) ) );
		}
		return false;
	}

	ErrorTable *scanErrors = dfa->getErrors();
	if ( scanErrors && !scanErrors->errors.empty() )
	{
		GPError *error = scanErrors->errors.front();
		currentLineNumber = error->line;
		currentToken = _TokenPtr( std::make_shared<CParserToken>( -1, L"(Error)", SParserVariant( error->lastTerminal.image.c_str() ) ) );
		return false;
	}

	LALR *lalr = grammar.getParser();
	Symbol *root = lalr->parse( dfa->getTokens(), trimReductions, true );
	ErrorTable *parseErrors = lalr->getErrors();
	if ( parseErrors && !parseErrors->errors.empty() )
	{
		GPError *error = parseErrors->errors.front();
		currentLineNumber = error->line;
		currentToken = _TokenPtr( std::make_shared<CParserToken>( static_cast<short>( error->lastTerminal.symbolIndex ), error->lastTerminal.symbol.c_str(), SParserVariant( error->lastTerminal.image.c_str() ) ) );

		for ( size_t i = 0; i < error->expected.size(); ++i )
			expectedTokens.push_back( _TokenPtr( std::make_shared<CParserToken>( -1, error->expected[i].c_str(), SParserVariant() ) ) );

		delete root;
		return false;
	}

	if ( root == 0 )
		return false;

	rootSymbol.reset( root );
	currentReduction = MakeReduction( rootSymbol.get(), grammar.getRuleTable(), grammar.getSymbolTable() );
	return true;
}

CParser::CParser()
{
}

bool CParser::Init( const char *pszGrammarFileName )
{
	const bool bResult = parser.LoadCompiledGrammar( pszGrammarFileName );
	parser.PutTrimReductions( true );
	return bResult;
}

bool CParser::Parse( const char *pszFileName )
{
	if ( !parser.ParseFile( pszFileName ) )
	{
		if ( parser.TokenCount() > 0 )
			return ErrorSyntax( parser.CurrentLineNumber() );
		if ( parser.CurrentToken() )
			return ErrorLexical( parser.CurrentLineNumber() );
		ErrorInternal( parser.CurrentLineNumber() );
		return false;
	}

	return DoneParsing( parser.GetCurrentReduction() );
}
