#ifndef LINUX
#pragma once
#endif
#ifndef _MSceneMessage
#define _MSceneMessage

#if defined __cplusplus



#include <maya/MMessage.h>





/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MSceneMessage : public MMessage
{
public:
	enum Message {
		kSceneUpdate,
		kBeforeNew,
		kAfterNew,
		kBeforeImport,
		kAfterImport,
		kBeforeOpen,
		kAfterOpen,
		kBeforeExport,
		kAfterExport,
		kBeforeSave,
		kAfterSave,
        kBeforeReference,
        kAfterReference,
        kBeforeRemoveReference,
        kAfterRemoveReference,
		kBeforeImportReference,
		kAfterImportReference,
		kBeforeExportReference,
		kAfterExportReference,

		kBeforeSoftwareRender,
		kAfterSoftwareRender,
		kBeforeSoftwareFrameRender,
		kAfterSoftwareFrameRender,
		kSoftwareRenderInterrupted,

		kMayaInitialized,
		kMayaExiting,
   };

public:
	static MCallbackId	addCallback( Message, void (*func)( void* clientData ),
									 void * clientData = NULL,
									 MStatus * ReturnStatus = NULL );

private:
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32


#endif /* __cplusplus */
#endif /* _MSceneMessage */
