
#include "stdafx.h"

#include <objbase.h>

#include "ShaderParser.h"

class CAutoMagic
{
public:
	CAutoMagic()
	{
		CoInitialize( 0 );
	}
	~CAutoMagic()
	{
		CoUninitialize();
	}
};

static CAutoMagic automagicinit;

int main(int argc, char* argv[])
{
	CShaderParser parser;
	parser.Init();
	if ( !parser.Parse(argv[1]) )
		return 0xDEAD;
	if ( parser.Save(argv[2]) )
		return 0xDEAD;

	return 0;
}
