#ifndef LINUX
#pragma once
#endif
#ifndef _MPxGeometryData
#define _MPxGeometryData

#if defined __cplusplus




#include <maya/MPxData.h>



class MPxGeometryIterator;
class MObjectArray;
class MIntArray;



/**

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MPxGeometryData : public MPxData
{
public:
	MPxGeometryData();
	virtual ~MPxGeometryData();

	virtual MPxGeometryIterator* iterator( MObjectArray & componentList,
											MObject & component,
											bool useComponents);

	virtual MPxGeometryIterator* iterator( MObjectArray & componentList,
											MObject & component,
											bool useComponents,
											bool world) const;

	virtual bool	updateCompleteVertexGroup( MObject & component ) const;

	virtual bool	deleteComponent( const MObjectArray& compList );
	virtual bool	deleteComponentsFromGroups( const MObjectArray& compList,
												MIntArray& groupIdArray,
												MObjectArray& groupComponentArray );

	virtual void	smartCopy( const MPxGeometryData *srcGeom );
	virtual	void			copy( const MPxData& src ) = 0;

	virtual MTypeId         typeId() const = 0;
	virtual MString         name() const = 0;

protected:

private:
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxGeometryData */
