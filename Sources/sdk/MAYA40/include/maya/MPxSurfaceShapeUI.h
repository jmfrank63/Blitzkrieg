#ifndef LINUX
#pragma once
#endif
#ifndef _MPxSurfaceShapeUI
#define _MPxSurfaceShapeUI

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MObject.h>
#include <maya/MPxNode.h>
#include <maya/M3dView.h>
#include <maya/MBoundingBox.h>
#include <maya/M3dView.h>
#include <maya/MSelectInfo.h>
#include <maya/MDrawRequest.h>
#include <maya/MDrawRequestQueue.h>


 
class MSelectionList;
class MPointArray;
class MPxSurfaceShape;
class MDrawData;
class MMaterial;



/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MPxSurfaceShapeUI
{
public:
	MPxSurfaceShapeUI();
	virtual ~MPxSurfaceShapeUI();

	void					getDrawData( void * geom, MDrawData & );


	virtual void			getDrawRequests( const MDrawInfo &,
											 bool objectAndActiveOnly,
											 MDrawRequestQueue & requests );
	virtual void		    draw( const MDrawRequest &, M3dView & view ) const;
	virtual bool		    select( MSelectInfo &selectInfo,
							    	MSelectionList &selectionList,
							    	MPointArray &worldSpaceSelectPts ) const;


	MPxSurfaceShape*		surfaceShape() const;
	MMaterial 				material( MDagPath & path ) const;

protected:
	  
private:
	static const char*	    className();
	friend class MDrawRequest;
	friend class MDrawInfo;

	void * instance;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxSurfaceShapeUI */
