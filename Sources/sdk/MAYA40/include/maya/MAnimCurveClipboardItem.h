#ifndef LINUX
#pragma once
#endif
#ifndef _MAnimCurveClipboardItem
#define _MAnimCurveClipboardItem


#if defined __cplusplus


#include <maya/MFnAnimCurve.h>
#include <maya/MObject.h>




/**
	This class provides a wrapper to the clipboard item used to hold
	on to cut/copy/paste information
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MAnimCurveClipboardItem
{
public:
					MAnimCurveClipboardItem();
					MAnimCurveClipboardItem( const MAnimCurveClipboardItem & );
					~MAnimCurveClipboardItem();

	const MObject		animCurve( MStatus * ReturnStatus = NULL ) const;
	MStatus				getAddressingInfo( unsigned &rowCount, 
										   unsigned &childCount,
										   unsigned &attrCount) const;
	const MString &		fullAttributeName( MStatus * ReturnStatus=NULL ) const;
	const MString &		leafAttributeName( MStatus * ReturnStatus=NULL ) const;
	const MString &		nodeName( MStatus * ReturnStatus=NULL ) const;
	MFnAnimCurve::AnimCurveType	animCurveType( MStatus
												* ReturnStatus=NULL ) const;
	MStatus				setAnimCurve( const MObject & curve );
	MStatus				setAddressingInfo( unsigned rowCount,
										   unsigned childCount,
										   unsigned attributeCount );
	MStatus				setNameInfo( const MString & nodeName, 
									 const MString & fullName, 
									 const MString & leafName );
	MAnimCurveClipboardItem &operator =  (const MAnimCurveClipboardItem &);
	bool	 				operator == (const MAnimCurveClipboardItem &) const;
	
protected:

private:
	void *							fCurve;
	MFnAnimCurve::AnimCurveType		fAnimCurveType;

	unsigned						fRowCount;
	unsigned						fChildCount;
	unsigned						fAttrCount;
	MString							fFullAttrName;
	MString							fLeafAttrName;
	MString							fNodeName;
	static const char*				className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MAnimCurveClipboardItem */
