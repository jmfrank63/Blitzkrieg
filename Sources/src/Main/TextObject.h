#ifndef __TEXT_OBJECT_H__
#define __TEXT_OBJECT_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "TextSystem.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CTextString : public ITextString
{
	OBJECT_COMPLETE_METHODS( CTextString );
	SHARED_RESOURCE_METHODS( nRefData.a, "Text.String" );
	DECLARE_SERIALIZE;
	//
	std::wstring szString;
	bool bChanged;
public:
	void STDCALL SwapData( ISharedResource *pResource );
	const WORD* STDCALL GetString() const { return szString.c_str(); }
	const int STDCALL GetLength() const { return szString.size(); }
	void STDCALL SetText( const WORD *pszText ) { szString = pszText == 0 ? L"" : reinterpret_cast<const wchar_t*>(pszText); bChanged = true; }
	//
	bool STDCALL IsChanged() const { return bChanged; }
	void STDCALL ResetChanged() { bChanged = false; }
	// internal container clearing
	void STDCALL ClearInternalContainer() {  }
	bool STDCALL Load( const bool bPreLoad = false ) { return false; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CTextDialog : public ITextDialog
{
	OBJECT_COMPLETE_METHODS( CTextDialog );
	SHARED_RESOURCE_METHODS( nRefData.a, "Text.Dialog" );
	//
	std::wstring szString;
	bool bChanged;
public:
	void STDCALL SwapData( ISharedResource *pResource );
	const WORD* STDCALL GetString() const { return szString.c_str(); };
	const int STDCALL GetLength() const { return szString.size(); }
	void STDCALL SetText( const WORD *pszText );
	//
	bool STDCALL IsChanged() const { return bChanged; }
	void STDCALL ResetChanged() { bChanged = false; }
	// internal container clearing
	void STDCALL ClearInternalContainer() {  }
	bool STDCALL Load( const bool bPreLoad = false );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif		//__TEXT_OBJECT_H__
