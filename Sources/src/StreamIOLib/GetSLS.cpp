#include "StdAfx.h"

#include "SaveLoadSystem.h"
ISaveLoadSystem *g_pGlobalSaveLoadSystem = 0;
ISaveLoadSystem* STDCALL GetSLS_Hook();
struct SGetSLSAutoMagic
{
	SGetSLSAutoMagic()
	{
		g_pGlobalSaveLoadSystem = GetSLS_Hook();
	}
};
static SGetSLSAutoMagic getslsinit;
