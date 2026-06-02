#ifndef LINUX
#pragma once
#endif
#ifndef _MFnBase
#define _MFnBase

#if defined __cplusplus




#include <maya/MFn.h>
#include <maya/MStatus.h>
#include <maya/MTypes.h>



class MObject;
class MPtrBase;



/**

Defines the interface for the API RTTI and Maya Object attachment methods
common to all Function Set Classes.

Implements the Function Set compatibility test methods for all Function Sets.

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnBase  
{
public:
 	virtual				~MFnBase();
	virtual MFn::Type	type() const;
	bool				hasObj( MFn::Type ) const;
	bool				hasObj( const MObject & ) const;
	MObject				object( MStatus* ReturnStatus = NULL ) const;
 	virtual MStatus		setObject( MObject & object );
 	virtual MStatus		setObject( const MObject & object );

protected:
	MFnBase(); 
	virtual const char*	className() const;
	void				setPtr( MPtrBase* );
	void				setPtr( const MPtrBase* );
	void				setPtrNull();
	virtual bool		objectChanged( MFn::Type, MStatus * );
 	MPtrBase* 			f_ptr;	    // initialized/set with volatile
 	const MPtrBase*		f_constptr;	// initialized/set with volatile/const
private:
#ifndef _WIN32
	MFnBase & operator=( const MFnBase & ) const;
#endif
	MFnBase & operator=( const MFnBase & );
	MFnBase * operator& () const;
	MFnBase * operator& ();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32


#define declareMinimalMFn( MFnClass )							 	\
	public:														 	\
		virtual MFn::Type type() const;							 	\
		virtual ~MFnClass();									 	\
																 	\
	protected:													 	\
		virtual const char* className() const;					 	\
																 	\
	private:													 	\
		MFnClass & operator=( const MFnClass & ) const;			 	\
		MFnClass & operator=( const MFnClass & );				 	\
		MFnClass * operator& () const;							 	\
		MFnClass * operator& ()
#define declareMFn( MFnClass, MFnParentClass )					 	\
	declareMinimalMFn( MFnClass );								 	\
	public:	        											 	\
		MFnClass() {};											 	\
		MFnClass( MObject & object, MStatus * ReturnStatus = NULL );\
		MFnClass( const MObject & object, MStatus * ReturnStatus = NULL )

#endif /* __cplusplus */
#endif /* _MFnBase */
