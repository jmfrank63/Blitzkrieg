#ifndef LINUX
#pragma once
#endif
#ifndef _MPxLocatorNode
#define _MPxLocatorNode

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MObject.h>
#include <maya/MPxNode.h>
#include <maya/M3dView.h>
#include <maya/MBoundingBox.h>
#include <maya/M3dView.h>


 
class MDagPath;



/**
  Create user defined locators.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MPxLocatorNode : public MPxNode  
{
public:
	MPxLocatorNode();
	virtual ~MPxLocatorNode();
	virtual MPxNode::Type type() const;


	virtual void draw( M3dView & view, const MDagPath & path, 
			    	   M3dView::DisplayStyle style, M3dView:: DisplayStatus );
	virtual bool isBounded() const;
	virtual MBoundingBox boundingBox() const; 


 
	unsigned                  color( M3dView::DisplayStatus displayStatus ); 
	MColor                    colorRGB( M3dView::DisplayStatus displayStatus );

	
	static MObject underWorldObject;
	static MObject localPosition;
		static MObject localPositionX;
		static MObject localPositionY;
		static MObject localPositionZ;
	static MObject worldPosition;
	    static MObject worldPositionX;
	    static MObject worldPositionY;
	    static MObject worldPositionZ;
	static MObject nodeBoundingBox;
	    static MObject nodeBoundingBoxMin;
	        static MObject nodeBoundingBoxMinX;
	        static MObject nodeBoundingBoxMinY;
	        static MObject nodeBoundingBoxMinZ;
	    static MObject nodeBoundingBoxMax;
	        static MObject nodeBoundingBoxMaxX;
	        static MObject nodeBoundingBoxMaxY;
	        static MObject nodeBoundingBoxMaxZ;
	    static MObject nodeBoundingBoxSize;
	        static MObject nodeBoundingBoxSizeX;
	        static MObject nodeBoundingBoxSizeY;
	        static MObject nodeBoundingBoxSizeZ;
	static MObject center;
	    static MObject boundingBoxCenterX;
	    static MObject boundingBoxCenterY;
	    static MObject boundingBoxCenterZ;
	static MObject matrix;
	static MObject inverseMatrix;
	static MObject worldMatrix;
	static MObject worldInverseMatrix;
	static MObject parentMatrix;
	static MObject parentInverseMatrix;
	static MObject visibility;
	static MObject intermediateObject;
	static MObject isTemplated;
	static MObject instObjGroups;
	    static MObject objectGroups;
	        static MObject objectGrpCompList;
	        static MObject objectGroupId;
	        static MObject objectGroupColor;
	static MObject useObjectColor;
	static MObject objectColor;

protected:
	  
private:
	static void				initialSetup();
	static const char*	    className();



};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxLocatorNode */
