#ifndef LINUX
#pragma once
#endif
#ifndef _MMessage
#define _MMessage

#if defined __cplusplus



#include <maya/MStatus.h>



typedef unsigned int MCallbackId;

typedef struct MMessageNode {
    void*           fClientPtr;
	void*			fServerPtr;
    MCallbackId		fId;
} * MMessageNodePtr;



/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MMessage
{ 
public:	
    static MStatus  removeCallback( MCallbackId id );

protected:
    static void addNode( MMessageNodePtr node ); 
    static void removeNode( MMessageNodePtr node );

private:
	static const char* 		className();
	static MMessageNodePtr 	findNode( MCallbackId id );
	
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32


#endif /* __cplusplus */
#endif /* _MMessage */
