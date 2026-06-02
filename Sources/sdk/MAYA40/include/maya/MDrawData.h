#ifndef LINUX
#pragma once
#endif
#ifndef _MDrawData
#define _MDrawData
#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MObject.h>
#include <maya/M3dView.h>



class MDagPath;
class MVector;



/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MDrawData  
{
public:
	MDrawData();
	MDrawData( const MDrawData& in );
	~MDrawData();

public:
	void *		geometry();
	
protected:

private:
	const char*	 className() const;
    friend class MMaterial;
    friend class MPxSurfaceShapeUI;
	friend class MDrawRequest;




    MDrawData( void* in );
	void*	 fDrawData;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MDrawData */
