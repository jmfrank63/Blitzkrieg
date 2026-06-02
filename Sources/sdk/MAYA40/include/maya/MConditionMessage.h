#ifndef LINUX
#pragma once
#endif
#ifndef _MConditionMessage
#define _MConditionMessage

#if defined __cplusplus



#include <maya/MMessage.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>






/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MConditionMessage : public MMessage
{ 
public:
	static MCallbackId	addConditionCallback(
								const MString& condition,
								void (*func)( bool state,
											  void* clientData ),
								void * clientData = NULL,
								MStatus * ReturnStatus = NULL );

	static MStatus		getConditionNames( MStringArray & names );

	static bool			getConditionState( const MString& condition,
										   MStatus * ReturnStatus = NULL );

private: 
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32


#endif /* __cplusplus */
#endif /* _MConditionMessage */
