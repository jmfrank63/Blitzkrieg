#include "StdAfx.h"

#include "Win32Random.h"
namespace NWin32Random
{
static unsigned int s_holdrand = 0;
void Seed( const int nSeed )
{
	s_holdrand = static_cast<unsigned int>( nSeed );
}
unsigned int Random() 
{ 
	return ( ((s_holdrand = s_holdrand * 214013u + 2531011u) >> 16) & 0x7fff );
}
};
