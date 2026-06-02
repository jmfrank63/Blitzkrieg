#if !defined(__AUTO_RUN_DATA_FORMAT_TEXT__)
#define __AUTO_RUN_DATA_FORMAT_TEXT__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CARText
{
	std::wstring wszText;
	CString szText;

public:
	bool Load( const std::vector<BYTE> &rData );
	void SetCodePage( int nCodePage );
	const CString& Get() const { return szText; }
};
#endif // !defined(__AUTO_RUN_DATA_FORMAT_TEXT__)
