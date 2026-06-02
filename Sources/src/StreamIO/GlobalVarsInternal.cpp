#include "StdAfx.h"

#include "GlobalVarsInternal.h"
CGlobalVars2::CGlobalVars2()
{
}
IGlobalVarsIterator* CGlobalVars2::CreateIterator() const
{
	return new CGlobalVarsIterator( this, SVarAccepter<CGlobalVars2::CVar>() );
}
struct SGlobalVarSortCmp
{
	const bool operator()( const CGlobalVars2::CVarsMap::const_iterator &var1, const CGlobalVars2::CVarsMap::const_iterator &var2 ) const
	{
		return var1->first < var2->first;
	}
};
void SGlobalVarsSorter::Sort( std::list<CGlobalVars2::CVarsMap::const_iterator> &vals )
{
	vals.sort( SGlobalVarSortCmp() );
}
/*
CGlobalVarsIterator::CGlobalVarsIterator( CGlobalVars2 *pGV )
: CBase( pGV, SVarAccepter<CGlobalVars2::CVar>() )
{
}
*/
