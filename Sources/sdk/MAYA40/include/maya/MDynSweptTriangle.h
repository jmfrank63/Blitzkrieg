#ifndef LINUX
#pragma once
#endif
#ifndef _MDynSweptTriangle
#define _MDynSweptTriangle

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>



class MVector;
class MPoint;



/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAFX_EXPORT MDynSweptTriangle  
{
public:
					MDynSweptTriangle();
					~MDynSweptTriangle();

	MVector			vertex( int vertexId, double t = 1.0 );

	MVector			normal( double t = 1.0 );

	MVector 		normalToPoint(double x, double y, double z, double t = 1.0);

	MVector			uvPoint( int vertexId );

protected:

private:


	static const char* className();
	MDynSweptTriangle( void* );
	void * fData;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MDynSweptTriangle */
