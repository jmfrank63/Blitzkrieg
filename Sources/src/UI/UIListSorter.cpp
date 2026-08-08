#include "StdAfx.h"
#include "../Platform/LegacyText.h"
#include "UIListSorter.h"

#include <cwctype>

namespace
{
int CompareWideNoCase( const wchar_t *left, const wchar_t *right )
{
	while ( *left != 0 && *right != 0 )
	{
		const wchar_t foldedLeft = static_cast<wchar_t>( std::towlower( *left ) );
		const wchar_t foldedRight = static_cast<wchar_t>( std::towlower( *right ) );
		if ( foldedLeft != foldedRight )
			return foldedLeft < foldedRight ? -1 : 1;
		++left;
		++right;
	}
	return *left == *right ? 0 : ( *left == 0 ? -1 : 1 );
}
}

bool CUIListTextSorter::operator() ( int nSortColumn, const IUIListRow *pRow1, const IUIListRow *pRow2, const bool bForward ) const
{
	IUIElement *pElement = pRow1->GetElement( nSortColumn );
	std::wstring wsz1 = NPlatform::WideFromWordString(pElement->GetWindowText( 0 ));
	pElement = pRow2->GetElement( nSortColumn );
	std::wstring wsz2 = NPlatform::WideFromWordString(pElement->GetWindowText( 0 ));
	int nRes = CompareWideNoCase( wsz1.c_str(), wsz2.c_str() );
	if ( nRes == 0 )
	{
		return false;
	}
	else
		return (bForward ? nRes < 0 : nRes > 0);
}
bool CUIListNumberSorter::operator() ( int nSortColumn, const IUIListRow *pRow1, const IUIListRow *pRow2, const bool bForward ) const
{
	IUIElement *pElement = pRow1->GetElement( nSortColumn );
	std::wstring wsz1 = NPlatform::WideFromWordString(pElement->GetWindowText( 0 ));
	pElement = pRow2->GetElement( nSortColumn );
	std::wstring wsz2 = NPlatform::WideFromWordString(pElement->GetWindowText( 0 ));
	std::string sz1 = NStr::ToAscii( wsz1 );
	std::string sz2 = NStr::ToAscii( wsz2 );
	double d1 = atof( sz1.c_str() );
	double d2 = atof( sz2.c_str() );
	if ( d1 == d2 )
		return false;
	else
		return (bForward ? d1 > d2 : d1 < d2);
}
bool CUIListUserDataSorter::operator() ( int nSortColumn, const IUIListRow *pRow1, const IUIListRow *pRow2, const bool bForward ) const
{
	int n1 = pRow1->GetUserData();
	int n2 = pRow2->GetUserData();
	return (bForward ? n1 < n2 : n1 > n2 );
}
