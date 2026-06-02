#ifndef LINUX
#pragma once
#endif
#ifndef _MDynSweptLine
#define _MDynSweptLine

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>



class MVector;



/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAFX_EXPORT MDynSweptLine  
{
public:
					MDynSweptLine();
					~MDynSweptLine();

	MVector			vertex( int vertexId, double t = 1.0 );
	MVector			normal( double x, double y, double z, double t = 1.0 );
	MVector			tangent( double t = 1.0 );
	double			length( double t = 1.0 );

protected:

private:


	static const char* className();
	MDynSweptLine( void* );
	void * fData;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MDynSweptLine */
