#ifndef LINUX
#pragma once
#endif
#ifndef _MModelMessage
#define _MModelMessage

#if defined __cplusplus



#include <maya/MMessage.h>





/**
*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MModelMessage : public MMessage
{
public:
	enum Message {
		kActiveListModified
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
#endif /* _MModelMessage */
