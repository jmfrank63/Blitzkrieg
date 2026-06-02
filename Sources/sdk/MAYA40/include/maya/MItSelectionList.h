#ifndef LINUX
#pragma once
#endif
#ifndef _MItSelectionList
#define _MItSelectionList

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MObject.h>
#include <maya/MSelectionList.h>
#include <maya/MStringArray.h>



class MDagPath;



/**
  Class for iterating over the items in an MSelection list.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MItSelectionList
{
public:
	enum selItemType{
		kUnknownItem = -1,
		kDagSelectionItem,
		kAnimSelectionItem,
		kDNselectionItem
	};
	bool		isDone( MStatus * ReturnStatus = NULL ); 
	MStatus		next(); 
	MStatus		reset();
	MStatus		getDependNode( MObject &depNode );
	MStatus		getDagPath( MDagPath &dagPath, MObject &component );
	MStatus     getDagPath( MDagPath &dagPath );
	MStatus     getStrings( MStringArray & array );
	selItemType itemType( MStatus * ReturnStatus = NULL ); 
	MStatus		setFilter( MFn::Type filter );
	bool		hasComponents( MStatus * ReturnStatus = NULL ) const;
	MItSelectionList( 	const MSelectionList & list, 
						MFn::Type = MFn::kInvalid,
						MStatus * ReturnStatus = NULL );
	virtual ~MItSelectionList();
protected:

private:
	static const char* 	className();
	MStatus				resetInner();
	void*				f_main_iter;
	void*				f_component_iter;
	void*				list_data;
	MFn::Type			f_filter;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MItSelectionList */
