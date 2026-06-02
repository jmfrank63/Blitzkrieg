#ifndef LINUX
#pragma once
#endif
#ifndef _MAnimCurveChange
#define _MAnimCurveChange

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>





/**

Create Anim Curve change caches.

Undo Anim Curve changes which have been cached.

Redo Anim Curve changes which were previously undone.

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MAnimCurveChange  
{

public:
	MAnimCurveChange( MStatus * ReturnStatus = NULL );
	~MAnimCurveChange();
	MStatus undoIt();
	MStatus redoIt();

protected:

private:
	static const char* className();
 
 
	void*		 data; 
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MAnimCurveChange */
