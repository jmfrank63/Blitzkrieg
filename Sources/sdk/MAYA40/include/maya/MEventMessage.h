#ifndef LINUX
#pragma once
#endif
#ifndef _MEventMessage
#define _MEventMessage

#if defined __cplusplus



#include <maya/MMessage.h>
#include <maya/MStringArray.h>






/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MEventMessage : public MMessage
{ 
public:
	static MCallbackId	addEventCallback(
								const MString& event,
								void (*func)( void* clientData ),
								void * clientData = NULL,
								MStatus * ReturnStatus = NULL );

	static MStatus		getEventNames( MStringArray & names );

private: 
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32


#endif /* __cplusplus */
#endif /* _MEventMessage */
