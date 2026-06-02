#ifndef LINUX
#pragma once
#endif
#ifndef _MBoundingBox
#define _MBoundingBox

#if defined __cplusplus



#include <maya/MTypes.h>
#include <maya/MPoint.h>

#ifdef _WIN32
#undef min
#undef max
#endif // _WIN32



class MMatrix;



/**
  This class implements a 3D bounding box
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MBoundingBox  
{
public:
    MBoundingBox();  
	MBoundingBox( const MBoundingBox & src );  
	MBoundingBox( const MPoint &corner1, const MPoint &corner2 );

	void	clear();

    void	transformUsing ( const MMatrix &matrix );
	void	expand( const MPoint & point );
	void	expand( const MBoundingBox & box );
	
	bool	contains( const MPoint & point ) const;
	double	width() const;
	double	height() const;
	double	depth() const;
	MPoint	center() const;
	MPoint	min() const;
	MPoint	max() const;

	MBoundingBox & operator=( const MBoundingBox & other );

protected:

private: 
	double data[6];
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MBoundingBox */
