#ifndef __VIDEOCHECK_H__
#define __VIDEOCHECK_H__
#pragma ONCE
namespace NVideoCheck
{
struct SVideoMemory
{
	struct SMemory
	{
		DWORD dwTotal;
		DWORD dwFree;
	};
	SMemory local;
	SMemory nonlocal;
	SMemory texture;
};
const wchar_t* STDCALL GetAPIName();
DWORD STDCALL GetAPIVersion();
bool STDCALL GetVideoMemory( SVideoMemory *pMemory );
};
#endif // __VIDEOCHECK_H__
