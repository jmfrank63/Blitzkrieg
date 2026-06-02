#if !defined(__AUTO_RUN_DATA_FORMAT_CONFIGURATION__)
#define __AUTO_RUN_DATA_FORMAT_CONFIGURATION__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "IniFile.h"
class CARConfiguration
{
	CIniFile iniFile;
public:
	bool Load( const std::vector<BYTE> &rData );
	const CIniFile& Get() const { return iniFile; }
};
#endif // !defined(__AUTO_RUN_DATA_FORMAT_CONFIGURATION__)
