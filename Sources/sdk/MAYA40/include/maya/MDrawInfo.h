#ifndef LINUX
#pragma once
#endif
#ifndef _MDrawInfo
#define _MDrawInfo
#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MObject.h>
#include <maya/M3dView.h>
#include <maya/MDrawRequest.h>



class MSelectionMask;
class MSelectionTypeSet;
class MPoint;
class MPointArray;
class MVector;
class MSelectionList;
class MMatrix;



/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MDrawInfo  
{
public:
	MDrawInfo();
	MDrawInfo( const MDrawInfo& in );
	~MDrawInfo();


	MDrawRequest			getPrototype(
								const MPxSurfaceShapeUI& drawHandler ) const;

	M3dView  				view() const;

	const MDagPath 			multiPath () const;

	const MMatrix 			projectionMatrix() const;

	const MMatrix 			inclusiveMatrix() const;

	M3dView::DisplayStyle	displayStyle() const;

	M3dView::DisplayStatus	displayStatus() const;

	bool					inSelect() const;
	bool					completelyInside() const;

	bool					canDrawComponent( bool isDisplayOn,
									const MSelectionMask & compMask ) const;

public:


protected:
	void*	 fData;

private:
	const char*	 className() const;


    MDrawInfo( void* in );
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MDrawInfo */
