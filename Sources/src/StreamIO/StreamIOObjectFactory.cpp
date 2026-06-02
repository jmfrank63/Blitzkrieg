#include "StdAfx.h"

#include "StreamIOObjectFactory.h"
#include "StreamIOTypes.h"

#include "MemFileSystem.h"
#include "RandomGenInternal.h"
CStreamIOObjectFactory theStreamIOObjectFactory;
CStreamIOObjectFactory::CStreamIOObjectFactory()
{
	REGISTER_CLASS( this, STREAMIO_MEMORY_STREAM, CMemFileStream );
	REGISTER_CLASS( this, STREAMIO_RANDOM_GEN_SEED, CRandomGenSeed );
}
static SModuleDescriptor theModuleDescriptor( "StreamIO", STREAMIO_STREAMIO, 0x0100, &theStreamIOObjectFactory, 0 );
const SModuleDescriptor* STDCALL GetModuleDescriptor()
{
	return &theModuleDescriptor;
}
