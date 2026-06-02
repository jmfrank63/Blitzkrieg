#ifndef __SHADERPARSER_H__
#define __SHADERPARSER_H__
#pragma once
#include "Parser.h"
#include "..\Formats\fmtShader.h"
class CShaderParser : public CParser
{
	virtual bool ErrorLexical( const int nLineNumber );
	virtual bool ErrorSyntax( const int nLineNumber );
	virtual void ErrorComment( const int nLineNumber );
	virtual void ErrorInternal( const int nLineNumber );
	virtual void ErrorNotLoaded( const int nLineNumber );
	virtual bool DoneParsing( _ReductionPtr reduction );
	std::vector<STechnique> techniques;
public:
	CShaderParser();
	bool Init();
	bool Save( const char *pszFileName );
};
#endif // __SHADERPARSER_H__
