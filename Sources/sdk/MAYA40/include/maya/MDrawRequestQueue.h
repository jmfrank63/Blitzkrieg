#ifndef LINUX
#pragma once
#endif
#ifndef _MDrawRequestQueue
#define _MDrawRequestQueue
#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MObject.h>
#include <maya/MDrawRequest.h>





/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MDrawRequestQueue  
{
public:
	MDrawRequestQueue();
	~MDrawRequestQueue();

public:
	bool			isEmpty() const;
	void			add( MDrawRequest & );
	MDrawRequest	remove();


protected:

private:
	const char*	 className() const;


    MDrawRequestQueue( void* in );
	void*	 fDrawRequestQueue;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MDrawRequestQueue */
