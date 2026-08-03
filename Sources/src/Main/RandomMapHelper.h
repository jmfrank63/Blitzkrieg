#ifndef __RANDOMMAPHELPER_H__
#define __RANDOMMAPHELPER_H__
#pragma ONCE
#include "../Formats/fmtSaveLoad.h"
#include "../StreamIO/RandomGen.h"
void STDCALL StoreRandomMap( const std::string &szMissionName, NSaveLoad::SRandomHeader *pRndHdr, CPtr<IRandomGenSeed> *ppSeed );
void STDCALL RestoreRandomMap( const std::string &szMissionName, const NSaveLoad::SRandomHeader &rndhdr, interface IRandomGenSeed *pSeed );
#endif // __RANDOMMAPHELPER_H__
