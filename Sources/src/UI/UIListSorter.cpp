#include "StdAfx.h"
#include "UIListSorter.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIListTextSorter::operator() ( int nSortColumn, const IUIListRow *pRow1, const IUIListRow *pRow2, const bool bForward ) const
{
	IUIElement *pElement = pRow1->GetElement( nSortColumn );
	std::wstring wsz1 = reinterpret_cast<const wchar_t*>(pElement->GetWindowText( 0 ));
	pElement = pRow2->GetElement( nSortColumn );
	std::wstring wsz2 = reinterpret_cast<const wchar_t*>(pElement->GetWindowText( 0 ));
	int nRes = _wcsicmp( wsz1.c_str(), wsz2.c_str() );
	if ( nRes == 0 )
	{
		return false;
	}
	else
		return (bForward ? nRes < 0 : nRes > 0);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIListNumberSorter::operator() ( int nSortColumn, const IUIListRow *pRow1, const IUIListRow *pRow2, const bool bForward ) const
{
	IUIElement *pElement = pRow1->GetElement( nSortColumn );
	std::wstring wsz1 = reinterpret_cast<const wchar_t*>(pElement->GetWindowText( 0 ));
	pElement = pRow2->GetElement( nSortColumn );
	std::wstring wsz2 = reinterpret_cast<const wchar_t*>(pElement->GetWindowText( 0 ));
	std::string sz1 = NStr::ToAscii( wsz1 );
	std::string sz2 = NStr::ToAscii( wsz2 );
	double d1 = atof( sz1.c_str() );
	double d2 = atof( sz2.c_str() );
	if ( d1 == d2 )
		return false;
	else
		return (bForward ? d1 > d2 : d1 < d2);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIListUserDataSorter::operator() ( int nSortColumn, const IUIListRow *pRow1, const IUIListRow *pRow2, const bool bForward ) const
{
	int n1 = pRow1->GetUserData();
	int n2 = pRow2->GetUserData();
	return (bForward ? n1 < n2 : n1 > n2 );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
