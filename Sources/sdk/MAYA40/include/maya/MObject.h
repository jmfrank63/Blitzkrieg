
#ifndef LINUX
#pragma once
#endif
#ifndef _MObject
#define _MObject

#if defined __cplusplus



#include <maya/MFn.h>
#include <maya/MTypes.h>
#include <stdio.h>



class MPtrBase;




/**

Determine the exact type (MFn::Type) of an MObject within Maya.

Determine if an Object exists.

Determine if an Object is compatible with a specific Function Set.

*/

class FND_EXPORT  MObject  
{
public:


	MObject();
	MObject( const MObject &other ); 
	~MObject();
	bool			hasFn( MFn::Type fs ) const;
	bool            isNull() const;
 
    MFn::Type		apiType() const;
	const char *    apiTypeStr() const;
 
	bool			operator == (const MObject &) const;
	bool			operator != (const MObject &) const;
	MObject &		operator =  (const MObject &);

	static MObject  kNullObj;

protected:

private:

	MPtrBase*       ptr;
	int             tp;
};

#endif /* __cplusplus */
#endif /* _MObject */
