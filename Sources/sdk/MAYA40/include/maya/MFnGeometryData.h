#ifndef LINUX
#pragma once
#endif
#ifndef _MFnGeometryData
#define _MFnGeometryData

#if defined __cplusplus



#include <maya/MFnData.h>



class MMatrix;



#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

/**
*/
class OPENMAYA_EXPORT MFnGeometryData : public MFnData 
{

    declareMFn( MFnGeometryData, MFnData );

public:

    MStatus        setMatrix( const MMatrix & );
    MStatus        getMatrix ( MMatrix & ) const;
    bool           matrixIsIdentity( MStatus * ReturnStatus = NULL ) const;
    bool           matrixIsNotIdentity( MStatus * ReturnStatus = NULL ) const;
    
    bool           hasObjectGroup( unsigned id,
                                   MStatus* ReturnStatus = NULL  ) const;
    MStatus        addObjectGroup( unsigned );
    MStatus        removeObjectGroup( unsigned );
    MStatus        changeObjectGroupId( unsigned , unsigned );

    unsigned       objectGroupCount( MStatus * ReturnStatus = NULL ) const;
    unsigned       objectGroup( unsigned index,
                                MStatus* ReturnStatus = NULL ) const;

    MFn::Type      objectGroupType( unsigned,
                                    MStatus* ReturnStatus = NULL ) const;

    MObject        objectGroupComponent( unsigned,
                                         MStatus* ReturnStatus = NULL ) const;
    MStatus        setObjectGroupComponent( unsigned, MObject & );
    MStatus        addObjectGroupComponent( unsigned, MObject & );
    MStatus        removeObjectGroupComponent( unsigned, MObject & );

    MStatus        copyObjectGroups( MObject & inGeom );


protected:

private:
 
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnGeometryData */
