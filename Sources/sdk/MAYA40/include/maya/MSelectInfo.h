#ifndef LINUX
#pragma once
#endif
#ifndef _MSelectInfo
#define _MSelectInfo
#if defined __cplusplus



#include <maya/MDrawInfo.h>



class MSelectionMask;
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

class OPENMAYAUI_EXPORT MSelectInfo : public MDrawInfo
{
public:
	MSelectInfo();
	MSelectInfo( const MSelectInfo& in );
	~MSelectInfo();

public:


    M3dView			view();

	bool			singleSelection() const;
	bool			selectClosest() const;
    bool			selectable( MSelectionMask & mask ) const;
    bool			selectableComponent( bool displayed, 
										 MSelectionMask & mask ) const;

    bool			isRay() const;

	MMatrix			getAlignmentMatrix() const;
    void			getLocalRay( MPoint&, MVector & ) const;

    bool			selectForHilite( const MSelectionMask & ) const;

    bool			selectOnHilitedOnly() const;

	int				highestPriority() const;
	void			setHighestPriority( int );

    void			addSelection( const MSelectionList &item,
								  const MPoint &point,
								  MSelectionList &list,
								  MPointArray &points,
								  const MSelectionMask & mask,
								  bool isComponent );

    MDagPath		selectPath() const;


protected:

private:
	const char*	 className() const;


    MSelectInfo( void* in );
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MSelectInfo */
