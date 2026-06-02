#ifndef LINUX
#pragma once
#endif
#ifndef _MPxGlBuffer
#define _MPxGlBuffer

#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTypes.h>

#if defined(__unix)
#include <GL/glx.h>
#endif



class MString;
class M3dView;




/**
  Create user defined (off-screen) GL buffers.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MPxGlBuffer
{
public:
	MPxGlBuffer();
	MPxGlBuffer( M3dView &view );
	virtual ~MPxGlBuffer();

#if defined(__unix)
	virtual	MStatus			open( short width, short height,
								  GLXContext shareCtx = NULL );

	virtual GLXDrawable	    drawable( MStatus * ReturnStatus = NULL );
	virtual	GLXContext      context( MStatus * ReturnStatus = NULL );
#endif // __unix

	virtual	MStatus			close();

protected:
	bool					hasColorIndex;
	bool					hasAlphaBuffer;
	bool					hasDepthBuffer;
	bool					hasAccumulationBuffer;

private:

	void   setData( void* );
	virtual const char*	className() const;
	void * 	data;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxGlBuffer */
