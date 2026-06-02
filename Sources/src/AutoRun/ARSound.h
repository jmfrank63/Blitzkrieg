#if !defined(__AUTO_RUN_DATA_FORMAT_SOUND__)
#define __AUTO_RUN_DATA_FORMAT_SOUND__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CARSound
{
	std::vector<BYTE> sound;
public:
	bool Load( const std::vector<BYTE> &rData );
	const std::vector<BYTE>& Get() const { return sound; }
};
#endif // !defined(__AUTO_RUN_DATA_FORMAT_SOUND__)
