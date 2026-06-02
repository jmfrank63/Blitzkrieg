#ifndef LINUX
#pragma once
#endif
#ifndef _MPxManipContainer
#define _MPxManipContainer

#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MObject.h>
#include <maya/MPxNode.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MVector.h>
#include <maya/MTypeId.h>
#include <maya/MString.h>
#include <maya/M3dView.h>
#include <maya/MDagPath.h>
#include <maya/MObjectArray.h>



class MPxManipContainer;
class MManipData;

typedef MManipData
(MPxManipContainer::*plugToManipConversionCallback)(unsigned index);

typedef MManipData
(MPxManipContainer::*manipToPlugConversionCallback)(unsigned index);



/**
MPxManipContainer is the base class for user defined manipulator
containers.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MPxManipContainer : public MPxNode
{
public:
	enum baseType {
        kFreePointTriadManip,
		kDirectionManip,
		kDistanceManip,
		kPointOnCurveManip,
		kPointOnSurfaceManip,
		kDiscManip,
		kCircleSweepManip,
		kToggleManip,
		kStateManip,
		kCurveSegmentManip
    }; 

	MPxManipContainer();
	virtual ~MPxManipContainer();
	virtual MPxNode::Type type() const;
	static  MStatus		initialize();
	static MPxManipContainer * newManipulator(const MString &manipName,
											  MObject &manipObject,
											  MStatus *ReturnStatus = NULL);

	virtual void			draw(M3dView &view, 
								 const MDagPath &path, 
								 M3dView::DisplayStyle style, 
								 M3dView::DisplayStatus status);
	virtual MStatus			connectToDependNode(const MObject &dependNode);
   
	virtual MStatus			createChildren();
	
	MDagPath	addFreePointTriadManip		(const MString &manipName,
											 const MString &pointName);
	MDagPath	addDirectionManip			(const MString &manipName,
											 const MString &directionName);
	MDagPath	addDistanceManip			(const MString &manipName,
											 const MString &distanceName);
	MDagPath	addPointOnCurveManip		(const MString &manipName,
											 const MString &paramName);
	MDagPath	addPointOnSurfaceManip		(const MString &manipName,
											 const MString &paramName);
	MDagPath	addDiscManip				(const MString &manipName,
											 const MString &angleName);
	MDagPath	addCircleSweepManip			(const MString &manipName,
											 const MString &angleName);
	MDagPath	addToggleManip				(const MString &manipName,
											 const MString &toggleName);
	MDagPath	addStateManip				(const MString &manipName,
											 const MString &stateName);
	MDagPath	addCurveSegmentManip		(const MString &manipName,
											 const MString &startParamName,
											 const MString &endParamName);
	MStatus                 finishAddingManips();
    static MStatus          addToManipConnectTable(MTypeId &);
    static MStatus          removeFromManipConnectTable(MTypeId &);
	
	void addPlugToManipConversionCallback(
	         unsigned manipIndex,
			 plugToManipConversionCallback callback);
	unsigned addManipToPlugConversionCallback(
	             MPlug &plug,
				 manipToPlugConversionCallback callback);

	MStatus	getConverterManipValue(unsigned manipIndex, unsigned &value);
	MStatus	getConverterManipValue(unsigned manipIndex, double &value);
	MStatus	getConverterManipValue(unsigned manipIndex, double &x, double &y);
	MStatus	getConverterManipValue(unsigned manipIndex, MPoint &point);
	MStatus	getConverterManipValue(unsigned manipIndex, MVector &vector);
	MStatus	getConverterManipValue(unsigned manipIndex, MMatrix &matrix);
	MStatus	getConverterManipValue(unsigned manipIndex, 
								   MEulerRotation &rotation);
	MStatus	getConverterManipValue(unsigned manipIndex, 
								   MTransformationMatrix &xform);
	
	MStatus	getConverterPlugValue(unsigned plugIndex, double &value);
	MStatus	getConverterPlugValue(unsigned plugIndex, double &x, double &y);
	MStatus	getConverterPlugValue(unsigned plugIndex, MPoint &point);
	MStatus	getConverterPlugValue(unsigned plugIndex, MVector &vector);
	MStatus	getConverterPlugValue(unsigned plugIndex, MMatrix &matrix);
	MStatus	getConverterPlugValue(unsigned plugIndex, 
								  MEulerRotation &rotation);
	

protected:
	  
private:
    static const char *     className(); 
	friend class			MFnPlugin;


	void *					internalData;
};


#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32
#endif /* __cplusplus */
#endif /* _MPxManipContainer */
