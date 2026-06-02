#ifndef LINUX
#pragma once
#endif
#ifndef _MLibrary
#define _MLibrary

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>





/**
 Initialize and cleanup routines for Maya running in library mode.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MLibrary  
{
public:
						MLibrary ();
	virtual				~MLibrary ();
	static MStatus		initialize (char* applicationName,
									bool viewLicense = false);
	static void			cleanup( int exitStatus = 0 );

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MLibrary */
