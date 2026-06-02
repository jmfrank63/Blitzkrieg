#ifndef LINUX
#pragma once
#endif
#ifndef _MUiMessage
#define _MUiMessage

#if defined __cplusplus



#include <maya/MMessage.h>






/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MUiMessage : public MMessage
{ 
public:
	static MCallbackId	addUiDeletedCallback(
								const MString& uiName,
								void (*func)( void* clientData ),
								void * clientData = NULL,
								MStatus * ReturnStatus = NULL );

private: 
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32


#endif /* __cplusplus */
#endif /* _MUiMessage */
