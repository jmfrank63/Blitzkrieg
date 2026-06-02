#ifndef __A5EXPORTMODEL_H__
#define __A5EXPORTMODEL_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include <maya/MPxFileTranslator.h>
#include <maya/MDagPath.h>
#include <maya/MObjectArray.h>
#include <maya/MPlugArray.h>
#include <maya/MDagPathArray.h>
#include <maya/MIntArray.h>
#include <maya/MMatrix.h>
#include <maya/MPoint.h>
struct SAdditionalBone
{
	MDagPath bonePath; // dag path to trasnform of the bone
	std::vector<MPlug> plugs; // all weight plugs for this bone
	MIntArray nJoints; // corresponding joints
};
class CA7ExportModel : public MPxFileTranslator
{
	MObjectArray oHierarchy;
	MObjectArray oLocatorsHierarchy;
	MObjectArray oMeshes;
	MObject oAABB;
	MObjectArray oAABB_As, oAABB_Ds;
	MObjectArray oLocators;
	MObjectArray oAnimations;


	void ClearAll();

	MObject FindObjectInHierarchy( MString &szName );
	int FindIndexInHierarchy( MString &szName );
	MObject FindObjectInLocators( MString &szName );
	int FindIndexInLocators( MString &szName );

	void ProcessJoint( MObjectArray &hierarchy, std::list<SJoint> &joints, std::list<std::string> &szTopNodes, bool bLocators );
	MStatus CollectHierarchy();
	MStatus CollectMeshes();
	MStatus ProcessHierarchy();
	MStatus ProcessMeshes();
	MStatus ProcessAABB();
	MStatus ProcessAnimations();

	void AddChildrenNodes( SSkeletonFormat::SNodeFormat *pNode, const std::list<SJoint> &joints );

	void OutputMatrix( const char *pszText, MMatrix &mx );
	void OutputVector( const char *pszText, MVector &v );
	void OutputPoint( const char *pszText, MPoint &p );
public:
	CA7ExportModel() {  }
	virtual ~CA7ExportModel() {  }

	static void* creator();

	MStatus reader( const MFileObject& file, const MString& optionsString, FileAccessMode mode );
	MStatus writer( const MFileObject& file, const MString& optionsString, FileAccessMode mode );

	bool haveReadMethod() const { return false; }
	bool haveWriteMethod() const { return true; }

	bool canBeOpened() const { return false; }
	MFileKind identifyFile( const MFileObject& fileName, const char* buffer, short size ) const;
};
#endif