#ifndef LINUX
#pragma once
#endif
#ifndef _MPxGeometryIterator
#define _MPxGeometryIterator

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MObject.h>


 
class MPxSurfaceShape;
class MPoint;
class MObjectArray;



/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MPxGeometryIterator
{
public:
	MPxGeometryIterator( void * userGeometry, MObjectArray & components );
	MPxGeometryIterator( void * userGeometry, MObject & components );
	virtual ~MPxGeometryIterator();


	virtual bool			isDone() const;
	virtual void			next();
	virtual void			reset();

	virtual void			component( MObject &component );

	virtual bool			hasPoints() const;
	virtual int				iteratorCount() const;
	virtual MPoint			point() const;
	virtual void			setPoint( const MPoint & ) const;
	virtual int				setPointGetNext( MPoint & );
	virtual int				index() const;


	virtual bool			hasNormals() const;

	virtual int				indexUnsimplified() const;


	int						currentPoint() const;
	void					setCurrentPoint( int );
	int						maxPoints() const;
	void					setMaxPoints( int );

		

	void					setObject( MPxSurfaceShape & );

	void*					geometry() const;
	
protected:
	  
private:
	static const char*	    className();


	void * instance;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxGeometryIterator */
